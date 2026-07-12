# shellcheck shell=bash
# Automatic update daemon (Phase 6). Installs a systemd --user timer that runs
# the lightweight update-check every 10 minutes. By default it only records
# whether an update is available (surfaced in the WebUI); it applies updates
# automatically only when AUTO_UPDATE=1 is set in install-state.env.
#
# --user (not system) because the checker reuses the service's git checkout and
# the same state dir the app writes to.

# configure_updater PREFIX  — install + enable the check timer for this user.
configure_updater() {
  local prefix="$1"
  local dest="${HOME}/.config/systemd/user"
  mkdir -p "$dest"

  cat >"${dest}/sonnenschein-update.service" <<UNIT
[Unit]
Description=Sonnenschein: check for updates
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
Environment=PREFIX=${prefix}
ExecStart=/usr/bin/env bash ${prefix}/installer/update-check.sh
UNIT

  cat >"${dest}/sonnenschein-update.timer" <<'UNIT'
[Unit]
Description=Sonnenschein: periodic update check

[Timer]
# First check shortly after login, then every 10 minutes.
OnBootSec=2min
OnUnitActiveSec=10min
Persistent=true

[Install]
WantedBy=timers.target
UNIT

  systemctl --user daemon-reload 2>/dev/null || true
  if systemctl --user enable --now sonnenschein-update.timer >/dev/null 2>&1; then
    success "Automatic update check enabled (every 10 min). Set AUTO_UPDATE=1 to auto-apply."
    return 0
  fi
  warn "Could not enable the update timer (no --user systemd session?)."
  return 1
}

remove_updater() {
  local dest="${HOME}/.config/systemd/user"
  systemctl --user disable --now sonnenschein-update.timer >/dev/null 2>&1 || true
  rm -f "${dest}/sonnenschein-update.timer" "${dest}/sonnenschein-update.service"
  systemctl --user daemon-reload 2>/dev/null || true
  info "Automatic update timer removed."
}
