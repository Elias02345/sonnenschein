#!/usr/bin/env bash
set -Eeuo pipefail

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/sonnenschein-update-test.XXXXXX")"
cleanup() {
    status=$?
    if [ "$status" -ne 0 ]; then
        test ! -f "$TEST_ROOT/test-update-state/status.json" || cat "$TEST_ROOT/test-update-state/status.json" >&2
        test ! -f "$TEST_ROOT/test-update-state/update.log" || cat "$TEST_ROOT/test-update-state/update.log" >&2
    fi
    rm -rf -- "$TEST_ROOT"
    exit "$status"
}
trap cleanup EXIT

mkdir -p "$TEST_ROOT/bin" "$TEST_ROOT/settings" "$TEST_ROOT/homebrew/plugins" "$TEST_ROOT/fixture/plugin/sonnenschein/dist"
mkdir -p "$TEST_ROOT/homebrew/plugins/sonnenschein"
printf 'old plugin\n' >"$TEST_ROOT/homebrew/plugins/sonnenschein/old-base"
plugin="$TEST_ROOT/fixture/plugin/sonnenschein"
printf '{}\n' >"$plugin/plugin.json"
printf '{"version":"0.2.9"}\n' >"$plugin/package.json"
printf '#!/bin/sh\n' >"$plugin/sonnenschein-run.sh"
printf '#!/bin/sh\n' >"$plugin/sonnenschein-update.sh"
printf 'backend\n' >"$plugin/main.py"
printf 'bundle\n' >"$plugin/dist/index.js"
printf 'license\n' >"$plugin/LICENSE"
python3 - "$TEST_ROOT/fixture/plugin" "$TEST_ROOT/fixture/plugin.zip" <<'PY'
import pathlib, sys, zipfile
root = pathlib.Path(sys.argv[1])
with zipfile.ZipFile(sys.argv[2], "w") as archive:
    for path in sorted(root.rglob("*")):
        name = path.relative_to(root).as_posix()
        if path.is_dir():
            archive.writestr(name + "/", b"")
        else:
            info = zipfile.ZipInfo.from_file(path, name)
            info.external_attr |= 0o755 << 16
            archive.writestr(info, path.read_bytes())
PY
printf 'appimage\n' >"$TEST_ROOT/fixture/client.AppImage"

zip_digest="$(sha256sum "$TEST_ROOT/fixture/plugin.zip" | cut -d' ' -f1)"
app_digest="$(sha256sum "$TEST_ROOT/fixture/client.AppImage" | cut -d' ' -f1)"
cat >"$TEST_ROOT/fixture/release.json" <<EOF
{"assets":[
 {"name":"Sonnenschein-Decky-Plugin-0.2.9-test.zip","browser_download_url":"https://fixture/plugin.zip","digest":"sha256:$zip_digest"},
 {"name":"Sonnenschein_Client-0.2.9-test-x86_64.AppImage","browser_download_url":"https://fixture/client.AppImage","digest":"sha256:$app_digest"}
]}
EOF

cat >"$TEST_ROOT/bin/curl" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
output=""
url=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        -o) output="$2"; shift 2 ;;
        -H) shift 2 ;;
        -*) shift ;;
        *) url="$1"; shift ;;
    esac
done
case "$url" in
    */releases/tags/*) cp "$FIXTURE_DIR/release.json" "$output" ;;
    */plugin.zip) cp "$FIXTURE_DIR/plugin.zip" "$output" ;;
    */client.AppImage) cp "$FIXTURE_DIR/client.AppImage" "$output" ;;
    *) exit 1 ;;
esac
EOF

cat >"$TEST_ROOT/bin/systemctl" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$SYSTEMCTL_LOG"
if [ "$*" = "start plugin_loader" ] && [ "${FAIL_FIRST_START:-0}" = 1 ] && [ ! -e "$START_FAILURE_MARKER" ]; then
    : >"$START_FAILURE_MARKER"
    exit 1
fi
EOF
cat >"$TEST_ROOT/bin/chown" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$TEST_ROOT/bin/curl" "$TEST_ROOT/bin/systemctl" "$TEST_ROOT/bin/chown"

STATUS_FILE="$TEST_ROOT/test-update-state/status.json"
uid="$(id -u)"
gid="$(id -g)"
PATH="$TEST_ROOT/bin:$PATH" \
FIXTURE_DIR="$TEST_ROOT/fixture" \
SYSTEMCTL_LOG="$TEST_ROOT/systemctl.log" \
SONNENSCHEIN_UPDATE_TESTING=1 \
    bash decky-plugin/sonnenschein-update.sh \
    v0.2.9-test "$TEST_ROOT" "$uid" "$gid"

grep -Fq '"state":"success"' "$STATUS_FILE"
grep -Fxq 'stop plugin_loader' "$TEST_ROOT/systemctl.log"
grep -Fxq 'start plugin_loader' "$TEST_ROOT/systemctl.log"
test -x "$TEST_ROOT/Applications/Sonnenschein_Client-0.2.9-test-x86_64.AppImage"
test -x "$TEST_ROOT/homebrew/plugins/sonnenschein/sonnenschein-update.sh"

# Inject a loader-start failure after atomic activation. The helper must put
# the complete previous plugin back before restarting Loader a second time.
printf 'preserve me\n' >"$TEST_ROOT/homebrew/plugins/sonnenschein/old-marker"
if PATH="$TEST_ROOT/bin:$PATH" \
    FIXTURE_DIR="$TEST_ROOT/fixture" \
    SYSTEMCTL_LOG="$TEST_ROOT/systemctl-failure.log" \
    FAIL_FIRST_START=1 \
    START_FAILURE_MARKER="$TEST_ROOT/start-failed" \
    SONNENSCHEIN_UPDATE_TESTING=1 \
    bash decky-plugin/sonnenschein-update.sh \
    v0.2.9-test "$TEST_ROOT" "$uid" "$gid"; then
    echo "Failure injection unexpectedly succeeded." >&2
    exit 1
fi
test -f "$TEST_ROOT/homebrew/plugins/sonnenschein/old-marker"
grep -Fq '"state":"error"' "$STATUS_FILE"
test "$(grep -c '^start plugin_loader$' "$TEST_ROOT/systemctl-failure.log")" -eq 2

echo "Deck updater integrity/lifecycle/rollback harness: OK"
