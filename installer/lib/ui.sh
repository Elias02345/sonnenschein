# shellcheck shell=bash
# Text-UI flow: banner and the guided questions described in the README
# (distro confirmation, GPU, service mode, autostart, WebUI password).

print_banner() {
  printf '%s' "$C_YELLOW$C_BOLD"
  cat <<'BANNER'
   ____                                       _          _
  / ___|  ___  _ __  _ __   ___ _ __  ___  ___| |__   ___(_)_ __
  \___ \ / _ \| '_ \| '_ \ / _ \ '_ \/ __|/ __| '_ \ / _ \ | '_ \
   ___) | (_) | | | | | | |  __/ | | \__ \ (__| | | |  __/ | | | |
  |____/ \___/|_| |_|_| |_|\___|_| |_|___/\___|_| |_|\___|_|_| |_|
BANNER
  printf '%s' "$C_RESET"
  printf '  Linux Moonlight host with auto-matching virtual displays\n\n'
}

# Guided prompts. Each writes its answer into a named global.
ui_confirm_distro() {
  distro_summary
  if ! confirm "Continue installing for ${DISTRO_NAME}?" Y; then
    die "Installation cancelled by user."
  fi
}

ui_choose_service_mode() {
  SERVICE_MODE="$(choose "How should Sonnenschein run?" 1 \
    "user (recommended — full Wayland/DBus session access)" \
    "system (advanced — needs extra session wiring)")"
  case "$SERVICE_MODE" in
    user*) SERVICE_MODE="user" ;;
    *) SERVICE_MODE="system" ;;
  esac
  export SERVICE_MODE
}

ui_choose_autostart() {
  if confirm "Start Sonnenschein automatically on login/boot?" Y; then
    AUTOSTART=1
  else
    AUTOSTART=0
  fi
  export AUTOSTART
}

# The WebUI sets its own password on first launch via the browser; we only
# point the user at it here. Kept as a hook for a future headless flow.
ui_password_notice() {
  info "On first launch, open the WebUI to set your admin password:"
  printf '      %shttps://localhost:47990%s\n' "$C_BOLD" "$C_RESET"
}
