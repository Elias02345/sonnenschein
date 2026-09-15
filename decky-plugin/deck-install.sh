#!/bin/bash
# Sonnenschein Decky plugin — clean install + diagnosis for the Steam Deck.
#
# Run in Desktop Mode (Konsole):
#   curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/dev/decky-plugin/deck-install.sh | bash
#
# What it does: stops Decky, removes any old Sonnenschein plugin remains
# (including stale __pycache__/root-owned files from sudo-unzips), fetches
# the latest release zip and client AppImage, installs both, restarts
# Decky, and then PROVES whether the backend is running — showing the
# exact Python traceback from the Decky log if it is not.
set -uo pipefail

REPO="Elias02345/sonnenschein"
INSTALL_USER="${SONNENSCHEIN_DECK_USER:-$(id -un)}"
INSTALL_GROUP="${SONNENSCHEIN_DECK_GROUP:-$(id -gn)}"
PLUGINS_DIR="$HOME/homebrew/plugins"
PLUGIN_DIR="$PLUGINS_DIR/sonnenschein"
LOGS_DIR="$HOME/homebrew/logs/sonnenschein"
DECKY_STOPPED=0
TMPZIP=""
TMPAPPIMAGE=""

cleanup() {
    [ -z "$TMPZIP" ] || rm -f "$TMPZIP"
    [ -z "$TMPAPPIMAGE" ] || rm -f "$TMPAPPIMAGE"
    if [ "$DECKY_STOPPED" -eq 1 ]; then
        sudo systemctl start plugin_loader >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '\033[32m[OK]\033[0m %s\n' "$*"; }
err()  { printf '\033[31m[ERROR]\033[0m %s\n' "$*"; }
info() { printf '\033[36m[INFO]\033[0m %s\n' "$*"; }

if [ ! -d "$PLUGINS_DIR" ]; then
    err "Decky Loader not found ($PLUGINS_DIR missing). Install it from https://decky.xyz first."
    exit 1
fi

bold "== Sonnenschein Decky plugin: clean install =="
info "sudo is required to restart Decky Loader — you may be asked for your password."

if [ -n "${SONNENSCHEIN_RELEASE_TAG:-}" ]; then
    case "$SONNENSCHEIN_RELEASE_TAG" in
        v[0-9]*.[0-9]*.[0-9]*-test) ;;
        *) err "Invalid SONNENSCHEIN_RELEASE_TAG."; exit 1 ;;
    esac
    RELEASE_API_PATH="releases/tags/$SONNENSCHEIN_RELEASE_TAG"
    info "Fetching pinned release $SONNENSCHEIN_RELEASE_TAG..."
else
    RELEASE_API_PATH="releases/latest"
    info "Fetching latest release info..."
fi
CURL_ARGS=(-sS -w $'\n%{http_code}')
if [ -n "${GITHUB_TOKEN:-}" ]; then
    CURL_ARGS+=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

RELEASE_BODY=""
HTTP_STATUS="000"
for RETRY_DELAY in 0 15 30 60; do
    if [ "$RETRY_DELAY" -gt 0 ]; then
        sleep "$RETRY_DELAY"
    fi

    RELEASE_RESPONSE=$(curl "${CURL_ARGS[@]}" "https://api.github.com/repos/$REPO/$RELEASE_API_PATH" 2>&1)
    HTTP_STATUS=${RELEASE_RESPONSE##*$'\n'}
    RELEASE_BODY=${RELEASE_RESPONSE%$'\n'*}

    if [ "$HTTP_STATUS" = "200" ] && printf '%s' "$RELEASE_BODY" | python3 -c 'import json, sys; json.load(sys.stdin)' 2>/dev/null; then
        break
    fi

    if printf '%s' "$RELEASE_BODY" | grep -qi "rate limit exceeded" && [ "$RETRY_DELAY" -lt 60 ]; then
        info "GitHub API rate limit reached for this IP (60 requests/hour, unauthenticated). Retrying with backoff..."
    fi
done

if [ "$HTTP_STATUS" != "200" ] || ! printf '%s' "$RELEASE_BODY" | python3 -c 'import json, sys; json.load(sys.stdin)' 2>/dev/null; then
    ERROR_BODY=$(printf '%s' "$RELEASE_BODY" | head -c 200)
    err "GitHub API request failed (HTTP $HTTP_STATUS): $ERROR_BODY"
    exit 1
fi

ZIP_URL=$(printf '%s' "$RELEASE_BODY" \
    | grep -o '"browser_download_url": *"[^"]*Sonnenschein-Decky-Plugin[^"]*\.zip"' \
    | head -1 | sed 's/.*"\(https[^"]*\)"/\1/')
if [ -z "$ZIP_URL" ]; then
    err "Could not find the plugin zip in the latest release."
    exit 1
fi
APPIMAGE_URL=$(printf '%s' "$RELEASE_BODY" \
    | grep -o '"browser_download_url": *"[^"]*Sonnenschein_Client-[^"]*x86_64\.AppImage"' \
    | head -1 | sed 's/.*"\(https[^"]*\)"/\1/')
if [ -z "$APPIMAGE_URL" ]; then
    err "Could not find the x86_64 client AppImage in the latest release."
    exit 1
fi
info "Downloading $ZIP_URL"
TMPZIP=$(mktemp /tmp/sonnenschein-plugin-XXXX.zip)
TMPAPPIMAGE=$(mktemp /tmp/sonnenschein-client-XXXX.AppImage)
if ! curl -fsSL -o "$TMPZIP" "$ZIP_URL"; then
    err "Plugin download failed."
    exit 1
fi
info "Downloading $APPIMAGE_URL"
if ! curl -fsSL -o "$TMPAPPIMAGE" "$APPIMAGE_URL"; then
    err "Client AppImage download failed."
    exit 1
fi
if [ ! -s "$TMPZIP" ] || [ ! -s "$TMPAPPIMAGE" ]; then
    err "A downloaded release asset is empty."
    exit 1
fi
if ! unzip -tq "$TMPZIP" >/dev/null; then
    err "Downloaded plugin zip is corrupt. The existing installation was not changed."
    exit 1
fi

APPLICATIONS_DIR="$HOME/Applications"
APPIMAGE_NAME=${APPIMAGE_URL##*/}
mkdir -p "$APPLICATIONS_DIR"
install -m 0755 "$TMPAPPIMAGE" "$APPLICATIONS_DIR/$APPIMAGE_NAME"
ok "Client AppImage installed and executable: $APPLICATIONS_DIR/$APPIMAGE_NAME"

info "Stopping Decky Loader..."
sudo systemctl stop plugin_loader
DECKY_STOPPED=1

info "Removing old plugin files and stale logs..."
sudo rm -rf "$PLUGIN_DIR" "$LOGS_DIR"

info "Installing to $PLUGIN_DIR ..."
# ~/homebrew/plugins is root-owned on the Deck — extraction needs sudo.
sudo unzip -q -o "$TMPZIP" -d "$PLUGINS_DIR"

if [ ! -f "$PLUGIN_DIR/plugin.json" ]; then
    err "Installation failed — $PLUGIN_DIR/plugin.json missing after unzip."
    sudo systemctl start plugin_loader
    exit 1
fi

# Decky's own convention: plugin contents owned by deck, the top-level
# plugin dir owned by root (the loader's permission fixer treats a
# root-owned top dir as 'already correct').
sudo chown -R "$INSTALL_USER:$INSTALL_GROUP" "$PLUGIN_DIR"
sudo chmod -R u+rwX,go+rX "$PLUGIN_DIR"
sudo chmod +x "$PLUGIN_DIR/sonnenschein-run.sh" 2>/dev/null
sudo find "$PLUGIN_DIR" -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null
sudo chown root:root "$PLUGIN_DIR"

info "Starting Decky Loader..."
sudo systemctl start plugin_loader
DECKY_STOPPED=0

info "Waiting 10s for the plugin backend to come up..."
sleep 10

bold "== Diagnosis =="
FAIL=0

# 1. Backend process alive? (title: "Sonnenschein (<path>/main.py)")
if pgrep -f "sonnenschein/main.py" >/dev/null 2>&1 || pgrep -x "Sonnenschein" >/dev/null 2>&1; then
    ok "Backend process is RUNNING."
else
    err "Backend process NOT found."
    FAIL=1
fi

# 2. Startup crash in the loader journal?
CRASH=$(sudo journalctl -u plugin_loader --since "2 minutes ago" --no-pager 2>/dev/null | grep -A 25 "Failed to start Sonnenschein")
if [ -n "$CRASH" ]; then
    err "Backend CRASHED at startup — this is the traceback:"
    echo "----------------------------------------------------------------"
    echo "$CRASH"
    echo "----------------------------------------------------------------"
    FAIL=1
else
    ok "No startup crash in the Decky journal."
fi

# 3. Plugin log written?
if [ -d "$LOGS_DIR" ] && [ -n "$(ls -A "$LOGS_DIR" 2>/dev/null)" ]; then
    ok "Plugin log exists — last lines:"
    tail -n 5 "$(ls -t "$LOGS_DIR"/* | head -1)"
else
    info "No plugin log yet (backend logs on first use)."
fi

echo
if [ "$FAIL" -eq 0 ]; then
    bold "RESULT: Plugin + client installation looks GOOD."
    ok "Client: $APPLICATIONS_DIR/$APPIMAGE_NAME"
    info "Start the client once in Desktop Mode to pair it, then switch to Game Mode."
    info "If Steam was running during install, restart Steam once (Game Mode: Steam > Power > Restart)."
else
    bold "RESULT: Backend problem found — please share the output above."
    info "Full journal: sudo journalctl -u plugin_loader -n 200 --no-pager"
fi
