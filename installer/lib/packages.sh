# shellcheck shell=bash
# Package installation. Reads the per-family package list and installs the
# runtime dependencies plus the GPU-specific encoder packages.

# Read a package list file, stripping comments and blank lines.
_read_pkg_list() {
  local file="$1"
  [ -r "$file" ] || return 0
  sed -e 's/#.*$//' -e 's/[[:space:]]*$//' "$file" | grep -v '^[[:space:]]*$'
}

install_packages() {
  # install_packages PACKAGES_DIR
  local pkg_dir="$1"
  local list_file="${pkg_dir}/${DISTRO_FAMILY}.list"
  local packages=()

  if [ ! -r "$list_file" ]; then
    warn "No package list for family '${DISTRO_FAMILY}' — skipping automatic dependency install."
    warn "Install the build/runtime dependencies manually (see docs/building.md)."
    return 0
  fi

  mapfile -t packages < <(_read_pkg_list "$list_file")
  if [ "${#packages[@]}" -eq 0 ]; then
    warn "Package list '${list_file}' is empty."
    return 0
  fi

  info "Installing ${#packages[@]} packages for ${DISTRO_FAMILY}..."
  require_sudo

  case "$PKG_MANAGER" in
    pacman)
      $SUDO pacman -Sy --needed --noconfirm "${packages[@]}"
      ;;
    apt-get)
      $SUDO apt-get update
      $SUDO apt-get install -y "${packages[@]}"
      ;;
    dnf)
      $SUDO dnf install -y "${packages[@]}"
      ;;
    zypper)
      $SUDO zypper --non-interactive install "${packages[@]}"
      ;;
    *)
      die "Unsupported package manager: '${PKG_MANAGER:-none}'. Install dependencies manually."
      ;;
  esac

  success "Dependencies installed."
}
