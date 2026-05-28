#!/usr/bin/env bash
#
# Sonnenschein updater — pulls the latest source for the chosen branch and
# rebuilds/reinstalls in place. Re-uses the installer's build step.
#
set -euo pipefail

BRANCH="${1:-main}"
PREFIX="${PREFIX:-/usr/local}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
install_error_trap

info "Updating Sonnenschein (branch: ${BRANCH})"

if [ ! -d "${REPO_ROOT}/.git" ]; then
  die "Update needs a git checkout. Re-run installer/install.sh instead."
fi

info "Fetching latest changes..."
git -C "$REPO_ROOT" fetch origin "$BRANCH"
git -C "$REPO_ROOT" checkout "$BRANCH"
git -C "$REPO_ROOT" pull --ff-only origin "$BRANCH"
git -C "$REPO_ROOT" submodule update --init --recursive

info "Rebuilding..."
cmake -S "$REPO_ROOT" -B "${REPO_ROOT}/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF
cmake --build "${REPO_ROOT}/build"

require_sudo
$SUDO cmake --install "${REPO_ROOT}/build"

# Restart the service if it is running.
if systemctl --user is-active --quiet sonnenschein 2>/dev/null; then
  info "Restarting user service."
  systemctl --user restart sonnenschein
elif systemctl is-active --quiet sonnenschein 2>/dev/null; then
  info "Restarting system service."
  $SUDO systemctl restart sonnenschein
fi

success "Update complete."
