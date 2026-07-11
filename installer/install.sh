#!/usr/bin/env bash
#
# Sonnenschein installer — distro-aware, from-source build + service setup.
#
# Everything lands in ONE directory (/opt/sonnenschein) plus a recorded
# manifest, so uninstall.sh can remove the installation without leftovers.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash
#   ./installer/install.sh [--yes] [--branch <name>] [--prefix <dir>] [--kms]
#
# Options:
#   --yes     non-interactive, accept all defaults
#   --branch  git branch to install (default: main)
#   --prefix  install prefix (default: /opt/sonnenschein)
#   --kms     grant CAP_SYS_ADMIN for KMS capture of physical displays.
#             WARNING: this breaks PipeWire portal capture (virtual
#             displays), so it is OFF by default. Only enable it if you
#             exclusively mirror physical monitors.
#
set -euo pipefail

REPO_URL="https://github.com/Elias02345/sonnenschein.git"
BRANCH="main"
PREFIX="/opt/sonnenschein"
KMS_CAP=0

# --- Argument parsing -----------------------------------------------------
while [ "$#" -gt 0 ]; do
  case "$1" in
    --yes | -y) export SONNENSCHEIN_ASSUME_YES=1 ;;
    --branch) BRANCH="${2:?--branch needs a value}"; shift ;;
    --prefix) PREFIX="${2:?--prefix needs a value}"; shift ;;
    --kms) KMS_CAP=1 ;;
    -h | --help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//' | head -n 20
      exit 0
      ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

# Piped installs (curl | bash) have no TTY on stdin — don't hang on prompts.
if [ ! -t 0 ]; then
  export SONNENSCHEIN_ASSUME_YES=1
fi

# Never build/install as root: npm + cmake caches would be root-owned and
# the systemd --user service must belong to the desktop user.
if [ "$(id -u)" -eq 0 ] && [ -z "${SONNENSCHEIN_ALLOW_ROOT:-}" ]; then
  echo "Do not run the installer as root. Run it as your desktop user;" >&2
  echo "it will ask for sudo only where needed." >&2
  exit 1
fi

# --- Locate the library directory, bootstrapping if piped via curl --------
# The source checkout is persistent (needed by update.sh / uninstall.sh),
# NOT a throwaway mktemp dir.
SRC_DIR="${SONNENSCHEIN_SRC_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/sonnenschein/src}"

SCRIPT_SOURCE="${BASH_SOURCE[0]:-}"
SCRIPT_DIR=""
if [ -n "$SCRIPT_SOURCE" ] && [ -f "$SCRIPT_SOURCE" ]; then
  SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"
fi

if [ -z "$SCRIPT_DIR" ] || [ ! -f "${SCRIPT_DIR}/lib/common.sh" ]; then
  # Running from a pipe (curl | bash): clone the repo, then re-exec.
  command -v git >/dev/null 2>&1 || { echo "git is required to bootstrap the installer." >&2; exit 1; }
  if [ -d "${SRC_DIR}/.git" ]; then
    echo "==> Updating existing source checkout in ${SRC_DIR}"
    git -C "$SRC_DIR" fetch origin "$BRANCH"
    git -C "$SRC_DIR" checkout "$BRANCH"
    git -C "$SRC_DIR" reset --hard "origin/${BRANCH}"
  else
    echo "==> Cloning Sonnenschein into ${SRC_DIR}"
    mkdir -p "$(dirname "$SRC_DIR")"
    git clone --branch "$BRANCH" "$REPO_URL" "$SRC_DIR"
  fi
  REEXEC_ARGS=(--branch "$BRANCH" --prefix "$PREFIX")
  [ "${SONNENSCHEIN_ASSUME_YES:-0}" = "1" ] && REEXEC_ARGS+=(--yes)
  [ "$KMS_CAP" = "1" ] && REEXEC_ARGS+=(--kms)
  exec bash "${SRC_DIR}/installer/install.sh" "${REEXEC_ARGS[@]}"
fi

# The repo root is the parent of the installer directory.
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
# shellcheck source=lib/distro.sh
. "${SCRIPT_DIR}/lib/distro.sh"
# shellcheck source=lib/gpu.sh
. "${SCRIPT_DIR}/lib/gpu.sh"
# shellcheck source=lib/compositor.sh
. "${SCRIPT_DIR}/lib/compositor.sh"
# shellcheck source=lib/packages.sh
. "${SCRIPT_DIR}/lib/packages.sh"
# shellcheck source=lib/service.sh
. "${SCRIPT_DIR}/lib/service.sh"
# shellcheck source=lib/permissions.sh
. "${SCRIPT_DIR}/lib/permissions.sh"
# shellcheck source=lib/firewall.sh
. "${SCRIPT_DIR}/lib/firewall.sh"
# shellcheck source=lib/wol.sh
. "${SCRIPT_DIR}/lib/wol.sh"
# shellcheck source=lib/libva.sh
. "${SCRIPT_DIR}/lib/libva.sh"
# shellcheck source=lib/ui.sh
. "${SCRIPT_DIR}/lib/ui.sh"

install_error_trap

# --- Build from source ----------------------------------------------------
build_from_source() {
  local build_dir="${REPO_ROOT}/build"
  info "Configuring build in ${build_dir}"

  # Submodules are required (moonlight-common-c, build-deps, etc.).
  if [ -f "${REPO_ROOT}/.gitmodules" ]; then
    info "Fetching submodules (first run downloads ~1 GB, please be patient)..."
    git -C "$REPO_ROOT" submodule update --init --recursive
  fi

  cmake -S "$REPO_ROOT" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSUNSHINE_EXECUTABLE_PATH="${PREFIX}/bin/sonnenschein" \
    -DBUILD_DOCS=OFF \
    -DBUILD_TESTS=OFF

  info "Building (this can take 10-30 minutes)..."
  cmake --build "$build_dir"

  info "Installing to ${PREFIX} (needs privileges)."
  require_sudo
  $SUDO cmake --install "$build_dir"

  # Preserve the exact file list for a leftover-free uninstall.
  if [ -f "${build_dir}/install_manifest.txt" ]; then
    $SUDO install -Dm644 "${build_dir}/install_manifest.txt" \
      "${PREFIX}/install_manifest.txt"
  fi

  BUILT_BINARY="${PREFIX}/bin/sonnenschein"
  export BUILT_BINARY
}

# Symlink the binary into /usr/local/bin so `sonnenschein` is on PATH.
link_into_path() {
  require_sudo
  $SUDO mkdir -p /usr/local/bin
  $SUDO ln -sfn "${PREFIX}/bin/sonnenschein" /usr/local/bin/sonnenschein
  # `sunshine` compat symlink for muscle memory / existing scripts.
  if [ ! -e /usr/local/bin/sunshine ] || [ -L /usr/local/bin/sunshine ]; then
    $SUDO ln -sfn "${PREFIX}/bin/sonnenschein" /usr/local/bin/sunshine
  fi
}

# Record install metadata so uninstall.sh works standalone and exactly.
write_install_state() {
  require_sudo
  $SUDO mkdir -p "$PREFIX"
  {
    echo "PREFIX=${PREFIX}"
    echo "SRC_DIR=${REPO_ROOT}"
    echo "BRANCH=${BRANCH}"
    echo "INSTALL_USER=$(id -un)"
    echo "LINGER_ENABLED=${LINGER_ENABLED:-0}"
    echo "KMS_CAP=${KMS_CAP}"
    echo "FIREWALL_CONFIGURED=${FIREWALL_CONFIGURED:-none}"
    echo "FIREWALL_MDNS_ADDED=${FIREWALL_MDNS_ADDED:-0}"
    echo "AVAHI_ENABLED=${AVAHI_ENABLED:-0}"
    echo "WOL_CONFIGURED=${WOL_CONFIGURED:-}"
    echo "INSTALL_DATE=$(date -Is)"
  } | $SUDO tee "${PREFIX}/install-state.env" >/dev/null

  # Copy the installer itself so uninstall/doctor survive a deleted checkout.
  $SUDO mkdir -p "${PREFIX}/installer"
  $SUDO cp -r "${SCRIPT_DIR}/." "${PREFIX}/installer/"
}

# --- Main flow ------------------------------------------------------------
main() {
  print_banner
  info "Logging to ${SONNENSCHEIN_LOG}"
  info "Install prefix: ${PREFIX}"
  info "Source checkout: ${REPO_ROOT}"

  detect_distro
  detect_gpu
  detect_compositor

  ui_confirm_distro
  gpu_summary
  compositor_summary

  packages_snapshot_before
  install_packages "${SCRIPT_DIR}/packages"
  ensure_libva

  build_from_source
  link_into_path
  packages_record_new "$PREFIX"

  ui_choose_service_mode
  ui_choose_autostart

  apply_permissions \
    "$BUILT_BINARY" \
    "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.rules" \
    "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.conf" \
    "$KMS_CAP"

  configure_firewall
  ensure_avahi
  configure_wol

  install_service "$SERVICE_MODE" "${REPO_ROOT}/build/sonnenschein.service" "$AUTOSTART"
  write_install_state

  # Start (or restart onto the fresh binary) right away — no manual step.
  if [ "$SERVICE_MODE" = "user" ]; then
    if systemctl --user restart sonnenschein.service 2>/dev/null; then
      success "Sonnenschein service started."
    else
      warn "Could not start the user service — start manually: systemctl --user start sonnenschein"
    fi
  else
    $SUDO systemctl restart sonnenschein.service 2>/dev/null \
      && success "Sonnenschein service started." \
      || warn "Could not start the system service."
  fi

  ui_final_summary "$PREFIX" "$SERVICE_MODE"
}

main "$@"
