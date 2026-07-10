# shellcheck shell=bash
# Package installation. Reads the per-family package list and installs the
# build + runtime dependencies. A before/after snapshot records exactly
# which packages THIS installer added, so uninstall.sh can offer to remove
# them again without touching anything that was already on the system.

# Read a package list file, stripping comments and blank lines.
_read_pkg_list() {
  local file="$1"
  [ -r "$file" ] || return 0
  sed -e 's/#.*$//' -e 's/[[:space:]]*$//' "$file" | grep -v '^[[:space:]]*$'
}

# List every installed package name, one per line (sorted).
_list_installed() {
  case "$PKG_MANAGER" in
    pacman) pacman -Qq 2>/dev/null | sort ;;
    apt-get) dpkg-query -W -f='${Package}\n' 2>/dev/null | sort ;;
    dnf) rpm -qa --qf '%{NAME}\n' 2>/dev/null | sort ;;
    zypper) rpm -qa --qf '%{NAME}\n' 2>/dev/null | sort ;;
    *) return 0 ;;
  esac
}

PKG_SNAPSHOT_FILE=""

packages_snapshot_before() {
  PKG_SNAPSHOT_FILE="$(mktemp /tmp/sonnenschein-pkgs.XXXXXX)"
  _list_installed >"$PKG_SNAPSHOT_FILE" || true
  export PKG_SNAPSHOT_FILE
}

# Diff the snapshot against the current state and persist the newly
# installed package names under the prefix (root-owned, survives updates).
packages_record_new() {
  local prefix="$1"
  [ -n "$PKG_SNAPSHOT_FILE" ] && [ -r "$PKG_SNAPSHOT_FILE" ] || return 0
  local after new
  after="$(mktemp /tmp/sonnenschein-pkgs.XXXXXX)"
  _list_installed >"$after" || true
  new="$(comm -13 "$PKG_SNAPSHOT_FILE" "$after" || true)"
  rm -f "$PKG_SNAPSHOT_FILE" "$after"
  require_sudo
  $SUDO mkdir -p "$prefix"
  printf '%s\n' "$new" | $SUDO tee "${prefix}/installed-packages.txt" >/dev/null
  if [ -n "$new" ]; then
    info "Recorded $(printf '%s\n' "$new" | grep -c .) newly installed packages for clean uninstall."
  else
    info "No new packages were needed — everything was already installed."
  fi
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
