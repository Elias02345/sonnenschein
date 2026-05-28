#!/usr/bin/env bash
#
# Sonnenschein post-install — re-applies permissions and (re)installs the
# service without rebuilding. Useful after a manual `cmake --install` or to
# switch the service mode.
#
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
SERVICE_MODE="${1:-user}"
AUTOSTART="${2:-1}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
# shellcheck source=lib/permissions.sh
. "${SCRIPT_DIR}/lib/permissions.sh"
# shellcheck source=lib/service.sh
. "${SCRIPT_DIR}/lib/service.sh"
install_error_trap

apply_permissions \
  "${PREFIX}/bin/sonnenschein" \
  "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.rules" \
  "${REPO_ROOT}/src_assets/linux/misc/60-sonnenschein.conf"

# Prefer the configured unit from the build dir, fall back to the template.
UNIT="${REPO_ROOT}/build/sonnenschein.service"
[ -r "$UNIT" ] || UNIT="${REPO_ROOT}/packaging/linux/sonnenschein.service.in"

install_service "$SERVICE_MODE" "$UNIT" "$AUTOSTART"
success "Post-install steps complete."
