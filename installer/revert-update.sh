#!/usr/bin/env bash
#
# Sonnenschein manual rollback — rebuilds a previous commit and restores the
# matching config backup. For operator use when an update went bad and the
# automatic rollback in update.sh did not run (or you want an older version).
#
# Usage: bash revert-update.sh [commit]
#   commit  target git ref; defaults to previous_commit from the update state.
#
set -euo pipefail

PREFIX="${PREFIX:-/opt/sonnenschein}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [ ! -d "${REPO_ROOT}/.git" ] && [ -r "${PREFIX}/install-state.env" ]; then
  # shellcheck disable=SC1091
  . "${PREFIX}/install-state.env"
  if [ -n "${SRC_DIR:-}" ] && [ -d "${SRC_DIR}/.git" ]; then
    REPO_ROOT="$SRC_DIR"
  fi
fi

# shellcheck source=lib/common.sh
# shellcheck disable=SC1091
. "${SCRIPT_DIR}/lib/common.sh"
install_error_trap

STATE_DIR="${SONNENSCHEIN_STATE_DIR:-${HOME}/.local/state/sonnenschein}"
STATE_FILE="${STATE_DIR}/update-state/status.json"
BACKUP_ROOT="${STATE_DIR}/backups"
CONFIG_DIR="${SONNENSCHEIN_CONFIG_DIR:-${HOME}/.config/sunshine}"

[ -d "${REPO_ROOT}/.git" ] || die "No git checkout at ${REPO_ROOT}. Re-run the installer."

TARGET="${1:-}"
if [ -z "$TARGET" ] && [ -r "$STATE_FILE" ]; then
  # Pull previous_commit out of the JSON without a JSON parser.
  TARGET="$(sed -n 's/.*"previous_commit": *"\([^"]*\)".*/\1/p' "$STATE_FILE" | head -n1)"
fi
[ -n "$TARGET" ] || die "No target commit given and none recorded. Usage: revert-update.sh <commit>"

info "Reverting Sonnenschein to ${TARGET}"
confirm "Rebuild and install commit ${TARGET}? This restarts the service." Y || die "Aborted."

git -C "$REPO_ROOT" checkout "$TARGET"
git -C "$REPO_ROOT" submodule update --init --recursive

info "Rebuilding ${TARGET}..."
cmake -S "$REPO_ROOT" -B "${REPO_ROOT}/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DSUNSHINE_EXECUTABLE_PATH="${PREFIX}/bin/sonnenschein" \
  -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF
cmake --build "${REPO_ROOT}/build"

require_sudo
$SUDO cmake --install "${REPO_ROOT}/build"
$SUDO mkdir -p "${PREFIX}/installer"
$SUDO cp -r "${REPO_ROOT}/installer/." "${PREFIX}/installer/"

# Strip any stale capability (would break PipeWire capture).
if command -v getcap >/dev/null 2>&1; then
  REAL="$(readlink -f "${PREFIX}/bin/sonnenschein" || true)"
  if [ -n "$REAL" ] && [ -n "$(getcap "$REAL" 2>/dev/null)" ]; then
    $SUDO setcap -r "$REAL" || true
  fi
fi

# Restore the most recent config backup, if the operator wants it.
# ls -t sorts by mtime (find cannot portably); timestamped names make it safe.
# shellcheck disable=SC2012
LATEST_BACKUP="$(ls -1dt "${BACKUP_ROOT}"/*/ 2>/dev/null | head -n1 || true)"
if [ -n "$LATEST_BACKUP" ] && [ -d "$LATEST_BACKUP" ]; then
  if confirm "Restore config from the latest backup (${LATEST_BACKUP})?" Y; then
    mkdir -p "$CONFIG_DIR"
    cp -a "${LATEST_BACKUP}." "$CONFIG_DIR/" 2>/dev/null || warn "Config restore incomplete."
    success "Config restored from ${LATEST_BACKUP}"
  fi
fi

if systemctl --user is-active --quiet sonnenschein 2>/dev/null; then
  systemctl --user restart sonnenschein
elif systemctl is-active --quiet sonnenschein 2>/dev/null; then
  $SUDO systemctl restart sonnenschein
fi

success "Reverted to ${TARGET} ($(git -C "$REPO_ROOT" rev-parse --short HEAD))."
