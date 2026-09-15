#!/usr/bin/env bash
set -Eeuo pipefail

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/sonnenschein-runner-test.XXXXXX")"
trap 'rm -rf -- "$TEST_ROOT"' EXIT

mkdir -p "$TEST_ROOT/home/Applications"
FAKE_CLIENT="$TEST_ROOT/home/Applications/Sonnenschein_Client-test-x86_64.AppImage"
ARGS_FILE="$TEST_ROOT/args"

cat >"$FAKE_CLIENT" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"$SONNENSCHEIN_TEST_ARGS"
EOF
chmod +x "$FAKE_CLIENT"

HOME="$TEST_ROOT/home" \
SONNENSCHEIN_HOST="deck-host.local" \
SONNENSCHEIN_APP="Test Game" \
SONNENSCHEIN_TEST_ARGS="$ARGS_FILE" \
  sh decky-plugin/sonnenschein-run.sh

EXPECTED=$'stream\ndeck-host.local\nTest Game\n--quit-after'
ACTUAL="$(cat "$ARGS_FILE")"
test "$ACTUAL" = "$EXPECTED"

echo "Deck runner lifecycle harness: OK"
