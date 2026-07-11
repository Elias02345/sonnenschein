# shellcheck shell=bash
# libva backport for Debian/Ubuntu (docs/STATUS.md §9.1).
#
# The prebuilt FFmpeg in third-party/build-deps links against
# vaMapBuffer2, introduced in libva 2.22. Ubuntu 24.04 ships 2.20 and
# Debian 12 ships 2.17, so a from-source Sonnenschein build fails at
# link time there. Arch/CachyOS (2.23+) and Fedora 41+ (2.22+) are fine.
# Fix: build libva 2.22 from source into /usr — same recipe as
# docs/building.md, automated.

ensure_libva() {
  # Only the debian family ships libva that is too old.
  [ "${DISTRO_FAMILY:-}" = "debian" ] || return 0

  local ver
  ver="$(pkg-config --modversion libva 2>/dev/null || echo 0)"
  if dpkg --compare-versions "$ver" ge 2.22 2>/dev/null; then
    info "libva ${ver} is new enough (>= 2.22)."
    return 0
  fi

  warn "libva ${ver} is too old for the prebuilt FFmpeg (needs >= 2.22, symbol vaMapBuffer2)."
  info "Building libva 2.22 from source (one-time, ~2 minutes)..."
  require_sudo
  $SUDO apt-get install -y meson python3-mako \
    libxcb-dri3-dev libxcb-present-dev libxcb-sync-dev libxcb-xfixes0-dev \
    libx11-xcb-dev >/dev/null

  local tmp multiarch
  tmp="$(mktemp -d /tmp/sonnenschein-libva.XXXXXX)"
  multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo x86_64-linux-gnu)"

  if ! git clone --depth 1 -b 2.22.0 https://github.com/intel/libva.git "${tmp}/libva"; then
    rm -rf "$tmp"
    die "Could not clone libva. Check your network and re-run the installer."
  fi

  (
    cd "${tmp}/libva"
    meson setup build --prefix=/usr --libdir="lib/${multiarch}" --buildtype=release \
      -Dwith_x11=yes -Dwith_wayland=yes -Dwith_glx=yes
    ninja -C build
  ) || {
    rm -rf "$tmp"
    die "libva build failed. See the log above; docs/building.md has the manual recipe."
  }
  $SUDO ninja -C "${tmp}/libva/build" install || {
    rm -rf "$tmp"
    die "libva install failed."
  }
  $SUDO ldconfig
  rm -rf "$tmp"

  ver="$(pkg-config --modversion libva 2>/dev/null || echo unknown)"
  success "libva ${ver} installed from source."
}
