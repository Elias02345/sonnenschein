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
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

// pipewire includes
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/debug/types.h>
#include <spa/param/video/raw.h>
#include <spa/utils/result.h>

// GLib / GDBus for portal communication
#include <gio/gio.h>

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

  // Helper: unique request token
  static std::atomic<uint32_t> request_counter{0};

  static std::string make_token() {
    return "sonnenschein_" + std::to_string(++request_counter);
  }

  static std::string make_request_path(GDBusConnection *conn) {
    auto name = g_dbus_connection_get_unique_name(conn);
    std::string sender(name ? name : "");
    // Replace dots and leading colon
    for (auto &c : sender) {
      if (c == '.' || c == ':') c = '_';
    }
    return "/org/freedesktop/portal/desktop/request/" + sender + "/" + make_token();
  }

  /**
   * Portal session: manages D-Bus interaction with xdg-desktop-portal.
   */
  struct portal_session_t {
    GDBusConnection *conn = nullptr;
    std::string session_handle;
    std::string restore_token;
    uint32_t pw_node_id = 0;
    int pw_fd = -1;
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
      return true;
    }

    bool create_session() {
      auto token = make_token();
      auto req_path = make_request_path(conn);
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
      auto req_path = make_request_path(conn);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(make_token().c_str()));
      g_variant_builder_add(opts, "{sv}", "types", g_variant_new_uint32(1)); // MONITOR=1
      g_variant_builder_add(opts, "{sv}", "multiple", g_variant_new_boolean(FALSE));
      g_variant_builder_add(opts, "{sv}", "persist_mode", g_variant_new_uint32(2)); // persistent
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
      auto req_path = make_request_path(conn);
      auto sig = subscribe_response(req_path);

      auto opts = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(opts, "{sv}", "handle_token", g_variant_new_string(make_token().c_str()));

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
            g_variant_unref(stream_entry);
            BOOST_LOG(info) << "Portal: stream node_id="sv << pw_node_id;
          }
          g_variant_unref(streams);
        }
      }

      // Get PipeWire fd
      GError *fd_err = nullptr;
      GUnixFDList *fd_list = nullptr;
      auto fd_result = g_dbus_connection_call_with_unix_fd_list_sync(
        conn, PORTAL_BUS_NAME, PORTAL_OBJECT, PORTAL_SCREENCAST,
        "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", session_handle.c_str(),
                       g_variant_builder_end(g_variant_builder_new(G_VARIANT_TYPE("a{sv}")))),
        nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &fd_list, nullptr, &fd_err);

      if (!fd_result) {
        BOOST_LOG(error) << "Portal: OpenPipeWireRemote failed: "sv << (fd_err ? fd_err->message : "unknown");
        if (fd_err) g_error_free(fd_err);
        return false;
      }

      gint32 fd_index = 0;
      g_variant_get(fd_result, "(h)", &fd_index);
      pw_fd = g_unix_fd_list_get(fd_list, fd_index, nullptr);
      g_variant_unref(fd_result);
      g_object_unref(fd_list);

      BOOST_LOG(info) << "Portal: PipeWire fd="sv << pw_fd << " node_id="sv << pw_node_id;
      ready = (pw_fd >= 0 && pw_node_id > 0);
      return ready;
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
    }

    static void on_state_changed(void *userdata, enum pw_stream_state old,
                                  enum pw_stream_state state, const char *error) {
      BOOST_LOG(info) << "PipeWire: stream state "sv
                      << pw_stream_state_as_string(old) << " -> "sv
                      << pw_stream_state_as_string(state)
                      << (error ? std::string(" (") + error + ")" : "");
    }

    static const pw_stream_events stream_events;

    bool init(int fd, uint32_t node_id) {
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

      core = pw_context_connect_fd(context, fcntl(fd, F_DUPFD_CLOEXEC, 3), nullptr, 0);
      if (!core) {
        BOOST_LOG(error) << "PipeWire: failed to connect to remote"sv;
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

      struct spa_rectangle default_size = SPA_RECTANGLE(1920, 1080);
      struct spa_rectangle min_size = SPA_RECTANGLE(1, 1);
      struct spa_rectangle max_size = SPA_RECTANGLE(8192, 8192);
      struct spa_fraction default_rate = SPA_FRACTION(60, 1);
      struct spa_fraction min_rate = SPA_FRACTION(0, 1);
      struct spa_fraction max_rate = SPA_FRACTION(144, 1);

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

      // Phase 1: Portal session
      portal = std::make_unique<portal_session_t>();
      if (!portal->init()) return -1;
      if (!portal->create_session()) return -1;
      if (!portal->select_sources()) return -1;
      if (!portal->start()) return -1;

      // Phase 2: PipeWire stream
      pw_cap = std::make_unique<pw_capture_t>();
      if (!pw_cap->init(portal->pw_fd, portal->pw_node_id)) return -1;

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
