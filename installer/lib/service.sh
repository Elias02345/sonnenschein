# shellcheck shell=bash
# systemd service installation. Defaults to a --user service because the
# virtual-display backend needs the user's DBus + WAYLAND_DISPLAY session
# (see docs/STATUS.md §9.7).

install_service() {
  # install_service MODE UNIT_FILE AUTOSTART
  #   MODE      = user | system
  #   UNIT_FILE = path to sonnenschein.service
  #   AUTOSTART = 1 to enable on login/boot, 0 otherwise
  local mode="$1"
  local unit_file="$2"
  local autostart="${3:-0}"

  if [ ! -r "$unit_file" ]; then
    warn "Service unit '${unit_file}' not found — skipping service install."
    return 0
  fi

  if [ "$mode" = "user" ]; then
    local dest="${HOME}/.config/systemd/user"
    mkdir -p "$dest"
    install -m644 "$unit_file" "${dest}/sonnenschein.service"
    systemctl --user daemon-reload || true
    if [ "$autostart" = "1" ]; then
      systemctl --user enable sonnenschein.service || warn "Could not enable user service."
      # Allow the service to run without an active login session.
      if have loginctl; then
        loginctl enable-linger "$(id -un)" 2>/dev/null || true
      fi
    fi
    success "User service installed. Start with: systemctl --user start sonnenschein"
  else
    require_sudo
    $SUDO install -m644 "$unit_file" /etc/systemd/system/sonnenschein.service
    $SUDO systemctl daemon-reload || true
    if [ "$autostart" = "1" ]; then
      $SUDO systemctl enable sonnenschein.service || warn "Could not enable system service."
    fi
    success "System service installed. Start with: sudo systemctl start sonnenschein"
  fi
}
