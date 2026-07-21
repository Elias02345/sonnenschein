#!/bin/sh
# Sonnenschein Decky runner — launched by Steam as a non-Steam shortcut.
# Host and app come from the shortcut's launch options:
#   SONNENSCHEIN_HOST='<addr>' SONNENSCHEIN_APP='<app title>' %command%
# A shell wrapper is required so Steam can focus the streaming window
# (pattern proven by MoonDeck, GPL-3).

if [ -z "$SONNENSCHEIN_HOST" ] || [ -z "$SONNENSCHEIN_APP" ]; then
    echo "SONNENSCHEIN_HOST / SONNENSCHEIN_APP not set" >&2
    exit 1
fi

# Locate the Sonnenschein Client: user override, newest AppImage, flatpak.
if [ -n "$SONNENSCHEIN_CLIENT" ] && [ -x "$SONNENSCHEIN_CLIENT" ]; then
    exec "$SONNENSCHEIN_CLIENT" stream "$SONNENSCHEIN_HOST" "$SONNENSCHEIN_APP" --quit-after
fi

for c in "$HOME"/Applications/Sonnenschein_Client*.AppImage; do
    [ -e "$c" ] && CLIENT="$c"
done
if [ -n "$CLIENT" ] && [ -x "$CLIENT" ]; then
    exec "$CLIENT" stream "$SONNENSCHEIN_HOST" "$SONNENSCHEIN_APP" --quit-after
fi

if command -v flatpak >/dev/null 2>&1 && flatpak info io.github.elias02345.Sonnenschein >/dev/null 2>&1; then
    exec flatpak run io.github.elias02345.Sonnenschein stream "$SONNENSCHEIN_HOST" "$SONNENSCHEIN_APP" --quit-after
fi

echo "Sonnenschein Client not found (put the AppImage in ~/Applications)" >&2
exit 1
