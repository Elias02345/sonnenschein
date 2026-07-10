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
if command -v curl >/dev/null 2>&1; then
  if curl -kfsS -o /dev/null --max-time 5 https://localhost:47990 2>/dev/null; then
    ok_check "WebUI responds at https://localhost:47990"
  else
    fail_check "WebUI not reachable at https://localhost:47990 (is the service running?)"
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
