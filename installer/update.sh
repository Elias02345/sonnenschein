#!/usr/bin/env bash
#
# Sonnenschein updater — pulls the latest source for the chosen branch and
# rebuilds/reinstalls in place. Backs up config first, and rolls back
# automatically if the post-update health check fails. Writes a JSON state
# file the WebUI can poll.
#
# Usage: bash update.sh [branch]
#   SONNENSCHEIN_SELFTEST=1 bash update.sh   # offline logic self-check, no changes
#
set -euo pipefail

# The updater MUST run in the invoking user's session, not as root:
#   - the source checkout is owned by the user (root git ops hit "dubious
#     ownership"),
#   - the service is a `systemctl --user` unit (invisible to root),
#   - config/state live under the user's $HOME,
#   - the post-update health check (doctor.sh) probes the Wayland session.
# Running the whole thing as root breaks the service restart and fails the
# Wayland health check, which triggers a needless auto-rollback. So if we were
# started with sudo, re-run as the invoking user and elevate only for the
# install step (via sudo internally). This makes `sudo bash update.sh` and
# `bash update.sh` behave identically.
if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != "root" ]; then
  _sns_uid="$(id -u "$SUDO_USER" 2>/dev/null || true)"
  echo "==> Started with sudo; re-running as ${SUDO_USER} (the updater elevates for install itself)."
  exec sudo -u "$SUDO_USER" env "XDG_RUNTIME_DIR=/run/user/${_sns_uid}" \
    bash "$0" "$@"
fi

# Restore the graphical session env for the health check when it was stripped
# from the inherited environment (e.g. a fresh sudo shell that got re-execed
# above, or a bare login shell). KDE/Plasma exports these into the user's
# systemd manager environment.
if [ -z "${WAYLAND_DISPLAY:-}" ] && command -v systemctl >/dev/null 2>&1; then
  while IFS='=' read -r _k _v; do
    case "$_k" in
      WAYLAND_DISPLAY|DISPLAY|DBUS_SESSION_BUS_ADDRESS)
        [ -n "${_v:-}" ] && export "${_k}=${_v}" ;;
    esac
  done < <(systemctl --user show-environment 2>/dev/null || true)
fi

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
# shellcheck disable=SC1091
. "${SCRIPT_DIR}/lib/common.sh"

# --- State + backup locations (user-writable, next to the app's other state) --
STATE_DIR="${SONNENSCHEIN_STATE_DIR:-${HOME}/.local/state/sonnenschein}"
STATE_FILE="${STATE_DIR}/update-state/status.json"
BACKUP_ROOT="${STATE_DIR}/backups"
CONFIG_DIR="${SONNENSCHEIN_CONFIG_DIR:-${HOME}/.config/sunshine}"

# JSON-escape a string value (backslash, quote, control chars).
_json_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  s="${s//$'\n'/\\n}"
  s="${s//$'\t'/\\t}"
  printf '%s' "$s"
}

# write_state PHASE OK MESSAGE [extra-json]
#   PHASE   = checking|backup|building|restarting|success|rolled_back|failed
#   OK      = true|false
write_state() {
  local phase="$1" ok="$2" msg="$3" extra="${4:-}"
  mkdir -p "$(dirname "$STATE_FILE")" 2>/dev/null || return 0
  {
    printf '{\n'
    printf '  "phase": "%s",\n' "$(_json_escape "$phase")"
    printf '  "ok": %s,\n' "$ok"
    printf '  "branch": "%s",\n' "$(_json_escape "$BRANCH")"
    printf '  "message": "%s",\n' "$(_json_escape "$msg")"
    printf '  "commit": "%s",\n' "$(_json_escape "${NEW_COMMIT:-}")"
    printf '  "previous_commit": "%s"' "$(_json_escape "${PRE_COMMIT:-}")"
    [ -n "$extra" ] && printf ',\n%s' "$extra"
    printf '\n}\n'
  } >"$STATE_FILE" 2>/dev/null || true
}

# --- Offline self-check ------------------------------------------------------
# Exercises the JSON escaping + state writer without touching the system.
if [ "${SONNENSCHEIN_SELFTEST:-0}" = "1" ]; then
  tmp="$(mktemp -d)"
  STATE_FILE="${tmp}/status.json"
  PRE_COMMIT="abc123"; NEW_COMMIT="def456"
  write_state "success" "true" 'done "quoted" & <ok>'
  grep -q '"phase": "success"' "$STATE_FILE" || { echo "SELFTEST FAIL: phase"; exit 1; }
  grep -q '"previous_commit": "abc123"' "$STATE_FILE" || { echo "SELFTEST FAIL: prev"; exit 1; }
  grep -q 'done \\"quoted\\"' "$STATE_FILE" || { echo "SELFTEST FAIL: escape"; exit 1; }
  esc="$(_json_escape 'a"b\c')"
  [ "$esc" = 'a\"b\\c' ] || { echo "SELFTEST FAIL: escape fn ($esc)"; exit 1; }
  rm -rf "$tmp"
  echo "SELFTEST OK"
  exit 0
fi

install_error_trap

info "Updating Sonnenschein (branch: ${BRANCH}, prefix: ${PREFIX})"

if [ ! -d "${REPO_ROOT}/.git" ]; then
  write_state "failed" "false" "No git checkout — reinstall via the installer."
  die "Update needs a git checkout. Re-run the installer instead:
  curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash"
fi

git_head() { git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown; }

PRE_COMMIT="$(git_head)"
NEW_COMMIT=""
write_state "checking" "true" "Fetching latest changes..."

info "Fetching latest changes..."
git -C "$REPO_ROOT" fetch origin "$BRANCH"

# --- Backup config before we touch anything ---------------------------------
BACKUP_DIR=""
if [ -d "$CONFIG_DIR" ]; then
  write_state "backup" "true" "Backing up configuration..."
  BACKUP_DIR="${BACKUP_ROOT}/$(date +%Y%m%d-%H%M%S)-${PRE_COMMIT}"
  mkdir -p "$BACKUP_DIR"
  cp -a "$CONFIG_DIR/." "$BACKUP_DIR/" 2>/dev/null || warn "Config backup incomplete (non-fatal)."
  info "Config backed up to ${BACKUP_DIR}"
  # Keep only the 5 most recent backups. ls -t sorts by mtime, which find
  # cannot do portably; timestamped dir names make this safe.
  # shellcheck disable=SC2012
  ls -1dt "${BACKUP_ROOT}"/*/ 2>/dev/null | tail -n +6 | while read -r old; do
    rm -rf "$old" 2>/dev/null || true
  done
fi

# build_and_install COMMITISH — checkout, build, install. Returns build status.
build_and_install() {
  local ref="$1"
  git -C "$REPO_ROOT" checkout "$ref"
  git -C "$REPO_ROOT" submodule update --init --recursive

  # Reclaim the build tree if a previous run left root-owned artifacts (e.g. an
  # old `sudo bash update.sh`, or the install_manifest.txt that `sudo cmake
  # --install` writes). A user-context build — especially Vite's emptyOutDir —
  # cannot delete/overwrite root-owned files and fails with EACCES. chown keeps
  # the build cache intact (vs. a full rm -rf).
  if [ -d "${REPO_ROOT}/build" ] && \
     find "${REPO_ROOT}/build" ! -user "$(id -un)" -print -quit 2>/dev/null | grep -q .; then
    warn "Build dir has files from an earlier root run — reclaiming ownership."
    require_sudo
    $SUDO chown -R "$(id -un):$(id -gn)" "${REPO_ROOT}/build" 2>/dev/null || true
  fi

  cmake -S "$REPO_ROOT" -B "${REPO_ROOT}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSUNSHINE_EXECUTABLE_PATH="${PREFIX}/bin/sonnenschein" \
    -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF
  cmake --build "${REPO_ROOT}/build"
  require_sudo
  $SUDO cmake --install "${REPO_ROOT}/build"
  if [ -f "${REPO_ROOT}/build/install_manifest.txt" ]; then
    $SUDO install -Dm644 "${REPO_ROOT}/build/install_manifest.txt" "${PREFIX}/install_manifest.txt"
    # `cmake --install` wrote install_manifest.txt as root; hand it back so the
    # next user-context build doesn't trip over a root-owned file.
    $SUDO chown "$(id -un):$(id -gn)" "${REPO_ROOT}/build/install_manifest.txt" 2>/dev/null || true
  fi
  $SUDO mkdir -p "${PREFIX}/installer"
  $SUDO cp -r "${REPO_ROOT}/installer/." "${PREFIX}/installer/"
  # A stale capability would break PipeWire capture — strip it after every build.
  if command -v getcap >/dev/null 2>&1; then
    local real
    real="$(readlink -f "${PREFIX}/bin/sonnenschein" || true)"
    if [ -n "$real" ] && [ -n "$(getcap "$real" 2>/dev/null)" ]; then
      $SUDO setcap -r "$real" || true
    fi
  fi
}

restart_service() {
  if systemctl --user is-active --quiet sonnenschein 2>/dev/null; then
    systemctl --user restart sonnenschein
  elif systemctl is-active --quiet sonnenschein 2>/dev/null; then
    require_sudo
    $SUDO systemctl restart sonnenschein
  fi
}

health_ok() {
  # doctor.sh --repair returns non-zero if checks fail after repair attempts.
  bash "${SCRIPT_DIR}/doctor.sh" --repair
}

rollback() {
  local reason="$1"
  warn "Update failed (${reason}) — rolling back to ${PRE_COMMIT}."
  write_state "rolled_back" "false" "Health check failed; rolling back to ${PRE_COMMIT}."
  if build_and_install "$PRE_COMMIT"; then
    # Restore config from the backup we took before the update.
    if [ -n "$BACKUP_DIR" ] && [ -d "$BACKUP_DIR" ]; then
      mkdir -p "$CONFIG_DIR"
      cp -a "$BACKUP_DIR/." "$CONFIG_DIR/" 2>/dev/null || warn "Config restore incomplete."
    fi
    restart_service
    NEW_COMMIT="$(git_head)"
    write_state "rolled_back" "true" "Rolled back to ${PRE_COMMIT} after a failed update."
    error "Update rolled back. The previous version (${PRE_COMMIT}) is running again."
  else
    write_state "failed" "false" "Rollback build failed — manual recovery needed (revert-update.sh)."
    die "Rollback build failed. Recover manually: bash ${SCRIPT_DIR}/revert-update.sh ${PRE_COMMIT}"
  fi
  exit 1
}

# --- Pull + rebuild ----------------------------------------------------------
write_state "building" "true" "Building the new version..."
info "Rebuilding..."
git -C "$REPO_ROOT" pull --ff-only origin "$BRANCH"
build_and_install "$BRANCH"
NEW_COMMIT="$(git_head)"

write_state "restarting" "true" "Restarting the service..."
info "Restarting the service..."
restart_service

# --- Health check → rollback on failure -------------------------------------
info "Running post-update health check..."
if health_ok; then
  write_state "success" "true" "Updated to ${NEW_COMMIT} on ${BRANCH}."
  success "Update complete (${NEW_COMMIT} on ${BRANCH}). All health checks passed."
else
  rollback "post-update health check failed"
fi
