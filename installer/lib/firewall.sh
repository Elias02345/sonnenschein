# shellcheck shell=bash
# Firewall + mDNS discovery setup. Moonlight clients need these ports
# (base 47989) and mDNS to find the host:
#   TCP 47984 (HTTPS), 47989 (HTTP), 47990 (WebUI), 48010 (RTSP)
#   UDP 47998-48000 (video/control/audio), 5353 (mDNS discovery)
# Avahi must be running for the host to announce itself on the LAN.

SONNENSCHEIN_TCP_PORTS="47984,47989,47990,48010"
SONNENSCHEIN_UDP_PORTS="47998,47999,48000,5353"

# Sets FIREWALL_CONFIGURED (ufw|firewalld|none) and FIREWALL_MDNS_ADDED.
configure_firewall() {
  FIREWALL_CONFIGURED="none"
  FIREWALL_MDNS_ADDED=0
  require_sudo

  if have ufw && $SUDO ufw status 2>/dev/null | grep -q "Status: active"; then
    info "ufw is active — opening Moonlight ports."
    $SUDO ufw allow proto tcp from any to any port "$SONNENSCHEIN_TCP_PORTS" comment 'Sonnenschein' >/dev/null \
      || warn "Could not add ufw TCP rule."
    $SUDO ufw allow proto udp from any to any port "$SONNENSCHEIN_UDP_PORTS" comment 'Sonnenschein' >/dev/null \
      || warn "Could not add ufw UDP rule."
    FIREWALL_CONFIGURED="ufw"
    success "ufw rules added (TCP ${SONNENSCHEIN_TCP_PORTS}, UDP ${SONNENSCHEIN_UDP_PORTS})."
  elif have firewall-cmd && $SUDO firewall-cmd --state >/dev/null 2>&1; then
    info "firewalld is running — opening Moonlight ports."
    local p
    for p in ${SONNENSCHEIN_TCP_PORTS//,/ }; do
      $SUDO firewall-cmd --permanent --add-port="${p}/tcp" >/dev/null || warn "firewalld: could not add ${p}/tcp"
    done
    for p in 47998 47999 48000; do
      $SUDO firewall-cmd --permanent --add-port="${p}/udp" >/dev/null || warn "firewalld: could not add ${p}/udp"
    done
    if ! $SUDO firewall-cmd --list-services 2>/dev/null | grep -qw mdns; then
      $SUDO firewall-cmd --permanent --add-service=mdns >/dev/null && FIREWALL_MDNS_ADDED=1
    fi
    $SUDO firewall-cmd --reload >/dev/null || warn "firewalld reload failed."
    FIREWALL_CONFIGURED="firewalld"
    success "firewalld rules added (TCP ${SONNENSCHEIN_TCP_PORTS}, UDP ${SONNENSCHEIN_UDP_PORTS})."
  else
    info "No active firewall detected (ufw/firewalld) — no rules needed."
  fi
  export FIREWALL_CONFIGURED FIREWALL_MDNS_ADDED
}

# Reverts exactly what configure_firewall added. Reads the mode + mdns flag
# from arguments (recorded in install-state.env).
remove_firewall_rules() {
  # remove_firewall_rules MODE MDNS_ADDED
  local mode="${1:-none}"
  local mdns_added="${2:-0}"
  [ "$mode" = "none" ] && return 0
  require_sudo

  case "$mode" in
    ufw)
      $SUDO ufw delete allow proto tcp from any to any port "$SONNENSCHEIN_TCP_PORTS" >/dev/null 2>&1 || true
      $SUDO ufw delete allow proto udp from any to any port "$SONNENSCHEIN_UDP_PORTS" >/dev/null 2>&1 || true
      info "ufw rules removed."
      ;;
    firewalld)
      local p
      for p in ${SONNENSCHEIN_TCP_PORTS//,/ }; do
        $SUDO firewall-cmd --permanent --remove-port="${p}/tcp" >/dev/null 2>&1 || true
      done
      for p in 47998 47999 48000; do
        $SUDO firewall-cmd --permanent --remove-port="${p}/udp" >/dev/null 2>&1 || true
      done
      if [ "$mdns_added" = "1" ]; then
        $SUDO firewall-cmd --permanent --remove-service=mdns >/dev/null 2>&1 || true
      fi
      $SUDO firewall-cmd --reload >/dev/null 2>&1 || true
      info "firewalld rules removed."
      ;;
  esac
}

# Moonlight discovery: the host announces itself via mDNS through Avahi.
# Sets AVAHI_ENABLED=1 only if WE turned the daemon on (for clean uninstall).
ensure_avahi() {
  AVAHI_ENABLED=0
  if systemctl is-active --quiet avahi-daemon 2>/dev/null; then
    info "avahi-daemon already running (mDNS discovery works)."
  elif have systemctl; then
    require_sudo
    info "Enabling avahi-daemon (Moonlight clients discover the host via mDNS)."
    if $SUDO systemctl enable --now avahi-daemon 2>/dev/null; then
      AVAHI_ENABLED=1
      success "avahi-daemon enabled."
    else
      warn "Could not enable avahi-daemon — Moonlight clients must add this host by IP."
    fi
  fi
  export AVAHI_ENABLED
}
