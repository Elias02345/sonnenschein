# shellcheck shell=bash
# Wake-on-LAN setup (Boot-to-Ready stage 2, see docs/ROADMAP.md).
# Arms the primary wired NIC for magic packets and persists the setting
# across reboots with a small oneshot systemd unit. Moonlight clients can
# then wake the host from suspend/soft-off before streaming.
#
# WoL also needs to be enabled in the BIOS/UEFI ("Wake on PCI-E/LAN") —
# we can only arm the OS side and tell the user about the rest.

WOL_UNIT=/etc/systemd/system/sonnenschein-wol.service

# Best-effort: pick the interface of the default route if it is ethernet.
_wol_primary_nic() {
  local dev
  dev="$(ip route show default 2>/dev/null | sed -n 's/.* dev \([^ ]*\).*/\1/p' | head -n1)"
  [ -n "$dev" ] || return 1
  # Skip wireless and virtual interfaces — WoL is a wired-NIC feature.
  case "$dev" in
    wl* | ww* | tun* | tap* | veth* | docker* | br-* | virbr*) return 1 ;;
  esac
  printf '%s' "$dev"
}

# Sets WOL_CONFIGURED (iface name or empty) and WOL_MAC for the summary.
configure_wol() {
  WOL_CONFIGURED=""
  WOL_MAC=""

  if ! have ethtool; then
    warn "ethtool not available — skipping Wake-on-LAN setup."
    export WOL_CONFIGURED WOL_MAC
    return 0
  fi

  local nic
  if ! nic="$(_wol_primary_nic)"; then
    info "No wired default-route NIC found — skipping Wake-on-LAN setup."
    export WOL_CONFIGURED WOL_MAC
    return 0
  fi

  require_sudo
  local supports
  supports="$($SUDO ethtool "$nic" 2>/dev/null | sed -n 's/.*Supports Wake-on: \(.*\)/\1/p')"
  if [ -z "$supports" ] || [ "${supports#*g}" = "$supports" ]; then
    info "NIC ${nic} does not support magic-packet wake (Supports Wake-on: ${supports:-unknown}) — skipping."
    export WOL_CONFIGURED WOL_MAC
    return 0
  fi

  if ! $SUDO ethtool -s "$nic" wol g 2>/dev/null; then
    warn "Could not enable Wake-on-LAN on ${nic}."
    export WOL_CONFIGURED WOL_MAC
    return 0
  fi

  # Persist across reboots (drivers often reset wol on boot).
  local ethtool_bin
  ethtool_bin="$(command -v ethtool)"
  $SUDO tee "$WOL_UNIT" >/dev/null <<UNIT
[Unit]
Description=Sonnenschein: arm Wake-on-LAN on ${nic}
After=network-pre.target

[Service]
Type=oneshot
ExecStart=${ethtool_bin} -s ${nic} wol g

[Install]
WantedBy=multi-user.target
UNIT
  $SUDO systemctl daemon-reload 2>/dev/null || true
  $SUDO systemctl enable sonnenschein-wol.service >/dev/null 2>&1 || warn "Could not enable the WoL persistence unit."

  WOL_CONFIGURED="$nic"
  WOL_MAC="$(cat "/sys/class/net/${nic}/address" 2>/dev/null || true)"
  success "Wake-on-LAN armed on ${nic}${WOL_MAC:+ (MAC ${WOL_MAC})} — also enable 'Wake on LAN' in your BIOS/UEFI."
  export WOL_CONFIGURED WOL_MAC
}

remove_wol() {
  # remove_wol IFACE (recorded in install-state.env; empty = nothing to do)
  local nic="${1:-}"
  [ -n "$nic" ] || return 0
  require_sudo
  $SUDO systemctl disable sonnenschein-wol.service >/dev/null 2>&1 || true
  $SUDO rm -f "$WOL_UNIT"
  $SUDO systemctl daemon-reload 2>/dev/null || true
  # Leave the NIC's current wol setting alone — turning it off could
  # break other tools the user relies on; removing persistence is enough.
  info "Wake-on-LAN persistence unit removed."
}
