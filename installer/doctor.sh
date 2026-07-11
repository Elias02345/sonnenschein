#!/usr/bin/env bash
#
# Sonnenschein doctor — checks every prerequisite the streaming path needs
# and prints [OK]/[FAIL] per check. With --repair it fixes what it can:
# stale file capabilities (they break PipeWire capture), udev rules,
# missing uhid module, and a stuck service.
#
# Usage: bash doctor.sh [--repair]
#
set -uo pipefail

PREFIX="${PREFIX:-/opt/sonnenschein}"
REPAIR=0
[ "${1:-}" = "--repair" ] && REPAIR=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
# shellcheck source=lib/firewall.sh
. "${SCRIPT_DIR}/lib/firewall.sh"

PASS=0
FAIL=0

ok_check() { printf '%s[OK]%s   %s\n' "$C_GREEN" "$C_RESET" "$*"; PASS=$((PASS + 1)); }
fail_check() { printf '%s[FAIL]%s %s\n' "$C_RED" "$C_RESET" "$*"; FAIL=$((FAIL + 1)); }
note() { printf '       %s\n' "$*"; }

BINARY="${PREFIX}/bin/sonnenschein"
[ -x "$BINARY" ] || BINARY="$(command -v sonnenschein || true)"

# --- 1. Binary -------------------------------------------------------------
if [ -n "$BINARY" ] && [ -x "$BINARY" ]; then
  ok_check "Binary found: ${BINARY}"
else
  fail_check "sonnenschein binary not found (looked in ${PREFIX}/bin and PATH)."
  note "Re-run the installer: installer/install.sh"
fi

# --- 2. File capabilities (must be EMPTY for PipeWire capture) -------------
if [ -n "$BINARY" ] && command -v getcap >/dev/null 2>&1; then
  REAL="$(readlink -f "$BINARY")"
  CAPS="$(getcap "$REAL" 2>/dev/null || true)"
  if [ -n "$CAPS" ]; then
    fail_check "Binary has file capabilities: ${CAPS}"
    note "CAP_SYS_ADMIN breaks PipeWire portal capture (virtual displays)."
    if [ "$REPAIR" = "1" ]; then
      require_sudo
      $SUDO setcap -r "$REAL" && ok_check "Repaired: capabilities removed."
    else
      note "Fix: sudo setcap -r ${REAL}   (or re-run doctor.sh --repair)"
    fi
  else
    ok_check "No file capabilities on the binary (PipeWire capture unblocked)."
  fi
fi

# --- 3. Session: Wayland + KDE ----------------------------------------------
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
  ok_check "Wayland session active."
else
  fail_check "Session type is '${XDG_SESSION_TYPE:-unknown}', not wayland."
  note "Virtual displays need a Wayland session (KDE Plasma 6 recommended)."
fi

if [ -n "${WAYLAND_DISPLAY:-}" ]; then
  ok_check "WAYLAND_DISPLAY=${WAYLAND_DISPLAY}"
else
  fail_check "WAYLAND_DISPLAY is not set — run doctor from a terminal INSIDE your desktop session (not SSH/TTY)."
fi

case "$(printf '%s' "${XDG_CURRENT_DESKTOP:-}" | tr '[:upper:]' '[:lower:]')" in
  *kde* | *plasma*) ok_check "KDE Plasma detected (first-class support)." ;;
  *) fail_check "Desktop is '${XDG_CURRENT_DESKTOP:-unknown}' — KWin virtual displays unavailable; portal capture only." ;;
esac

# --- 4. kscreen-doctor + Plasma version -------------------------------------
if command -v kscreen-doctor >/dev/null 2>&1; then
  ok_check "kscreen-doctor present (monitor on/off control)."
else
  fail_check "kscreen-doctor missing — install 'libkscreen' / 'kscreen'."
fi

if command -v plasmashell >/dev/null 2>&1; then
  PLASMA_VER="$(plasmashell --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -n1 || true)"
  if [ -n "$PLASMA_VER" ]; then
    MAJOR="${PLASMA_VER%%.*}"
    MINOR="${PLASMA_VER#*.}"
    if [ "$MAJOR" -gt 6 ] || { [ "$MAJOR" -eq 6 ] && [ "$MINOR" -ge 6 ]; }; then
      ok_check "Plasma ${PLASMA_VER} (>= 6.6: custom refresh rates on virtual outputs supported)."
    else
      fail_check "Plasma ${PLASMA_VER} < 6.6 — virtual outputs are limited to 60 Hz."
    fi
  fi
fi

# --- 5. PipeWire ------------------------------------------------------------
if command -v pw-cli >/dev/null 2>&1 && pw-cli info 0 >/dev/null 2>&1; then
  ok_check "PipeWire daemon reachable."
else
  if systemctl --user is-active --quiet pipewire 2>/dev/null; then
    ok_check "PipeWire service active."
  else
    fail_check "PipeWire not reachable — screen capture will fail."
    [ "$REPAIR" = "1" ] && systemctl --user restart pipewire 2>/dev/null && note "Restarted pipewire."
  fi
fi

if command -v busctl >/dev/null 2>&1 && busctl --user status org.freedesktop.portal.Desktop >/dev/null 2>&1; then
  ok_check "xdg-desktop-portal reachable."
else
  fail_check "xdg-desktop-portal not reachable — portal capture fallback unavailable."
fi

# --- 6. Input: uinput -------------------------------------------------------
if [ -e /dev/uinput ]; then
  if [ -w /dev/uinput ]; then
    ok_check "/dev/uinput writable (mouse/keyboard/gamepad emulation)."
  else
    fail_check "/dev/uinput exists but is not writable by $(id -un)."
    note "udev rule 60-sonnenschein.rules should grant access; log out/in or reboot."
    if [ "$REPAIR" = "1" ] && [ -r "${SCRIPT_DIR}/../src_assets/linux/misc/60-sonnenschein.rules" ]; then
      require_sudo
      $SUDO install -Dm644 "${SCRIPT_DIR}/../src_assets/linux/misc/60-sonnenschein.rules" /etc/udev/rules.d/60-sonnenschein.rules
      $SUDO udevadm control --reload-rules
      $SUDO udevadm trigger --property-match=DEVNAME=/dev/uinput || true
      note "Reinstalled udev rule + retriggered."
    fi
  fi
else
  fail_check "/dev/uinput missing — kernel module uinput not loaded."
  [ "$REPAIR" = "1" ] && { require_sudo; $SUDO modprobe uinput && note "Loaded uinput module."; }
fi

# --- 7. GPU / NVIDIA driver --------------------------------------------------
if command -v nvidia-smi >/dev/null 2>&1; then
  DRV="$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -n1 || true)"
  if [ -n "$DRV" ]; then
    DRV_MAJOR="${DRV%%.*}"
    if [ "$DRV_MAJOR" -ge 580 ] 2>/dev/null; then
      ok_check "NVIDIA driver ${DRV} (>= 580: Wayland HDR capable)."
    else
      fail_check "NVIDIA driver ${DRV} < 580 — HDR over Wayland unavailable; update the driver."
    fi
  fi
elif [ -d /proc/driver/nvidia ]; then
  fail_check "NVIDIA kernel driver loaded but nvidia-smi missing (install nvidia-utils)."
else
  note "No NVIDIA GPU detected — skipping driver check (AMD/Intel use VAAPI)."
fi

# --- 7b. Moonlight discovery: avahi + firewall ---------------------------------
if systemctl is-active --quiet avahi-daemon 2>/dev/null; then
  ok_check "avahi-daemon running (Moonlight clients can discover this host)."
else
  fail_check "avahi-daemon not running — Moonlight will NOT find this host automatically."
  if [ "$REPAIR" = "1" ]; then
    require_sudo
    if $SUDO systemctl enable --now avahi-daemon 2>/dev/null; then
      ok_check "Repaired: avahi-daemon enabled and started."
    fi
  else
    note "Fix: sudo systemctl enable --now avahi-daemon"
  fi
fi

if command -v ufw >/dev/null 2>&1 && sudo -n ufw status 2>/dev/null | grep -q "Status: active"; then
  if sudo -n ufw status 2>/dev/null | grep -q "47989"; then
    ok_check "ufw active with Sonnenschein ports open."
  else
    fail_check "ufw is active but the Moonlight ports are NOT open."
    if [ "$REPAIR" = "1" ]; then
      configure_firewall && ok_check "Repaired: firewall rules added."
    else
      note "Fix: re-run the installer or doctor.sh --repair"
    fi
  fi
elif command -v firewall-cmd >/dev/null 2>&1 && sudo -n firewall-cmd --state >/dev/null 2>&1; then
  if sudo -n firewall-cmd --list-ports 2>/dev/null | grep -q "47989"; then
    ok_check "firewalld active with Sonnenschein ports open."
  else
    fail_check "firewalld is running but the Moonlight ports are NOT open."
    if [ "$REPAIR" = "1" ]; then
      configure_firewall && ok_check "Repaired: firewall rules added."
    else
      note "Fix: re-run the installer or doctor.sh --repair"
    fi
  fi
else
  note "No active firewall detected (or sudo needed) — skipping port check."
fi

# --- 7c. Wake-on-LAN (Boot-to-Ready) -------------------------------------------
if [ -r "${PREFIX}/install-state.env" ]; then
  WOL_CONFIGURED=""
  # shellcheck disable=SC1091
  . "${PREFIX}/install-state.env"
  if [ -n "${WOL_CONFIGURED:-}" ] && command -v ethtool >/dev/null 2>&1; then
    WOL_NOW="$(sudo -n ethtool "$WOL_CONFIGURED" 2>/dev/null | sed -n 's/.*Wake-on: \([a-z]*\)$/\1/p' | tail -n1)"
    if [ "$WOL_NOW" = "g" ]; then
      MAC="$(cat "/sys/class/net/${WOL_CONFIGURED}/address" 2>/dev/null || true)"
      ok_check "Wake-on-LAN armed on ${WOL_CONFIGURED}${MAC:+ (MAC ${MAC})}."
    elif [ -n "$WOL_NOW" ]; then
      fail_check "Wake-on-LAN on ${WOL_CONFIGURED} is '${WOL_NOW}', not 'g' (magic packet)."
      if [ "$REPAIR" = "1" ]; then
        require_sudo
        $SUDO ethtool -s "$WOL_CONFIGURED" wol g && ok_check "Repaired: WoL re-armed."
      else
        note "Fix: sudo ethtool -s ${WOL_CONFIGURED} wol g   (or doctor.sh --repair)"
      fi
    else
      note "Wake-on-LAN state unreadable without sudo — skipping check."
    fi
  fi
fi

# --- 8. Service state ---------------------------------------------------------
if systemctl --user list-unit-files 2>/dev/null | grep -q '^sonnenschein\.service'; then
  if systemctl --user is-active --quiet sonnenschein.service; then
    ok_check "sonnenschein user service is running."
  else
    fail_check "sonnenschein user service installed but not running."
    if [ "$REPAIR" = "1" ]; then
      systemctl --user restart sonnenschein.service && ok_check "Repaired: service restarted."
    else
      note "Start: systemctl --user start sonnenschein"
      note "Logs:  journalctl --user -eu sonnenschein"
    fi
  fi
else
  note "No user service installed (running manually is fine too)."
fi

# --- 9. WebUI health -----------------------------------------------------------
# The service unit sleeps 5s before starting (desktop-init grace period), so
# poll for up to ~15s instead of failing on the first probe.
if command -v curl >/dev/null 2>&1; then
  WEBUI_OK=0
  for _ in 1 2 3 4 5 6; do
    if curl -kfsS -o /dev/null --max-time 3 https://localhost:47990 2>/dev/null; then
      WEBUI_OK=1
      break
    fi
    sleep 2
  done
  if [ "$WEBUI_OK" = "1" ]; then
    ok_check "WebUI responds at https://localhost:47990"
  else
    fail_check "WebUI not reachable at https://localhost:47990 (is the service running?)"
    note "Logs: journalctl --user -eu sonnenschein"
  fi
fi

# --- Summary --------------------------------------------------------------------
echo
if [ "$FAIL" -eq 0 ]; then
  success "All ${PASS} checks passed — Sonnenschein is ready to stream."
  exit 0
else
  warn "${FAIL} check(s) failed, ${PASS} passed."
  [ "$REPAIR" = "0" ] && info "Try: bash ${BASH_SOURCE[0]} --repair"
  exit 1
fi
