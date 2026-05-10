/**
 * @file src/platform/linux/pwgrab.cpp
 * @brief PipeWire portal-based screen capture for virtual displays.
 *
 * Uses xdg-desktop-portal ScreenCast D-Bus API to obtain a PipeWire
 * stream node, then connects via libpipewire to receive DMA-BUF or
 * SHM frames. This is the only capture method that can see KWin
 * virtual outputs (which have no DRM connector).
 */
#ifdef SUNSHINE_BUILD_PIPEWIRE

// standard includes
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <limits.h>
#include <map>
#include <mutex>
#include <poll.h>
#include <pwd.h>
#include <thread>
#include <unistd.h>
#include <vector>

// pipewire includes
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/debug/types.h>
#include <spa/param/video/raw.h>
#include <spa/utils/result.h>

// GLib / GDBus for portal communication
#include <gio/gio.h>

#ifdef SUNSHINE_BUILD_KWIN
#include <zkde-screencast-unstable-v1.h>
#endif

// local includes
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/video.h"
#include "vaapi.h"
#include "graphics.h"

using namespace std::literals;

namespace pw {

  // Portal D-Bus constants
  static constexpr auto PORTAL_BUS_NAME = "org.freedesktop.portal.Desktop";
  static constexpr auto PORTAL_OBJECT   = "/org/freedesktop/portal/desktop";
  static constexpr auto PORTAL_SCREENCAST = "org.freedesktop.portal.ScreenCast";
  static constexpr auto PORTAL_REQUEST  = "org.freedesktop.portal.Request";
  static constexpr uint32_t SOURCE_MONITOR = 1;
  static constexpr uint32_t SOURCE_WINDOW = 2;
  static constexpr uint32_t SOURCE_VIRTUAL = 4;
  static constexpr uint32_t CURSOR_HIDDEN = 1;
  static constexpr uint32_t CURSOR_EMBEDDED = 2;
  static constexpr uint32_t CURSOR_METADATA = 4;

  // Helper: unique request token
  static std::atomic<uint32_t> request_counter{0};

  static std::string make_token() {
    return "sonnenschein_" + std::to_string(++request_counter);
  }

  static std::string make_request_path(GDBusConnection *conn, const std::string &token) {
    auto name = g_dbus_connection_get_unique_name(conn);
    std::string sender(name ? name : "");
    // Replace dots and leading colon
    for (auto &c : sender) {
      if (c == '.' || c == ':') c = '_';
    }
    if (!sender.empty() && sender[0] == '_') sender = sender.substr(1);
    return "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
  }

#ifdef SUNSHINE_BUILD_KWIN
  /**
   * Direct KWin ScreenCast session.
   *
   * This bypasses xdg-desktop-portal and requests a PipeWire node directly
   * for the wl_output named by the virtual-display backend. KDE's portal
   * "Virtual Display" source is a separate XDG virtual monitor and has been
   * observed to force 1920x1080, so we must target the existing output.
   */
  struct kwin_session_t {
    struct output_t {
      wl_output *output = nullptr;
      std::string name;
      std::string description;
      int width = 0;
      int height = 0;
      int x = 0;
      int y = 0;
    };

    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    zkde_screencast_unstable_v1 *screencast = nullptr;
    zkde_screencast_stream_unstable_v1 *stream = nullptr;
    std::map<wl_output *, output_t> outputs;
    uint32_t node_id = PW_ID_ANY;
    int width = 0;
    int height = 0;
    bool ready = false;
    bool failed = false;
    std::string error;

    ~kwin_session_t() {
      if (stream) {
        zkde_screencast_stream_unstable_v1_close(stream);
        stream = nullptr;
      }
      if (screencast) {
        zkde_screencast_unstable_v1_destroy(screencast);
        screencast = nullptr;
      }
      for (auto &[output, _] : outputs) {
        wl_output_destroy(output);
      }
      outputs.clear();
      if (registry) {
        wl_registry_destroy(registry);
        registry = nullptr;
      }
      if (display) {
        wl_display_disconnect(display);
        display = nullptr;
      }
    }

    bool init() {
      setup_permission();

      const char *wl_name = std::getenv("WAYLAND_DISPLAY");
      if (!wl_name) {
        BOOST_LOG(warning) << "KWin direct capture: WAYLAND_DISPLAY is not set"sv;
        return false;
      }

      display = wl_display_connect(wl_name);
      if (!display) {
        BOOST_LOG(warning) << "KWin direct capture: couldn't connect to Wayland display "sv << wl_name;
        return false;
      }

      registry = wl_display_get_registry(display);
      wl_registry_add_listener(registry, &registry_listener, this);
      wl_display_roundtrip(display);
      wl_display_roundtrip(display);

      if (!screencast) {
        BOOST_LOG(warning) << "KWin direct capture: zkde_screencast_unstable_v1 not available; falling back to portal"sv;
        return false;
      }
      return true;
    }

    bool start(const std::string &target_output_name) {
      wl_output *target = nullptr;
      output_t *params = nullptr;
      
      // 1. Try exact match by name or description
      for (auto &[output, candidate] : outputs) {
        BOOST_LOG(info) << "KWin direct capture: found output '"sv << candidate.name
                        << "' (desc: '"sv << candidate.description << "') "sv
                        << candidate.width << "x"sv << candidate.height
                        << " at "sv << candidate.x << ","sv << candidate.y;
        if (candidate.name == target_output_name || candidate.description == target_output_name) {
          target = output;
          params = &candidate;
        }
      }

      // 2. If not found, try fallback: the first output starting with 'Virtual-'
      // or description containing 'Virtual'
      if (!target) {
        for (auto &[output, candidate] : outputs) {
          if (candidate.name.find("Virtual-") == 0 || candidate.description.find("Virtual") != std::string::npos) {
            BOOST_LOG(info) << "KWin direct capture: using fallback virtual output '"sv << candidate.name << "'";
            target = output;
            params = &candidate;
            break;
          }
        }
      }

      if (!target || !params) {
        BOOST_LOG(warning) << "KWin direct capture: target output '"sv
                           << target_output_name << "' not found; refusing portal VIRTUAL fallback"sv;
        return false;
      }

      stream = zkde_screencast_unstable_v1_stream_output(
        screencast,
        target,
        ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED);
      if (!stream) {
        BOOST_LOG(warning) << "KWin direct capture: stream_output returned null"sv;
        return false;
      }
      zkde_screencast_stream_unstable_v1_add_listener(stream, &stream_listener, this);

      if (!wait_for_stream()) {
        BOOST_LOG(warning) << "KWin direct capture: "sv << (error.empty() ? "timeout waiting for stream" : error);
        return false;
      }

      width = params->width;
      height = params->height;
      BOOST_LOG(info) << "KWin direct capture: streaming output '"sv
                      << params->name << "' "sv << width << "x"sv << height
                      << " node_id="sv << node_id;
      return node_id != PW_ID_ANY && width > 0 && height > 0;
    }

  private:
    static std::string env_string(const char *key) {
      const char *value = std::getenv(key);
      return value ? value : "";
    }

    static std::filesystem::path home_dir() {
      if (auto home = env_string("HOME"); !home.empty()) {
        return home;
      }
      if (auto *pw = getpwuid(geteuid())) {
        return pw->pw_dir;
      }
      return {};
    }

    static std::filesystem::path user_applications_dir() {
      if (auto data_home = env_string("XDG_DATA_HOME"); !data_home.empty()) {
        return std::filesystem::path(data_home) / "applications";
      }
      auto home = home_dir();
      if (home.empty()) {
        return {};
      }
      return home / ".local" / "share" / "applications";
    }

    static std::string executable_path() {
      std::array<char, PATH_MAX> path {};
      const auto len = readlink("/proc/self/exe", path.data(), path.size() - 1);
      if (len <= 0) {
        return {};
      }
      return {path.data(), static_cast<std::size_t>(len)};
    }

    static void setup_permission() {
      static bool initialized = false;
      if (initialized) {
        return;
      }
      initialized = true;

      if (env_string("KWIN_WAYLAND_NO_PERMISSION_CHECKS") == "1") {
        BOOST_LOG(info) << "KWin direct capture: permission checks disabled by environment"sv;
        return;
      }

      const auto applications = user_applications_dir();
      const auto exe = executable_path();
      if (applications.empty() || exe.empty()) {
        BOOST_LOG(warning) << "KWin direct capture: cannot prepare permission desktop file"sv;
        return;
      }

      std::error_code ec;
      std::filesystem::create_directories(applications, ec);
      if (ec) {
        BOOST_LOG(warning) << "KWin direct capture: failed to create "sv
                           << applications.string() << ": "sv << ec.message();
        return;
      }

      const auto path = applications / "sonnenschein-kwin-screencast.desktop";

      std::ofstream file(path);
      if (!file.is_open()) {
        BOOST_LOG(warning) << "KWin direct capture: failed to write permission file "sv << path.string();
        return;
      }

      file << "[Desktop Entry]\n"
           << "Exec=" << exe << "\n"
           << "X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1\n"
           << "Type=Application\n"
           << "Name=sonnenschein-kwin-wayland-permission\n"
           << "Comment=Sonnenschein KWin screencast permission\n"
           << "NoDisplay=true\n";
      file.close();

      BOOST_LOG(info) << "KWin direct capture: created permission file "sv
                      << path.string() << "; waiting for KWin to rescan"sv;
      std::this_thread::sleep_for(3s);
    }

    bool wait_for_stream() {
      auto deadline = std::chrono::steady_clock::now() + 5s;
      while (!ready && !failed && std::chrono::steady_clock::now() < deadline) {
        wl_display_flush(display);
        pollfd pfd {};
        pfd.fd = wl_display_get_fd(display);
        pfd.events = POLLIN;

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
          break;
        }

        if (poll(&pfd, 1, static_cast<int>(remaining.count())) > 0 && (pfd.revents & POLLIN)) {
          if (wl_display_dispatch(display) < 0) {
            error = "wl_display_dispatch failed";
            return false;
          }
        }
      }
      return ready && !failed;
    }

    static void on_registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
      auto *self = static_cast<kwin_session_t *>(data);
      if (!std::strcmp(interface, zkde_screencast_unstable_v1_interface.name)) {
        const auto bind_version = std::min(version, static_cast<uint32_t>(6));
        self->screencast = static_cast<zkde_screencast_unstable_v1 *>(
          wl_registry_bind(registry, name, &zkde_screencast_unstable_v1_interface, bind_version));
        BOOST_LOG(info) << "KWin direct capture: bound zkde_screencast_unstable_v1 version "sv << bind_version;
      } else if (!std::strcmp(interface, wl_output_interface.name)) {
        const auto bind_version = std::min(version, static_cast<uint32_t>(4));
        auto *output = static_cast<wl_output *>(
          wl_registry_bind(registry, name, &wl_output_interface, bind_version));
        self->outputs.emplace(output, output_t{output});
        wl_output_add_listener(output, &output_listener, self);
      }
    }

    static void on_registry_global_remove(void *, wl_registry *, uint32_t) {}

    static constexpr wl_registry_listener registry_listener {
      .global = on_registry_global,
      .global_remove = on_registry_global_remove,
    };

    static void on_output_geometry(void *data, wl_output *output, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *, const char *, int32_t) {
      auto *self = static_cast<kwin_session_t *>(data);
      auto &params = self->outputs.at(output);
      params.x = x;
      params.y = y;
    }

    static void on_output_mode(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t) {
      if (!(flags & WL_OUTPUT_MODE_CURRENT)) {
        return;
      }
      auto *self = static_cast<kwin_session_t *>(data);
      auto &params = self->outputs.at(output);
      params.width = width;
      params.height = height;
    }

    static void on_output_done(void *, wl_output *) {}
    static void on_output_scale(void *, wl_output *, int32_t) {}

    static void on_output_name(void *data, wl_output *output, const char *name) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->outputs.at(output).name = name ? name : "";
    }

    static void on_output_description(void *data, wl_output *output, const char *description) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->outputs.at(output).description = description ? description : "";
    }

    static constexpr wl_output_listener output_listener {
      .geometry = on_output_geometry,
      .mode = on_output_mode,
      .done = on_output_done,
      .scale = on_output_scale,
      .name = on_output_name,
      .description = on_output_description,
    };

    static void on_stream_closed(void *data, zkde_screencast_stream_unstable_v1 *) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->failed = true;
      self->error = "stream closed by KWin";
    }

    static void on_stream_created(void *data, zkde_screencast_stream_unstable_v1 *, uint32_t node) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->node_id = node;
      self->ready = true;
    }

    static void on_stream_failed(void *data, zkde_screencast_stream_unstable_v1 *, const char *message) {
      auto *self = static_cast<kwin_session_t *>(data);
      self->failed = true;
      self->error = message ? message : "stream_output failed";
    }

    static void on_stream_serial(void *, zkde_screencast_stream_unstable_v1 *, uint32_t, uint32_t) {}

    static constexpr zkde_screencast_stream_unstable_v1_listener stream_listener {
      .closed = on_stream_closed,
      .created = on_stream_created,
      .failed = on_stream_failed,
      .serial = on_stream_serial,
    };
  };
#endif

  /**
   * Portal session: manages D-Bus interaction with xdg-desktop-portal.
   */
  struct portal_session_t {
    GDBusConnection *conn = nullptr;
    std::string session_handle;
    std::string restore_token;
    uint32_t pw_node_id = 0;
    int pw_fd = -1;
    uint32_t available_source_types = 0;
    uint32_t available_cursor_modes = 0;
    bool ready = false;

    // Synchronization for async D-Bus calls
    std::mutex mtx;
    std::condition_variable cv;
    bool response_received = false;
    uint32_t response_code = 0;
    GVariant *response_results = nullptr;

    ~portal_session_t() {
      if (pw_fd >= 0) close(pw_fd);
      if (conn) g_object_unref(conn);
    }

    static void on_response(GDBusConnection *, const gchar *, const gchar *,
                            const gchar *, const gchar *, GVariant *params,
                            gpointer user_data) {
      auto *self = static_cast<portal_session_t *>(user_data);
      std::lock_guard lk(self->mtx);

      g_variant_get(params, "(u@a{sv})", &self->response_code, &self->response_results);
      self->response_received = true;
      self->cv.notify_all();
    }

    bool wait_response(guint signal_id, int timeout_ms = 10000) {
      response_received = false;
      response_code = 0;
      if (response_results) { g_variant_unref(response_results); response_results = nullptr; }

      // We must pump the GLib main context for D-Bus signal delivery.
      // g_dbus_signal callbacks are dispatched on the default main context,
      // so we iterate it in a polling loop until the response arrives.
      auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
      while (!response_received && std::chrono::steady_clock::now() < deadline) {
        g_main_context_iteration(nullptr, FALSE);  // non-blocking pump
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      g_dbus_connection_signal_unsubscribe(conn, signal_id);

      if (!response_received) {
        BOOST_LOG(error) << "Portal: D-Bus response timed out"sv;
        return false;
      }
      if (response_code != 0) {
        BOOST_LOG(error) << "Portal: request denied (code " << response_code << ")"sv;
        return false;
      }
      return true;
    }

    guint subscribe_response(const std::string &request_path) {
      return g_dbus_connection_signal_subscribe(
        conn, PORTAL_BUS_NAME, PORTAL_REQUEST, "Response",
        request_path.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_response, this, nullptr);
    }

    bool init() {
      GError *err = nullptr;
      conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
      if (!conn) {
        BOOST_LOG(error) << "Portal: can't connect to session bus: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        return false;
      }
      available_source_types = get_uint_property("AvailableSourceTypes");
      available_cursor_modes = get_uint_property("AvailableCursorModes");
      BOOST_LOG(info) << "Portal: available source types="sv << available_source_types
                      << " cursor modes="sv << available_cursor_modes;
      return true;
    }

    uint32_t get_uint_property(const char *name) {
      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, "org.freedesktop.DBus.Properties",
        "Get", g_variant_new("(ss)", PORTAL_SCREENCAST, name),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &err);
      if (!result) {
        BOOST_LOG(warning) << "Portal: failed to read property "sv << name << ": "sv
                           << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        return 0;
      }

      GVariant *value = nullptr;
      g_variant_get(result, "(@v)", &value);
      uint32_t out = 0;
      if (value) {
        GVariant *inner = g_variant_get_variant(value);
        if (inner && g_variant_is_of_type(inner, G_VARIANT_TYPE_UINT32)) {
          out = g_variant_get_uint32(inner);
        }
        if (inner) g_variant_unref(inner);
        g_variant_unref(value);
      }
      g_variant_unref(result);
      return out;
    }

    bool create_session() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
      g_variant_builder_add(opts, "{sv}", "session_handle_token", g_variant_new_string(make_token().c_str()));

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "CreateSession", g_variant_new("(a{sv})", opts), nullptr,
        G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: CreateSession failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      if (!wait_response(sig)) return false;

      // Extract session handle
      if (response_results) {
        GVariant *handle = g_variant_lookup_value(response_results, "session_handle", G_VARIANT_TYPE_STRING);
        if (handle) {
          session_handle = g_variant_get_string(handle, nullptr);
          g_variant_unref(handle);
          BOOST_LOG(info) << "Portal: session created: "sv << session_handle;
        }
      }
      return !session_handle.empty();
    }

    bool select_sources() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
      g_variant_builder_add(opts, "{sv}", "types", g_variant_new_uint32(SOURCE_MONITOR));
      g_variant_builder_add(opts, "{sv}", "multiple", g_variant_new_boolean(FALSE));
      g_variant_builder_add(opts, "{sv}", "persist_mode", g_variant_new_uint32(2)); // persistent
      if (available_cursor_modes & CURSOR_EMBEDDED) {
        g_variant_builder_add(opts, "{sv}", "cursor_mode", g_variant_new_uint32(CURSOR_EMBEDDED));
        BOOST_LOG(info) << "Portal: requesting embedded cursor mode"sv;
      } else if (available_cursor_modes & CURSOR_METADATA) {
        BOOST_LOG(warning) << "Portal: cursor metadata mode is available, but PipeWire cursor metadata is not implemented yet; cursor will stay hidden"sv;
      } else if (available_cursor_modes & CURSOR_HIDDEN) {
        BOOST_LOG(warning) << "Portal: only hidden cursor mode is available"sv;
      }
      if (!restore_token.empty()) {
        g_variant_builder_add(opts, "{sv}", "restore_token", g_variant_new_string(restore_token.c_str()));
      }

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "SelectSources", g_variant_new("(oa{sv})", session_handle.c_str(), opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: SelectSources failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      return wait_response(sig);
    }

    bool start() {
      auto token = make_token();
      auto req_path = make_request_path(conn, token);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(token.c_str()));

      GError *err = nullptr;
      auto result = g_dbus_connection_call_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "Start", g_variant_new("(osa{sv})", session_handle.c_str(), "", opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &err);
      g_variant_builder_unref(opts);

      if (!result) {
        BOOST_LOG(error) << "Portal: Start failed: "sv << (err ? err->message : "unknown");
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sig);
        return false;
      }
      g_variant_unref(result);

      if (!wait_response(sig)) return false;

      // Extract streams and restore_token
      if (response_results) {
        GVariant *token = g_variant_lookup_value(response_results, "restore_token", G_VARIANT_TYPE_STRING);
        if (token) {
          restore_token = g_variant_get_string(token, nullptr);
          g_variant_unref(token);
        }

        GVariant *streams = g_variant_lookup_value(response_results, "streams", nullptr);
        if (streams) {
          GVariantIter iter;
          g_variant_iter_init(&iter, streams);
          GVariant *stream_entry = nullptr;
          if ((stream_entry = g_variant_iter_next_value(&iter))) {
            GVariant *node_id_v = g_variant_get_child_value(stream_entry, 0);
            pw_node_id = g_variant_get_uint32(node_id_v);
            g_variant_unref(node_id_v);

            GVariant *props = g_variant_get_child_value(stream_entry, 1);
            log_stream_properties(props);
            g_variant_unref(props);

            g_variant_unref(stream_entry);
            BOOST_LOG(info) << "Portal: stream node_id="sv << pw_node_id;
          }
          g_variant_unref(streams);
        }
      }

      // Get PipeWire fd
      GError *fd_err = nullptr;
      GUnixFDList *fd_list = nullptr;
      auto fd_opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      auto fd_result = g_dbus_connection_call_with_unix_fd_list_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", session_handle.c_str(), fd_opts),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &fd_list, nullptr, &fd_err);
      g_variant_builder_unref(fd_opts);

      if (!fd_result) {
        BOOST_LOG(error) << "Portal: OpenPipeWireRemote failed: "sv << (fd_err ? fd_err->message : "unknown");
        if (fd_err) g_error_free(fd_err);
        return false;
      }

      if (!fd_list) {
        BOOST_LOG(error) << "Portal: OpenPipeWireRemote returned no fd list"sv;
        g_variant_unref(fd_result);
        return false;
      }

      gint32 fd_index = 0;
      g_variant_get(fd_result, "(h)", &fd_index);
      GError *get_fd_err = nullptr;
      pw_fd = g_unix_fd_list_get(fd_list, fd_index, &get_fd_err);
      g_variant_unref(fd_result);
      g_object_unref(fd_list);

      if (pw_fd < 0) {
        BOOST_LOG(error) << "Portal: failed to get PipeWire fd: "sv << (get_fd_err ? get_fd_err->message : "unknown");
        if (get_fd_err) g_error_free(get_fd_err);
        return false;
      }

      BOOST_LOG(info) << "Portal: PipeWire fd="sv << pw_fd << " node_id="sv << pw_node_id;
      ready = (pw_fd >= 0 && pw_node_id > 0);
      return ready;
    }

    static std::string source_type_name(uint32_t source_type) {
      switch (source_type) {
        case SOURCE_MONITOR:
          return "MONITOR";
        case SOURCE_WINDOW:
          return "WINDOW";
        case SOURCE_VIRTUAL:
          return "VIRTUAL";
        default:
          return "unknown(" + std::to_string(source_type) + ")";
      }
    }

    static void log_stream_properties(GVariant *props) {
      if (!props) return;

      uint32_t source_type = 0;
      if (g_variant_lookup(props, "source_type", "u", &source_type)) {
        BOOST_LOG(info) << "Portal: stream source_type="sv << source_type_name(source_type);
        if (source_type == SOURCE_VIRTUAL) {
          BOOST_LOG(warning) << "Portal: selected XDG VIRTUAL source; KDE currently tends to expose this as 1920x1080. Prefer the existing Sonnenschein monitor source if the dialog offers it."sv;
        }
      }

      const gchar *id = nullptr;
      if (g_variant_lookup(props, "id", "&s", &id)) {
        BOOST_LOG(info) << "Portal: stream id="sv << id;
      }

      const gchar *mapping_id = nullptr;
      if (g_variant_lookup(props, "mapping_id", "&s", &mapping_id)) {
        BOOST_LOG(info) << "Portal: stream mapping_id="sv << mapping_id;
      }

      gint32 x = 0, y = 0;
      if (g_variant_lookup(props, "position", "(ii)", &x, &y)) {
        BOOST_LOG(info) << "Portal: stream position="sv << x << ","sv << y;
      }

      gint32 width = 0, height = 0;
      if (g_variant_lookup(props, "size", "(ii)", &width, &height)) {
        BOOST_LOG(info) << "Portal: stream logical size="sv << width << "x"sv << height;
      }

      guint64 serial = 0;
      if (g_variant_lookup(props, "pipewire-serial", "t", &serial)) {
        BOOST_LOG(info) << "Portal: stream pipewire-serial="sv << serial;
      }
    }
  };

  /**
   * PipeWire stream capture: connects to PipeWire and receives frames.
   */
  struct pw_capture_t {
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_stream *stream = nullptr;

    uint32_t width = 0, height = 0;
    uint32_t stride = 0;
    uint32_t format = 0; // SPA format
    uint32_t requested_width = 1920;
    uint32_t requested_height = 1080;
    uint32_t requested_framerate = 60;
    bool is_dmabuf = false;

    std::mutex frame_mtx;
    std::condition_variable frame_cv;
    bool has_new_frame = false;

    // Latest frame data (SHM path)
    std::vector<uint8_t> frame_data;

    // Latest frame DMA-BUF info
    struct dmabuf_frame_t {
      int fd = -1;
      uint32_t width = 0, height = 0;
      uint32_t stride = 0, offset = 0;
      uint32_t drm_format = 0;
    };
    dmabuf_frame_t dmabuf_frame;

    static void on_process(void *userdata) {
      auto *self = static_cast<pw_capture_t *>(userdata);
      auto *buf = pw_stream_dequeue_buffer(self->stream);
      if (!buf) return;

      auto *d = &buf->buffer->datas[0];

      if (d->type == SPA_DATA_DmaBuf) {
        std::lock_guard lk(self->frame_mtx);
        self->dmabuf_frame.fd = d->fd;
        self->dmabuf_frame.width = self->width;
        self->dmabuf_frame.height = self->height;
        self->dmabuf_frame.stride = d->chunk->stride;
        self->dmabuf_frame.offset = d->chunk->offset;
        self->is_dmabuf = true;
        self->has_new_frame = true;
        self->frame_cv.notify_all();
      }
      else if (d->type == SPA_DATA_MemFd || d->type == SPA_DATA_MemPtr) {
        auto *src = static_cast<uint8_t *>(d->data);
        if (src && d->chunk->size > 0) {
          std::lock_guard lk(self->frame_mtx);
          self->frame_data.resize(d->chunk->size);
          std::memcpy(self->frame_data.data(), src + d->chunk->offset, d->chunk->size);
          self->stride = d->chunk->stride;
          self->is_dmabuf = false;
          self->has_new_frame = true;
          self->frame_cv.notify_all();
        }
      }

      pw_stream_queue_buffer(self->stream, buf);
    }

    static void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param) {
      auto *self = static_cast<pw_capture_t *>(userdata);
      if (!param || id != SPA_PARAM_Format) return;

      struct spa_video_info vid_info;
      if (spa_format_video_raw_parse(param, &vid_info.info.raw) < 0) return;

      self->width = vid_info.info.raw.size.width;
      self->height = vid_info.info.raw.size.height;
      self->format = vid_info.info.raw.format;

      BOOST_LOG(info) << "PipeWire: format negotiated "sv
                      << self->width << "x"sv << self->height
                      << " fmt="sv << self->format;
      if (self->width != self->requested_width || self->height != self->requested_height) {
        BOOST_LOG(warning) << "PipeWire: negotiated size differs from client request "
                           << self->requested_width << "x"sv << self->requested_height
                           << " -> "sv << self->width << "x"sv << self->height;
      }
    }

    static void on_state_changed(void *userdata, enum pw_stream_state old,
                                  enum pw_stream_state state, const char *error) {
      BOOST_LOG(info) << "PipeWire: stream state "sv
                      << pw_stream_state_as_string(old) << " -> "sv
                      << pw_stream_state_as_string(state)
                      << (error ? std::string(" (") + error + ")" : "");
    }

    static const pw_stream_events stream_events;

    bool init(int fd, uint32_t node_id, uint32_t req_width, uint32_t req_height, uint32_t req_framerate) {
      requested_width = req_width ? req_width : 1920;
      requested_height = req_height ? req_height : 1080;
      requested_framerate = req_framerate ? req_framerate : 60;

      pw_init(nullptr, nullptr);

      loop = pw_thread_loop_new("sonnenschein-pw", nullptr);
      if (!loop) {
        BOOST_LOG(error) << "PipeWire: failed to create thread loop"sv;
        return false;
      }

      context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
      if (!context) {
        BOOST_LOG(error) << "PipeWire: failed to create context"sv;
        return false;
      }

      pw_thread_loop_lock(loop);
      pw_thread_loop_start(loop);

      if (fd >= 0) {
        core = pw_context_connect_fd(context, fcntl(fd, F_DUPFD_CLOEXEC, 3), nullptr, 0);
      } else {
        core = pw_context_connect(context, nullptr, 0);
      }
      if (!core) {
        BOOST_LOG(error) << "PipeWire: failed to connect to "sv << (fd >= 0 ? "remote" : "local core");
        pw_thread_loop_unlock(loop);
        return false;
      }

      auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        nullptr);

      stream = pw_stream_new(core, "sonnenschein-capture", props);
      if (!stream) {
        BOOST_LOG(error) << "PipeWire: failed to create stream"sv;
        pw_thread_loop_unlock(loop);
        return false;
      }

      pw_stream_add_listener(stream, &stream_listener, &stream_events, this);

      // Build format params — prefer DMA-BUF, fallback to SHM
      uint8_t params_buf[1024];
      struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buf, sizeof(params_buf));

      struct spa_rectangle default_size = SPA_RECTANGLE(requested_width, requested_height);
      struct spa_rectangle min_size = SPA_RECTANGLE(1, 1);
      struct spa_rectangle max_size = SPA_RECTANGLE(8192, 8192);
      struct spa_fraction default_rate = SPA_FRACTION(requested_framerate, 1);
      struct spa_fraction min_rate = SPA_FRACTION(0, 1);
      struct spa_fraction max_rate = SPA_FRACTION(std::max<uint32_t>(requested_framerate, 144), 1);

      BOOST_LOG(info) << "PipeWire: requesting format "sv
                      << requested_width << "x"sv << requested_height
                      << "@"sv << requested_framerate;

      auto *fmt = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,    SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3,
          SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx),
        SPA_FORMAT_VIDEO_size,   SPA_POD_CHOICE_RANGE_Rectangle(
          &default_size, &min_size, &max_size),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
          &default_rate, &min_rate, &max_rate)
      ));

      const struct spa_pod *param_list[] = { fmt };

      pw_stream_connect(stream, PW_DIRECTION_INPUT, node_id,
        static_cast<pw_stream_flags>(
          PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        param_list, 1);

      pw_thread_loop_unlock(loop);

      BOOST_LOG(info) << "PipeWire: stream connecting to node "sv << node_id;
      return true;
    }

    void cleanup() {
      if (loop) {
        pw_thread_loop_lock(loop);
        if (stream) { pw_stream_destroy(stream); stream = nullptr; }
        if (core) { pw_core_disconnect(core); core = nullptr; }
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        if (context) { pw_context_destroy(context); context = nullptr; }
        pw_thread_loop_destroy(loop);
        loop = nullptr;
      }
      pw_deinit();
    }

    ~pw_capture_t() { cleanup(); }

    spa_hook stream_listener {};
  };

  const pw_stream_events pw_capture_t::stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = pw_capture_t::on_state_changed,
    .param_changed = pw_capture_t::on_param_changed,
    .process = pw_capture_t::on_process,
  };

  // ---- platf::display_t implementation ----

  struct img_t : public platf::img_t {
    ~img_t() override {
      delete[] data;
      data = nullptr;
    }
  };

  class pw_display_t : public platf::display_t {
  public:
    int init(platf::mem_type_e hwdevice_type, const std::string &display_name, const ::video::config_t &config) {
      mem_type = hwdevice_type;
      delay = std::chrono::nanoseconds{1s} / config.framerate;

      if (::video::is_encoder_probing_active) {
        // This is an encoder probe (boot-time or stream pre-flight).
        // We don't want to pop up a Portal dialog just to probe encoders.
        // Skip Portal init, set dummy resolution.
        width = config.width ? config.width : 1920;
        height = config.height ? config.height : 1080;
        env_width = width;
        env_height = height;
        BOOST_LOG(info) << "PipeWire: Skipping portal init for encoder probe (dummy mode)"sv;
        return 0;
      }

      int pipewire_fd = -1;
      uint32_t pipewire_node_id = PW_ID_ANY;

#ifdef SUNSHINE_BUILD_KWIN
      // Prefer KWin's direct screencast protocol for named Sonnenschein
      // virtual outputs. The KDE portal "Virtual Display" source is not
      // the same output and has been observed to negotiate 1920x1080.
      if (!display_name.empty()) {
        kwin_direct = std::make_unique<kwin_session_t>();
        if (kwin_direct->init() && kwin_direct->start(display_name)) {
          pipewire_fd = -1;
          pipewire_node_id = kwin_direct->node_id;
          BOOST_LOG(info) << "KWin direct capture: selected for output '"sv << display_name << "'"sv;
        } else {
          BOOST_LOG(error) << "KWin direct capture: failed for output '"sv
                           << display_name
                           << "'. Not falling back to KDE portal VIRTUAL source because it is fixed at 1920x1080."sv;
          return -1;
        }
      }
#endif

      // Phase 1: Portal session fallback for non-KWin/non-named capture.
      if (pipewire_node_id == PW_ID_ANY) {
        portal = std::make_unique<portal_session_t>();
        if (!portal->init()) return -1;
        if (!portal->create_session()) return -1;
        if (!portal->select_sources()) return -1;
        if (!portal->start()) return -1;
        pipewire_fd = portal->pw_fd;
        pipewire_node_id = portal->pw_node_id;
      }

      // Phase 2: PipeWire stream
      pw_cap = std::make_unique<pw_capture_t>();
      if (!pw_cap->init(pipewire_fd, pipewire_node_id,
                        config.width > 0 ? static_cast<uint32_t>(config.width) : 1920,
                        config.height > 0 ? static_cast<uint32_t>(config.height) : 1080,
                        config.framerate > 0 ? static_cast<uint32_t>(config.framerate) : 60)) {
        return -1;
      }

      // Wait for format negotiation (up to 5s)
      for (int i = 0; i < 50 && pw_cap->width == 0; ++i) {
        std::this_thread::sleep_for(100ms);
      }

      if (pw_cap->width == 0) {
        BOOST_LOG(error) << "PipeWire: no frames received within 5s"sv;
        return -1;
      }

      width = pw_cap->width;
      height = pw_cap->height;
      env_width = width;
      env_height = height;

      BOOST_LOG(info) << "PipeWire display initialized: "sv << width << "x"sv << height;
      return 0;
    }

    platf::capture_e capture(
      const push_captured_image_cb_t &push_captured_image_cb,
      const pull_free_image_cb_t &pull_free_image_cb,
      bool *cursor
    ) override {
      if (!pw_cap) {
        std::this_thread::sleep_for(delay);
        std::shared_ptr<platf::img_t> img_out;
        if (!pull_free_image_cb(img_out)) return platf::capture_e::interrupted;
        if (!push_captured_image_cb(std::move(img_out), false)) return platf::capture_e::interrupted;
        return platf::capture_e::ok;
      }

      auto next_frame = std::chrono::steady_clock::now();
      sleep_overshoot_logger.reset();

      while (true) {
        auto now = std::chrono::steady_clock::now();
        if (next_frame > now) {
          std::this_thread::sleep_for(next_frame - now);
          sleep_overshoot_logger.first_point(next_frame);
          sleep_overshoot_logger.second_point_now_and_log();
        }
        next_frame += delay;
        if (next_frame < now) next_frame = now + delay;

        // Wait for new frame from PipeWire
        {
          std::unique_lock lk(pw_cap->frame_mtx);
          pw_cap->frame_cv.wait_for(lk, std::chrono::milliseconds(500),
            [this]{ return pw_cap->has_new_frame; });

          if (!pw_cap->has_new_frame) {
            // Timeout — push empty frame
            std::shared_ptr<platf::img_t> img_out;
            if (!push_captured_image_cb(std::move(img_out), false)) {
              return platf::capture_e::ok;
            }
            continue;
          }

          pw_cap->has_new_frame = false;

          // Check for resolution change
          if (pw_cap->width != (uint32_t)width || pw_cap->height != (uint32_t)height) {
            width = pw_cap->width;
            height = pw_cap->height;
            return platf::capture_e::reinit;
          }
        }

        std::shared_ptr<platf::img_t> img_out;
        if (!pull_free_image_cb(img_out)) {
          return platf::capture_e::interrupted;
        }

        auto frame_ts = std::chrono::steady_clock::now();

        // Copy SHM frame data to image
        if (!pw_cap->is_dmabuf) {
          std::lock_guard lk(pw_cap->frame_mtx);
          auto row_bytes = std::min((int)pw_cap->stride, img_out->row_pitch);
          auto rows = std::min((int)pw_cap->height, img_out->height);
          for (int y = 0; y < rows; ++y) {
            std::memcpy(
              img_out->data + y * img_out->row_pitch,
              pw_cap->frame_data.data() + y * pw_cap->stride,
              row_bytes);
          }
        }

        img_out->frame_timestamp = frame_ts;

        if (!push_captured_image_cb(std::move(img_out), true)) {
          return platf::capture_e::ok;
        }
      }
      return platf::capture_e::ok;
    }

    std::shared_ptr<platf::img_t> alloc_img() override {
      auto img = std::make_shared<pw::img_t>();
      img->width = width;
      img->height = height;
      img->pixel_pitch = 4;
      img->row_pitch = img->pixel_pitch * width;
      img->data = new std::uint8_t[height * img->row_pitch]();
      return img;
    }

    int dummy_img(platf::img_t *img) override {
      return 0;
    }

    std::unique_ptr<platf::avcodec_encode_device_t> make_avcodec_encode_device(platf::pix_fmt_e pix_fmt) override {
#ifdef SUNSHINE_BUILD_VAAPI
      if (mem_type == platf::mem_type_e::vaapi) {
        return va::make_avcodec_encode_device(width, height, false);
      }
#endif
      return std::make_unique<platf::avcodec_encode_device_t>();
    }

    platf::mem_type_e mem_type;
    std::chrono::nanoseconds delay;
#ifdef SUNSHINE_BUILD_KWIN
    std::unique_ptr<kwin_session_t> kwin_direct;
#endif
    std::unique_ptr<portal_session_t> portal;
    std::unique_ptr<pw_capture_t> pw_cap;
  };

}  // namespace pw

namespace platf {
  std::shared_ptr<display_t> pw_display(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
    // PipeWire always delivers system memory (SHM) frames. Encoders like NVENC request CUDA,
    // but the Sunshine encoder framework will automatically fallback to GPU->RAM->GPU copy
    // if the display provides system memory.
    auto effective_type = (hwdevice_type == platf::mem_type_e::cuda) ? platf::mem_type_e::system : hwdevice_type;

    if (effective_type != platf::mem_type_e::system && effective_type != platf::mem_type_e::vaapi) {
      BOOST_LOG(error) << "PipeWire: unsupported hw device type"sv;
      return nullptr;
    }

    auto disp = std::make_shared<pw::pw_display_t>();
    if (disp->init(effective_type, display_name, config)) {
      return nullptr;
    }
    return disp;
  }
  std::vector<std::string> pw_display_names() {
    // PipeWire portal doesn't enumerate — returns a single virtual entry
    return {"pipewire-virtual"};
  }
}  // namespace platf

#endif  // SUNSHINE_BUILD_PIPEWIRE
