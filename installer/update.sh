#!/usr/bin/env bash
#
# Sonnenschein updater — pulls the latest source for the chosen branch and
# rebuilds/reinstalls in place. Re-uses the installer's build step.
#
# Usage: bash update.sh [branch]
#
set -euo pipefail

BRANCH="${1:-main}"
PREFIX="${PREFIX:-/opt/sonnenschein}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# When run from the copy under ${PREFIX}/installer, the parent is not a git
# checkout — resolve the recorded source dir instead.
if [ ! -d "${REPO_ROOT}/.git" ] && [ -r "${PREFIX}/install-state.env" ]; then
  # shellcheck disable=SC1091
  . "${PREFIX}/install-state.env"
  if [ -n "${SRC_DIR:-}" ] && [ -d "${SRC_DIR}/.git" ]; then
    REPO_ROOT="$SRC_DIR"
  fi
fi

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
install_error_trap

info "Updating Sonnenschein (branch: ${BRANCH}, prefix: ${PREFIX})"

if [ ! -d "${REPO_ROOT}/.git" ]; then
  die "Update needs a git checkout. Re-run the installer instead:
  curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash"
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
  -DSUNSHINE_EXECUTABLE_PATH="${PREFIX}/bin/sonnenschein" \
  -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF
cmake --build "${REPO_ROOT}/build"

require_sudo
$SUDO cmake --install "${REPO_ROOT}/build"
if [ -f "${REPO_ROOT}/build/install_manifest.txt" ]; then
  $SUDO install -Dm644 "${REPO_ROOT}/build/install_manifest.txt" \
    "${PREFIX}/install_manifest.txt"
fi
# Keep the standalone installer copy in sync.
$SUDO mkdir -p "${PREFIX}/installer"
$SUDO cp -r "${REPO_ROOT}/installer/." "${PREFIX}/installer/"

# A rebuilt binary loses nothing, but an OLD stale capability would still
# break PipeWire capture — make sure none survives the update.
if command -v getcap >/dev/null 2>&1; then
  REAL="$(readlink -f "${PREFIX}/bin/sonnenschein" || true)"
  if [ -n "$REAL" ] && [ -n "$(getcap "$REAL" 2>/dev/null)" ]; then
    info "Removing stale file capabilities from the updated binary."
    $SUDO setcap -r "$REAL" || true
  fi
fi

# Restart the service if it is running.
if systemctl --user is-active --quiet sonnenschein 2>/dev/null; then
  info "Restarting user service."
  systemctl --user restart sonnenschein
elif systemctl is-active --quiet sonnenschein 2>/dev/null; then
  info "Restarting system service."
  $SUDO systemctl restart sonnenschein
fi

success "Update complete ($(git -C "$REPO_ROOT" rev-parse --short HEAD) on ${BRANCH})."

# Post-update self-check: verify the whole streaming path and repair what
# broke (stale caps, stopped avahi, missing firewall rules, dead service).
info "Running post-update health check..."
if bash "${SCRIPT_DIR}/doctor.sh" --repair; then
  success "All health checks passed."
else
  warn "Some health checks failed — see output above. The update itself is applied."
fi
