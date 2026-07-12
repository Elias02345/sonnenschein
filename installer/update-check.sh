#!/usr/bin/env bash
#
# Sonnenschein update checker — lightweight poll run by the systemd --user
# timer. Fetches the tracked branch, records whether an update is available in
# the shared state file, and (only if AUTO_UPDATE=1) applies it via update.sh.
#
# Does NOT build anything itself — checking must stay cheap enough to run every
# few minutes. Usage: bash update-check.sh
#
set -euo pipefail

PREFIX="${PREFIX:-/opt/sonnenschein}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [ -r "${PREFIX}/install-state.env" ]; then
  # shellcheck disable=SC1091
  . "${PREFIX}/install-state.env"
  if [ -n "${SRC_DIR:-}" ] && [ -d "${SRC_DIR}/.git" ]; then
    REPO_ROOT="$SRC_DIR"
  fi
fi
BRANCH="${BRANCH:-main}"
AUTO_UPDATE="${AUTO_UPDATE:-0}"

STATE_DIR="${SONNENSCHEIN_STATE_DIR:-${HOME}/.local/state/sonnenschein}"
CHECK_FILE="${STATE_DIR}/update-state/available.json"

[ -d "${REPO_ROOT}/.git" ] || exit 0

git -C "$REPO_ROOT" fetch --quiet origin "$BRANCH" 2>/dev/null || exit 0

LOCAL="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
REMOTE="$(git -C "$REPO_ROOT" rev-parse "origin/${BRANCH}" 2>/dev/null || echo unknown)"

AVAILABLE=false
BEHIND=0
if [ "$LOCAL" != "$REMOTE" ] && [ "$REMOTE" != "unknown" ]; then
  # Only "available" if remote is actually ahead (not diverged/behind).
  if git -C "$REPO_ROOT" merge-base --is-ancestor "$LOCAL" "$REMOTE" 2>/dev/null; then
    AVAILABLE=true
    BEHIND="$(git -C "$REPO_ROOT" rev-list --count "${LOCAL}..${REMOTE}" 2>/dev/null || echo 0)"
  fi
fi

mkdir -p "$(dirname "$CHECK_FILE")" 2>/dev/null || true
{
  printf '{\n'
  printf '  "available": %s,\n' "$AVAILABLE"
  printf '  "branch": "%s",\n' "$BRANCH"
  printf '  "local": "%s",\n' "${LOCAL:0:12}"
  printf '  "remote": "%s",\n' "${REMOTE:0:12}"
  printf '  "commits_behind": %s,\n' "${BEHIND:-0}"
  printf '  "auto_update": %s\n' "$([ "$AUTO_UPDATE" = "1" ] && echo true || echo false)"
  printf '}\n'
} >"$CHECK_FILE" 2>/dev/null || true

if [ "$AVAILABLE" = "true" ] && [ "$AUTO_UPDATE" = "1" ]; then
  exec bash "${SCRIPT_DIR}/update.sh" "$BRANCH"
fi
