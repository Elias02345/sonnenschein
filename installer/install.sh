#!/usr/bin/env bash
#
# Sonnenschein installer — distro-aware, from-source build + service setup.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash
#   ./installer/install.sh [--yes] [--branch <name>] [--prefix <dir>]
#
set -euo pipefail

REPO_URL="https://github.com/Elias02345/sonnenschein.git"
BRANCH="main"
PREFIX="/usr/local"

# --- Argument parsing -----------------------------------------------------
while [ "$#" -gt 0 ]; do
  case "$1" in
    --yes | -y) export SONNENSCHEIN_ASSUME_YES=1 ;;
    --branch) BRANCH="${2:?--branch needs a value}"; shift ;;
    --prefix) PREFIX="${2:?--prefix needs a value}"; shift ;;
    -h | --help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//' | head -n 8
      exit 0
      ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

# --- Locate the library directory, bootstrapping if piped via curl --------
SCRIPT_SOURCE="${BASH_SOURCE[0]:-}"
SCRIPT_DIR=""
if [ -n "$SCRIPT_SOURCE" ] && [ -f "$SCRIPT_SOURCE" ]; then
  SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"
fi

if [ -z "$SCRIPT_DIR" ] || [ ! -f "${SCRIPT_DIR}/lib/common.sh" ]; then
  # Running from a pipe (curl | bash): clone the repo, then re-exec.
  command -v git >/dev/null 2>&1 || { echo "git is required to bootstrap the installer." >&2; exit 1; }
  BOOTSTRAP_DIR="$(mktemp -d)"
  echo "==> Bootstrapping installer into ${BOOTSTRAP_DIR}"
  git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$BOOTSTRAP_DIR/sonnenschein"
  exec bash "$BOOTSTRAP_DIR/sonnenschein/installer/install.sh" \
    --branch "$BRANCH" --prefix "$PREFIX" \
    ${SONNENSCHEIN_ASSUME_YES:+--yes}
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
# shellcheck source=lib/ui.sh
. "${SCRIPT_DIR}/lib/ui.sh"

install_error_trap

# --- Build from source ----------------------------------------------------
build_from_source() {
  local build_dir="${REPO_ROOT}/build"
  info "Configuring build in ${build_dir}"

  # Submodules are required (libdisplaydevice, etc.).
  if [ -f "${REPO_ROOT}/.gitmodules" ]; then
    git -C "$REPO_ROOT" submodule update --init --recursive
  fi

  cmake -S "$REPO_ROOT" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_DOCS=OFF \
    -DBUILD_TESTS=OFF

  info "Building (this can take a few minutes)..."
  cmake --build "$build_dir"

  info "Installing to ${PREFIX} (needs privileges)."
  require_sudo
  $SUDO cmake --install "$build_dir"

  BUILT_BINARY="${PREFIX}/bin/sonnenschein"
  export BUILT_BINARY
}

# --- Main flow ------------------------------------------------------------
main() {
  print_banner
  info "Logging to ${SONNENSCHEIN_LOG}"

  detect_distro
  detect_gpu
  detect_compositor

  ui_confirm_distro
  gpu_summary
  compositor_summary

  install_packages "${SCRIPT_DIR}/packages"
  build_from_source

  ui_choose_service_mode
  ui_choose_autostart

  apply_permissions \
    "$BUILT_BINARY" \
    "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.rules" \
    "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.conf"

  install_service "$SERVICE_MODE" "${REPO_ROOT}/build/sonnenschein.service" "$AUTOSTART"

  ui_password_notice
  success "Sonnenschein installed. Enjoy your sunshine. 🌇"
}

main "$@"
