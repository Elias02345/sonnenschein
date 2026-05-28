# shellcheck shell=bash
# Compositor / session detection. Sets SESSION_TYPE (wayland|x11|tty),
# COMPOSITOR (kwin|mutter|wlroots|hyprland|xorg|unknown) and a support tier.

detect_compositor() {
  SESSION_TYPE="${XDG_SESSION_TYPE:-unknown}"
  COMPOSITOR="unknown"
  COMPOSITOR_TIER="unknown"

  local desktop
  desktop="$(printf '%s' "${XDG_CURRENT_DESKTOP:-}${DESKTOP_SESSION:-}" | tr '[:upper:]' '[:lower:]')"

  case "$desktop" in
    *kde* | *plasma*)
      COMPOSITOR="kwin"
      COMPOSITOR_TIER="first-class"
      ;;
    *gnome*)
      COMPOSITOR="mutter"
      COMPOSITOR_TIER="first-class"
      ;;
    *hyprland*)
      COMPOSITOR="hyprland"
      COMPOSITOR_TIER="best-effort"
      ;;
    *sway* | *wlroots*)
      COMPOSITOR="wlroots"
      COMPOSITOR_TIER="first-class"
      ;;
    *)
      if [ "$SESSION_TYPE" = "x11" ]; then
        COMPOSITOR="xorg"
        COMPOSITOR_TIER="fallback"
      fi
      ;;
  esac

  export SESSION_TYPE COMPOSITOR COMPOSITOR_TIER
}

compositor_summary() {
  info "Session type: ${C_BOLD}${SESSION_TYPE}${C_RESET}"
  info "Compositor: ${COMPOSITOR} (${COMPOSITOR_TIER} support)"

  if [ "$SESSION_TYPE" != "wayland" ]; then
    warn "Sonnenschein targets Wayland first. X11 is a fallback path only."
  fi
  if [ "$COMPOSITOR" = "kwin" ]; then
    info "KWin is the main development target — virtual-display auto-match works best here."
  fi
}
