#!/usr/bin/env bash
set -Eeuo pipefail

RELEASE_TAG="${1:?missing release tag}"
DECK_HOME="${2:?missing Deck home}"
DECK_UID="${3:?missing Deck uid}"
DECK_GID="${4:?missing Deck gid}"
REPO="Elias02345/sonnenschein"
if [ "${SONNENSCHEIN_UPDATE_TESTING:-0}" = 1 ]; then
    STATE_DIR="$DECK_HOME/test-update-state"
else
    STATE_DIR="/var/lib/sonnenschein-decky-update"
fi
STATUS_FILE="$STATE_DIR/status.json"
LOG_FILE="$STATE_DIR/update.log"

case "$RELEASE_TAG" in
    v[0-9]*.[0-9]*.[0-9]*-test) ;;
    *) printf 'Invalid release tag.\n' >&2; exit 1 ;;
esac
case "$DECK_UID:$DECK_GID" in
    *[!0-9:]*|:*) printf 'Invalid Deck uid/gid.\n' >&2; exit 1 ;;
esac
case "$DECK_HOME" in
    /home/*|/tmp/sonnenschein-update-test.*) ;;
    *) printf 'Invalid Deck home.\n' >&2; exit 1 ;;
esac

PLUGIN_DIR="$DECK_HOME/homebrew/plugins/sonnenschein"
PLUGINS_DIR="$DECK_HOME/homebrew/plugins"
LOGS_DIR="$DECK_HOME/homebrew/logs/sonnenschein"
APPLICATIONS_DIR="$DECK_HOME/Applications"
VERSION="${RELEASE_TAG#v}"
ZIP_NAME="Sonnenschein-Decky-Plugin-${VERSION}.zip"
APPIMAGE_NAME="Sonnenschein_Client-${VERSION}-x86_64.AppImage"
TMP_DIR="$(mktemp -d /tmp/sonnenschein-update.XXXXXX)"
STAGE_PARENT="$PLUGINS_DIR/.sonnenschein-stage-$$"
STAGED_PLUGIN="$STAGE_PARENT/sonnenschein"
BACKUP_PLUGIN="$PLUGINS_DIR/.sonnenschein-backup-$$"
DECKY_STOPPED=0
NEW_ACTIVE=0
BACKUP_CREATED=0

write_status() {
    local state="$1"
    local message="$2"
    local tmp
    tmp="$(mktemp "$STATE_DIR/.status.XXXXXX")"
    printf '{"state":"%s","message":"%s"}\n' "$state" "$message" >"$tmp"
    chmod 0644 "$tmp"
    mv "$tmp" "$STATUS_FILE"
}

cleanup() {
    local status=$?
    if [ "$status" -ne 0 ] && { [ "$NEW_ACTIVE" -eq 1 ] || [ "$BACKUP_CREATED" -eq 1 ]; }; then
        systemctl stop plugin_loader >/dev/null 2>&1 || true
        rm -rf -- "$PLUGIN_DIR"
        if [ "$BACKUP_CREATED" -eq 1 ] && [ -d "$BACKUP_PLUGIN" ]; then
            mv "$BACKUP_PLUGIN" "$PLUGIN_DIR"
            BACKUP_CREATED=0
        fi
        systemctl start plugin_loader >/dev/null 2>&1 || true
        DECKY_STOPPED=0
    elif [ "$DECKY_STOPPED" -eq 1 ]; then
        systemctl start plugin_loader >/dev/null 2>&1 || true
    fi
    rm -rf -- "$STAGE_PARENT"
    rm -rf -- "$TMP_DIR"
    if [ "$status" -ne 0 ]; then
        write_status "error" "Update fehlgeschlagen. Details stehen im Update-Log."
    fi
    exit "$status"
}
trap cleanup EXIT

if [ "${SONNENSCHEIN_UPDATE_TESTING:-0}" = 1 ]; then
    mkdir -p "$STATE_DIR"
    chmod 0755 "$STATE_DIR"
else
    install -d -o root -g root -m 0755 "$STATE_DIR"
fi
rm -f -- "$LOG_FILE"
if [ "${SONNENSCHEIN_UPDATE_TESTING:-0}" = 1 ]; then
    : >"$LOG_FILE"
    chmod 0644 "$LOG_FILE"
else
    install -o root -g root -m 0644 /dev/null "$LOG_FILE"
fi
exec >>"$LOG_FILE" 2>&1

write_status "installing" "Prüfe Release und SHA-256-Digests…"
release_json="$TMP_DIR/release.json"
curl -fsSL --retry 3 \
    -H 'Accept: application/vnd.github+json' \
    -H 'User-Agent: sonnenschein-decky-updater' \
    "https://api.github.com/repos/$REPO/releases/tags/$RELEASE_TAG" \
    -o "$release_json"

read_asset() {
    python3 - "$release_json" "$1" <<'PY'
import json, sys
release = json.load(open(sys.argv[1], encoding="utf-8"))
matches = [a for a in release.get("assets", []) if a.get("name") == sys.argv[2]]
if len(matches) != 1 or not str(matches[0].get("digest", "")).startswith("sha256:"):
    raise SystemExit(1)
print(matches[0]["browser_download_url"])
print(matches[0]["digest"].removeprefix("sha256:"))
PY
}

mapfile -t zip_meta < <(read_asset "$ZIP_NAME")
mapfile -t app_meta < <(read_asset "$APPIMAGE_NAME")
test "${#zip_meta[@]}" -eq 2
test "${#app_meta[@]}" -eq 2

zip_file="$TMP_DIR/$ZIP_NAME"
app_file="$TMP_DIR/$APPIMAGE_NAME"
curl -fsSL --retry 3 -o "$zip_file" "${zip_meta[0]}"
curl -fsSL --retry 3 -o "$app_file" "${app_meta[0]}"
printf '%s  %s\n' "${zip_meta[1]}" "$zip_file" | sha256sum -c -
printf '%s  %s\n' "${app_meta[1]}" "$app_file" | sha256sum -c -

expected_entries="$TMP_DIR/expected.txt"
actual_entries="$TMP_DIR/actual.txt"
printf '%s\n' \
    sonnenschein/ \
    sonnenschein/LICENSE \
    sonnenschein/dist/ \
    sonnenschein/dist/index.js \
    sonnenschein/main.py \
    sonnenschein/package.json \
    sonnenschein/plugin.json \
    sonnenschein/sonnenschein-run.sh \
    sonnenschein/sonnenschein-update.sh | sort >"$expected_entries"
unzip -Z1 "$zip_file" | sort >"$actual_entries"
cmp -s "$expected_entries" "$actual_entries"
python3 - "$zip_file" <<'PY'
import stat, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as archive:
    total = 0
    for entry in archive.infolist():
        mode = (entry.external_attr >> 16) & 0xFFFF
        if entry.is_dir():
            if mode and not stat.S_ISDIR(mode):
                raise SystemExit("non-directory mode on directory entry")
        elif mode and not stat.S_ISREG(mode):
            raise SystemExit("non-regular file in plugin archive")
        total += entry.file_size
    if total > 20 * 1024 * 1024:
        raise SystemExit("plugin archive expands beyond 20 MiB")
PY
unzip -tq "$zip_file" >/dev/null

write_status "installing" "Installiere AppImage und Decky-Plugin…"
mkdir -p "$APPLICATIONS_DIR"
install -o "$DECK_UID" -g "$DECK_GID" -m 0755 "$app_file" "$APPLICATIONS_DIR/$APPIMAGE_NAME"

# Prepare and permission the complete replacement before stopping Decky.
rm -rf -- "$STAGE_PARENT" "$BACKUP_PLUGIN"
mkdir -p "$STAGE_PARENT"
unzip -q "$zip_file" -d "$STAGE_PARENT"
test -f "$STAGED_PLUGIN/plugin.json"
chown -R "$DECK_UID:$DECK_GID" "$STAGED_PLUGIN"
chmod -R u+rwX,go+rX "$STAGED_PLUGIN"
chmod +x "$STAGED_PLUGIN/sonnenschein-run.sh" "$STAGED_PLUGIN/sonnenschein-update.sh"
chown root:root "$STAGED_PLUGIN"

systemctl stop plugin_loader
DECKY_STOPPED=1
test -d "$PLUGIN_DIR"
mv "$PLUGIN_DIR" "$BACKUP_PLUGIN"
BACKUP_CREATED=1
mv "$STAGED_PLUGIN" "$PLUGIN_DIR"
NEW_ACTIVE=1
rmdir "$STAGE_PARENT"
rm -rf -- "$LOGS_DIR"
systemctl start plugin_loader
DECKY_STOPPED=0
rm -rf -- "$BACKUP_PLUGIN"
BACKUP_CREATED=0
NEW_ACTIVE=0

write_status "success" "Update installiert. Decky wurde neu gestartet."
