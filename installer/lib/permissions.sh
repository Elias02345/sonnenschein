# shellcheck shell=bash
# Capability + input-device permissions. Mirrors packaging/linux/misc/postinst
# so a from-source install behaves like a packaged one.

apply_permissions() {
  # apply_permissions BINARY_PATH RULES_FILE CONF_FILE
  local binary="$1"
  local rules_file="${2:-}"
  local conf_file="${3:-}"

  require_sudo

  # KMS grab needs CAP_SYS_ADMIN on the binary.
  if have setcap && [ -x "$binary" ]; then
    info "Granting CAP_SYS_ADMIN to ${binary} (KMS capture)."
    $SUDO setcap cap_sys_admin+p "$(readlink -f "$binary")" \
      || warn "Failed to set capability — KMS grab may not work."
  else
    warn "setcap not available — skipping capability grant."
  fi

  # Input device udev rule (uinput / uhid).
  if [ -n "$rules_file" ] && [ -r "$rules_file" ]; then
    info "Installing udev rule for /dev/uinput and /dev/uhid."
    $SUDO install -Dm644 "$rules_file" /etc/udev/rules.d/60-sonnenschein.rules
    if have udevadm; then
      $SUDO udevadm control --reload-rules
      $SUDO udevadm trigger --property-match=DEVNAME=/dev/uinput || true
      $SUDO udevadm trigger --property-match=DEVNAME=/dev/uhid || true
    fi
  fi

  # uhid module autoload (DS5 emulation).
  if [ -n "$conf_file" ] && [ -r "$conf_file" ]; then
    $SUDO install -Dm644 "$conf_file" /etc/modules-load.d/60-sonnenschein.conf
    $SUDO modprobe uhid 2>/dev/null || true
  fi

  success "Permissions configured (a reboot may be needed for udev changes)."
}
