# shellcheck shell=bash
# Distribution detection. Sets DISTRO_ID, DISTRO_LIKE, DISTRO_NAME,
# DISTRO_FAMILY (arch|debian|fedora|opensuse|unknown) and PKG_MANAGER.

detect_distro() {
  DISTRO_ID="unknown"
  DISTRO_LIKE=""
  DISTRO_NAME="Unknown Linux"

  if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    DISTRO_ID="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
    DISTRO_NAME="${PRETTY_NAME:-$DISTRO_ID}"
  fi

  DISTRO_FAMILY="unknown"
  PKG_MANAGER=""

  case " $DISTRO_ID $DISTRO_LIKE " in
    *" arch "* | *" archlinux "* | *" cachyos "* | *" endeavouros "* | *" manjaro "*)
      DISTRO_FAMILY="arch"
      PKG_MANAGER="pacman"
      ;;
    *" debian "* | *" ubuntu "*)
      DISTRO_FAMILY="debian"
      PKG_MANAGER="apt-get"
      ;;
    *" fedora "* | *" rhel "* | *" nobara "*)
      DISTRO_FAMILY="fedora"
      PKG_MANAGER="dnf"
      ;;
    *" opensuse "* | *" suse "* | *" opensuse-tumbleweed "*)
      DISTRO_FAMILY="opensuse"
      PKG_MANAGER="zypper"
      ;;
  esac

  # SteamOS is Arch-based but pins /usr read-only.
  if [ "$DISTRO_ID" = "steamos" ]; then
    DISTRO_FAMILY="arch"
    PKG_MANAGER="pacman"
    STEAMOS=1
  else
    STEAMOS=0
  fi

  export DISTRO_ID DISTRO_LIKE DISTRO_NAME DISTRO_FAMILY PKG_MANAGER STEAMOS
}

distro_summary() {
  info "Detected distribution: ${C_BOLD}${DISTRO_NAME}${C_RESET}"
  info "Package family: ${DISTRO_FAMILY} (${PKG_MANAGER:-no package manager found})"
  if [ "${STEAMOS:-0}" = "1" ]; then
    warn "SteamOS detected — /usr is read-only. Installation is best-effort."
  fi
}
