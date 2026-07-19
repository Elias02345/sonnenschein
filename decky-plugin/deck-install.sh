#!/bin/bash
# Sonnenschein Decky plugin — clean install + diagnosis for the Steam Deck.
#
# Run in Desktop Mode (Konsole):
#   curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/dev/decky-plugin/deck-install.sh | bash
#
# What it does: stops Decky, removes any old Sonnenschein plugin remains
# (including stale __pycache__/root-owned files from sudo-unzips), fetches
# the latest release zip, installs it with correct ownership, restarts
# Decky, and then PROVES whether the backend is running — showing the
# exact Python traceback from the Decky log if it is not.
set -uo pipefail

REPO="Elias02345/sonnenschein"
PLUGINS_DIR="$HOME/homebrew/plugins"
PLUGIN_DIR="$PLUGINS_DIR/sonnenschein"
LOGS_DIR="$HOME/homebrew/logs/sonnenschein"

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

info "Stopping Decky Loader..."
sudo systemctl stop plugin_loader

info "Removing old plugin files and stale logs..."
sudo rm -rf "$PLUGIN_DIR" "$LOGS_DIR"

info "Fetching latest release info..."
ZIP_URL=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
    | grep -o '"browser_download_url": *"[^"]*Sonnenschein-Decky-Plugin[^"]*\.zip"' \
    | head -1 | sed 's/.*"\(https[^"]*\)"/\1/')
if [ -z "$ZIP_URL" ]; then
    err "Could not find the plugin zip in the latest release."
    exit 1
fi
info "Downloading $ZIP_URL"
TMPZIP=$(mktemp /tmp/sonnenschein-plugin-XXXX.zip)
curl -fsSL -o "$TMPZIP" "$ZIP_URL"

info "Installing to $PLUGIN_DIR ..."
# ~/homebrew/plugins is root-owned on the Deck — extraction needs sudo.
sudo unzip -q -o "$TMPZIP" -d "$PLUGINS_DIR"
rm -f "$TMPZIP"

if [ ! -f "$PLUGIN_DIR/plugin.json" ]; then
    err "Installation failed — $PLUGIN_DIR/plugin.json missing after unzip."
    sudo systemctl start plugin_loader
    exit 1
fi

# Decky's own convention: plugin contents owned by deck, the top-level
# plugin dir owned by root (the loader's permission fixer treats a
# root-owned top dir as 'already correct').
sudo chown -R "$(id -un):$(id -gn)" "$PLUGIN_DIR"
sudo chmod -R u+rwX,go+rX "$PLUGIN_DIR"
sudo chmod +x "$PLUGIN_DIR/sonnenschein-run.sh" 2>/dev/null
sudo find "$PLUGIN_DIR" -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null
sudo chown root:root "$PLUGIN_DIR"

info "Starting Decky Loader..."
sudo systemctl start plugin_loader

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
    bold "RESULT: Installation looks GOOD. Switch to Game Mode and open the Sonnenschein panel."
    info "If Steam was running during install, restart Steam once (Game Mode: Steam > Power > Restart)."
else
    bold "RESULT: Backend problem found — please share the output above."
    info "Full journal: sudo journalctl -u plugin_loader -n 200 --no-pager"
fi
