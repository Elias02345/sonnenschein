# shellcheck shell=bash
# GPU detection. Sets GPU_VENDOR (nvidia|amd|intel|unknown) and GPU_NAME.
# Used to pick the right encoder packages and warn about driver gaps.

detect_gpu() {
  GPU_VENDOR="unknown"
  GPU_NAME="Unknown GPU"

  local line=""
  if have lspci; then
    line="$(lspci -mm 2>/dev/null | grep -iE 'vga|3d|display' | head -n1 || true)"
  fi

  case "$(printf '%s' "$line" | tr '[:upper:]' '[:lower:]')" in
    *nvidia*)
      GPU_VENDOR="nvidia"
      ;;
    *amd* | *"advanced micro devices"* | *radeon* | *ati*)
      GPU_VENDOR="amd"
      ;;
    *intel*)
      GPU_VENDOR="intel"
      ;;
  esac

  if [ -n "$line" ]; then
    GPU_NAME="$line"
  fi

  export GPU_VENDOR GPU_NAME
}

gpu_summary() {
  info "Detected GPU vendor: ${C_BOLD}${GPU_VENDOR}${C_RESET}"
  case "$GPU_VENDOR" in
    nvidia)
      warn "NVIDIA: driver 550+ required, 580+ recommended for Wayland HDR."
      ;;
    amd)
      info "AMD: Mesa 25+ recommended for HDR (RADV/VAAPI)."
      ;;
    intel)
      warn "Intel: encoding support is experimental; AV1 needs Arc Battlemage."
      ;;
    *)
      warn "Could not identify the GPU vendor — you may need to pick encoder packages manually."
      ;;
  esac
}
