#!/usr/bin/env bash
set -Eeuo pipefail

VERSION="${1:?usage: generate-client-release-notes.sh <version> <output>}"
OUTPUT="${2:?usage: generate-client-release-notes.sh <version> <output>}"
TAG="v${VERSION}"
BASE="https://github.com/Elias02345/sonnenschein"

CHANGELOG_SECTION="$(awk -v version="$VERSION" '
  index($0, "## " version " — ") == 1 { found=1; next }
  found && /^## / { exit }
  found { print }
' CHANGELOG.md)"

if [[ -z "$CHANGELOG_SECTION" ]]; then
  echo "No CHANGELOG.md section found for ${VERSION}" >&2
  exit 1
fi

cat >"$OUTPUT" <<EOF
# Sonnenschein ${TAG}

> **Test build:** Please report Steam Deck, controller, and host-lifecycle
> results before a stable release is created.

## What changed

${CHANGELOG_SECTION}

## Downloads

| Platform | Download |
|---|---|
| Steam Deck one-shot installer | [Sonnenschein-Deck-Installer.sh](${BASE}/releases/download/${TAG}/Sonnenschein-Deck-Installer.sh) |
| Steam Deck / Linux client | [Sonnenschein_Client-${VERSION}-x86_64.AppImage](${BASE}/releases/download/${TAG}/Sonnenschein_Client-${VERSION}-x86_64.AppImage) |
| Steam Deck integration | [Sonnenschein-Decky-Plugin-${VERSION}.zip](${BASE}/releases/download/${TAG}/Sonnenschein-Decky-Plugin-${VERSION}.zip) |
| Windows | [SonnenscheinClientSetup-${VERSION}.exe](${BASE}/releases/download/${TAG}/SonnenscheinClientSetup-${VERSION}.exe) |
| macOS | [Sonnenschein_Client-${VERSION}.dmg](${BASE}/releases/download/${TAG}/Sonnenschein_Client-${VERSION}.dmg) |

Debug-symbol packages are available in the asset list below.

## Steam Deck installation

Follow the complete [Steam Deck setup and troubleshooting guide](${BASE}/blob/${TAG}/docs/steam-deck.md).

The verified one-shot installer updates both the client AppImage and Decky plugin:

\`\`\`bash
curl -fsSL ${BASE}/releases/download/${TAG}/Sonnenschein-Deck-Installer.sh | bash
\`\`\`

## Verification

- Built from the immutable \`${TAG}\` source tag.
- Client, Decky backend, bundle, installer and lifecycle harnesses are required
  to pass before publishing.
- Published assets will be audited after upload and recorded in \`STATUS.md\`;
  real Steam Deck plus host testing remains required for this test build.
EOF
