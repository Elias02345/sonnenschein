#!/usr/bin/env bash
#
# Sonnenschein uninstaller — removes the binary, services, udev/modules rules
# and (optionally) the user configuration.
#
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=lib/common.sh
. "${SCRIPT_DIR}/lib/common.sh"
install_error_trap

info "Uninstalling Sonnenschein"

# Stop and disable services.
if systemctl --user list-unit-files 2>/dev/null | grep -q '^sonnenschein\.service'; then
  systemctl --user disable --now sonnenschein.service 2>/dev/null || true
  rm -f "${HOME}/.config/systemd/user/sonnenschein.service"
  systemctl --user daemon-reload 2>/dev/null || true
fi

if systemctl list-unit-files 2>/dev/null | grep -q '^sonnenschein\.service'; then
  require_sudo
  $SUDO systemctl disable --now sonnenschein.service 2>/dev/null || true
  $SUDO rm -f /etc/systemd/system/sonnenschein.service
  $SUDO systemctl daemon-reload 2>/dev/null || true
fi

# Remove binary + compat symlink.
require_sudo
$SUDO rm -f "${PREFIX}/bin/sonnenschein" "${PREFIX}/bin/sunshine"
$SUDO rm -rf "${PREFIX}/share/sonnenschein"

# Remove input rules.
$SUDO rm -f /etc/udev/rules.d/60-sonnenschein.rules
$SUDO rm -f /etc/modules-load.d/60-sonnenschein.conf
have udevadm && $SUDO udevadm control --reload-rules || true

# Optionally drop user config/state.
if confirm "Also remove your configuration and state (~/.config/sunshine)?" N; then
  rm -rf "${HOME}/.config/sunshine"
  success "Configuration removed."
else
  info "Configuration kept at ~/.config/sunshine."
fi

success "Sonnenschein uninstalled."
