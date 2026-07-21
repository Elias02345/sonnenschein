#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$(mktemp -d /tmp/sonnenschein-install-test-XXXXXX)"
trap 'rm -rf "$TEST_DIR"' EXIT

export HOME="$TEST_DIR/home/deck"
export FIXTURE_ZIP="$TEST_DIR/plugin.zip"
export FIXTURE_APPIMAGE="$TEST_DIR/client.AppImage"
STUB_DIR="$TEST_DIR/bin"
mkdir -p "$HOME/homebrew/plugins" "$HOME/.config/Sonnenschein" "$STUB_DIR"
printf 'pairing-must-survive\n' > "$HOME/.config/Sonnenschein/sonnenschein-client.conf"
printf '\177ELFfake-appimage\n' > "$FIXTURE_APPIMAGE"

python3 -c 'import os, zipfile
p = os.environ["FIXTURE_ZIP"]
with zipfile.ZipFile(p, "w") as z:
    z.writestr("sonnenschein/plugin.json", "{}")
    z.writestr("sonnenschein/main.py", "class Plugin: pass\n")
    z.writestr("sonnenschein/sonnenschein-run.sh", "#!/bin/sh\n")
'

cat > "$STUB_DIR/curl" <<'EOF'
#!/usr/bin/env bash
set -eu
output=""
previous=""
for argument in "$@"; do
    if [ "$previous" = "-o" ]; then output="$argument"; fi
    previous="$argument"
done
url=${!#}
case "$url" in
    *api.github.com*)
        printf '%s\n200' '{"assets":[{"browser_download_url":"https://example.test/Sonnenschein-Decky-Plugin-0.2.5-test.zip"},{"browser_download_url":"https://example.test/Sonnenschein_Client-0.2.5-test-x86_64.AppImage"}]}'
        ;;
    *Decky-Plugin*) cp "$FIXTURE_ZIP" "$output" ;;
    *x86_64.AppImage) cp "$FIXTURE_APPIMAGE" "$output" ;;
    *) exit 22 ;;
esac
EOF

cat > "$STUB_DIR/sudo" <<'EOF'
#!/usr/bin/env bash
set -eu
case "${1:-}" in
    systemctl|journalctl) exit 0 ;;
    chown) exit 0 ;;
    *) exec "$@" ;;
esac
EOF

cat > "$STUB_DIR/pgrep" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF

cat > "$STUB_DIR/sleep" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF

chmod +x "$STUB_DIR"/*
PATH="$STUB_DIR:$PATH" bash "$ROOT_DIR/deck-install.sh"

APPIMAGE="$HOME/Applications/Sonnenschein_Client-0.2.5-test-x86_64.AppImage"
test -x "$APPIMAGE"
test -s "$APPIMAGE"
test -f "$HOME/homebrew/plugins/sonnenschein/plugin.json"
grep -qx 'pairing-must-survive' "$HOME/.config/Sonnenschein/sonnenschein-client.conf"

printf 'Deck installer one-shot harness: OK\n'
