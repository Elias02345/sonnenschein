# shellcheck shell=bash
# Input-device permissions and (opt-in) KMS capture capability.
#
# IMPORTANT: CAP_SYS_ADMIN on the binary BREAKS PipeWire portal capture —
# xdg-desktop-portal refuses privileged callers ("Unable to open
# /proc/PID/root", see docs/STATUS.md §9.10). PipeWire capture is what the
# virtual-display path needs, so the capability is NOT applied by default.
# Only --kms installs it, for users who exclusively mirror physical
# displays through DRM/KMS.

apply_permissions() {
  # apply_permissions BINARY_PATH RULES_FILE CONF_FILE KMS_CAP
  local binary="$1"
  local rules_file="${2:-}"
  local conf_file="${3:-}"
  local kms_cap="${4:-0}"

  require_sudo

  if [ "$kms_cap" = "1" ]; then
    if have setcap && [ -x "$binary" ]; then
      warn "Granting CAP_SYS_ADMIN for KMS capture (--kms)."
      warn "This DISABLES PipeWire portal capture and thus virtual displays!"
      $SUDO setcap cap_sys_admin+p "$(readlink -f "$binary")" \
        || warn "Failed to set capability — KMS grab may not work."
    else
      warn "setcap not available — skipping capability grant."
    fi
  else
    # Make sure no stale capability from an earlier install breaks the
    # PipeWire path.
    if have getcap && [ -x "$binary" ]; then
      local real
      real="$(readlink -f "$binary")"
      if [ -n "$(getcap "$real" 2>/dev/null)" ]; then
        info "Removing stale file capabilities from ${real} (they break PipeWire capture)."
        $SUDO setcap -r "$real" || warn "Could not remove capabilities from ${real}."
      fi
    fi
  fi

  # Input device udev rule (uinput / uhid) — required for mouse, keyboard
  # and gamepad emulation.
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
