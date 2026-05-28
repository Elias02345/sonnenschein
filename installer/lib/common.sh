# shellcheck shell=bash
# Common helpers: logging, colors, user interaction, error handling.
# Sourced by install.sh and the other entry-point scripts.

# Guard against double-sourcing.
if [ -n "${SONNENSCHEIN_COMMON_SOURCED:-}" ]; then
  return 0
fi
SONNENSCHEIN_COMMON_SOURCED=1

# --- Colors (disabled when not a TTY) -------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
  C_RESET=$'\033[0m'
  C_RED=$'\033[31m'
  C_GREEN=$'\033[32m'
  C_YELLOW=$'\033[33m'
  C_BLUE=$'\033[34m'
  C_BOLD=$'\033[1m'
else
  C_RESET='' C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_BOLD=''
fi

# --- Logging --------------------------------------------------------------
: "${SONNENSCHEIN_LOG:=/tmp/sonnenschein-install.log}"

_log() {
  # _log LEVEL MESSAGE...
  local level="$1"
  shift
  printf '[%s] %s: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$level" "$*" \
    >>"$SONNENSCHEIN_LOG" 2>/dev/null || true
}

info() {
  printf '%s==>%s %s\n' "$C_BLUE$C_BOLD" "$C_RESET" "$*"
  _log INFO "$*"
}

success() {
  printf '%s✓%s %s\n' "$C_GREEN$C_BOLD" "$C_RESET" "$*"
  _log INFO "$*"
}

warn() {
  printf '%s!%s %s\n' "$C_YELLOW$C_BOLD" "$C_RESET" "$*" >&2
  _log WARN "$*"
}

error() {
  printf '%sError:%s %s\n' "$C_RED$C_BOLD" "$C_RESET" "$*" >&2
  _log ERROR "$*"
}

die() {
  error "$*"
  exit 1
}

# --- Error trap -----------------------------------------------------------
# Call `install_error_trap` after sourcing to get a helpful failure message.
_on_error() {
  local exit_code=$?
  local line=${1:-?}
  error "Aborted on line ${line} (exit ${exit_code})."
  error "See the full log at: ${SONNENSCHEIN_LOG}"
  exit "$exit_code"
}

install_error_trap() {
  trap '_on_error "$LINENO"' ERR
}

# --- User interaction -----------------------------------------------------
# Non-interactive mode skips every prompt and uses the supplied defaults.
: "${SONNENSCHEIN_ASSUME_YES:=0}"

confirm() {
  # confirm "Question?" [default:Y|N]  -> returns 0 for yes, 1 for no
  local prompt="$1"
  local default="${2:-Y}"
  local reply

  if [ "$SONNENSCHEIN_ASSUME_YES" = "1" ]; then
    return 0
  fi

  local hint="[Y/n]"
  [ "$default" = "N" ] && hint="[y/N]"

  printf '%s %s ' "$prompt" "$hint"
  read -r reply || reply=""
  reply="${reply:-$default}"

  case "$reply" in
    [Yy]*) return 0 ;;
    *) return 1 ;;
  esac
}

choose() {
  # choose "Prompt" default_index option1 option2 ...
  # Echoes the chosen option to stdout.
  local prompt="$1"
  local default_index="$2"
  shift 2
  local options=("$@")
  local i reply

  if [ "$SONNENSCHEIN_ASSUME_YES" = "1" ]; then
    printf '%s\n' "${options[$((default_index - 1))]}"
    return 0
  fi

  printf '%s\n' "$prompt" >&2
  for i in "${!options[@]}"; do
    printf '  %d) %s\n' "$((i + 1))" "${options[$i]}" >&2
  done
  printf 'Choice [%s]: ' "$default_index" >&2
  read -r reply || reply=""
  reply="${reply:-$default_index}"

  if ! [ "$reply" -ge 1 ] 2>/dev/null || [ "$reply" -gt "${#options[@]}" ]; then
    reply="$default_index"
  fi
  printf '%s\n' "${options[$((reply - 1))]}"
}

# --- Privilege helpers ----------------------------------------------------
# Resolve a command for running privileged operations without forcing the
# whole script to run as root (we want the service installed for the user).
require_sudo() {
  # SUDO is consumed by the package/service/permission helpers.
  export SUDO
  if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
    return 0
  fi
  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
    return 0
  fi
  die "This step needs root privileges but 'sudo' was not found. Re-run as root."
}

have() {
  command -v "$1" >/dev/null 2>&1
}
