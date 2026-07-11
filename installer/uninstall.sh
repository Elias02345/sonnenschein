#!/usr/bin/env bash
#
# Sonnenschein uninstaller — removes EVERYTHING the installer put on the
# system: every file from the CMake install manifest, the /opt prefix,
# PATH symlinks, services, udev/modules rules, and (optionally) the
# packages the installer pulled in, your configuration, and the source
# checkout. Goal: zero leftovers.
#
set -euo pipefail

PREFIX="${PREFIX:-/opt/sonnenschein}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
# shellcheck source=lib/firewall.sh
. "${SCRIPT_DIR}/lib/firewall.sh"
# shellcheck source=lib/wol.sh
. "${SCRIPT_DIR}/lib/wol.sh"
install_error_trap

# The whole flow lives in main() so bash parses it completely before we
# start deleting files — this script may live INSIDE the prefix it removes.
main() {
  info "Uninstalling Sonnenschein (prefix: ${PREFIX})"

  # Load recorded install metadata if available.
  SRC_DIR=""
  LINGER_ENABLED=0
  FIREWALL_CONFIGURED=none
  FIREWALL_MDNS_ADDED=0
  AVAHI_ENABLED=0
  WOL_CONFIGURED=""
  INSTALL_USER="$(id -un)"
  if [ -r "${PREFIX}/install-state.env" ]; then
    # shellcheck disable=SC1091
    . "${PREFIX}/install-state.env"
  fi

  # --- Stop and remove services -------------------------------------------
  if systemctl --user list-unit-files 2>/dev/null | grep -q '^sonnenschein\.service'; then
    info "Stopping user service."
    systemctl --user disable --now sonnenschein.service 2>/dev/null || true
    rm -f "${HOME}/.config/systemd/user/sonnenschein.service"
    systemctl --user daemon-reload 2>/dev/null || true
  fi

  if systemctl list-unit-files 2>/dev/null | grep -q '^sonnenschein\.service'; then
    require_sudo
    $SUDO systemctl disable --now sonnenschein.service 2>/dev/null || true
    $SUDO rm -f /etc/systemd/system/sonnenschein.service
    $SUDO systemctl daemon-reload 2>/dev/null || true
  fi

  if [ "${LINGER_ENABLED}" = "1" ] && have loginctl; then
    info "Reverting loginctl linger for ${INSTALL_USER} (installer enabled it)."
    loginctl disable-linger "$INSTALL_USER" 2>/dev/null || true
  fi

  require_sudo

  # --- Remove every file from the CMake install manifest ------------------
  local manifest="${PREFIX}/install_manifest.txt"
  if [ -r "$manifest" ]; then
    info "Removing $(grep -c . "$manifest") files from the install manifest."
    while IFS= read -r f; do
      [ -n "$f" ] || continue
      $SUDO rm -f "$f"
    done <"$manifest"
  else
    warn "No install manifest at ${manifest} — removing known paths only."
  fi

  # --- PATH symlinks (only if they point into our prefix) -----------------
  local link target
  for link in /usr/local/bin/sonnenschein /usr/local/bin/sunshine; do
    if [ -L "$link" ]; then
      target="$(readlink -f "$link" || true)"
      case "$target" in
        "${PREFIX}"/*) $SUDO rm -f "$link" ;;
      esac
    fi
  done

  # --- Firewall rules + avahi + WoL (only what WE changed) -----------------
  remove_firewall_rules "$FIREWALL_CONFIGURED" "$FIREWALL_MDNS_ADDED"
  if [ "$AVAHI_ENABLED" = "1" ]; then
    info "Disabling avahi-daemon (installer enabled it)."
    $SUDO systemctl disable --now avahi-daemon 2>/dev/null || true
  fi
  remove_wol "${WOL_CONFIGURED:-}"

  # --- udev / modules-load rules (both possible locations) ----------------
  $SUDO rm -f /etc/udev/rules.d/60-sonnenschein.rules
  $SUDO rm -f /etc/modules-load.d/60-sonnenschein.conf
  $SUDO rm -f /usr/lib/udev/rules.d/60-sonnenschein.rules 2>/dev/null || true
  if have udevadm; then
    $SUDO udevadm control --reload-rules || true
  fi

  # --- Optionally remove the packages the installer pulled in -------------
  local pkg_file="${PREFIX}/installed-packages.txt"
  if [ -s "$pkg_file" ]; then
    local count
    count="$(grep -c . "$pkg_file" || true)"
    echo
    info "The installer added ${count} packages that were not on your system before:"
    sed 's/^/      /' "$pkg_file"
    if confirm "Remove these packages again?" N; then
      local pkgs=()
      mapfile -t pkgs < <(grep -v '^[[:space:]]*$' "$pkg_file")
      if command -v pacman >/dev/null 2>&1; then
        $SUDO pacman -Rns --noconfirm "${pkgs[@]}" || warn "Some packages could not be removed (may be needed by others)."
      elif command -v apt-get >/dev/null 2>&1; then
        $SUDO apt-get remove -y "${pkgs[@]}" || warn "Some packages could not be removed."
        $SUDO apt-get autoremove -y || true
      elif command -v dnf >/dev/null 2>&1; then
        $SUDO dnf remove -y "${pkgs[@]}" || warn "Some packages could not be removed."
      elif command -v zypper >/dev/null 2>&1; then
        $SUDO zypper --non-interactive remove "${pkgs[@]}" || warn "Some packages could not be removed."
      fi
      success "Installer-added packages removed."
    else
      info "Packages kept."
    fi
  fi

  # --- Optionally drop user config/state -----------------------------------
  echo
  if confirm "Also remove your configuration and pairing data (~/.config/sunshine)?" N; then
    rm -rf "${HOME}/.config/sunshine"
    success "Configuration removed."
  else
    info "Configuration kept at ~/.config/sunshine."
  fi
  if [ -d "${HOME}/.local/state/sonnenschein" ]; then
    rm -rf "${HOME}/.local/state/sonnenschein"
    info "Runtime state removed (~/.local/state/sonnenschein)."
  fi

  # --- Optionally drop the source checkout ---------------------------------
  if [ -n "$SRC_DIR" ] && [ -d "$SRC_DIR" ] && [ "$SRC_DIR" != "/" ]; then
    if confirm "Remove the source checkout at ${SRC_DIR}?" Y; then
      rm -rf "$SRC_DIR"
      success "Source checkout removed."
    fi
  fi

  # --- Finally: the prefix itself ------------------------------------------
  if [ -d "$PREFIX" ] && [ "$PREFIX" != "/" ] && [ "$PREFIX" != "/usr" ] && [ "$PREFIX" != "/usr/local" ]; then
    $SUDO rm -rf "$PREFIX"
  fi

  success "Sonnenschein uninstalled — no leftovers."
}

main "$@"
