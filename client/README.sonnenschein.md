# sonnenschein-client

Der native Client für [**Sonnenschein**](https://github.com/Elias02345/sonnenschein) —
ein Fork von [Moonlight-Qt](https://github.com/moonlight-stream/moonlight-qt) (GPL-3.0),
rebrandet und um Sonnenschein-Mehrwerte erweitert. Moonlight-Protokoll-Kompatibilität
bleibt vollständig erhalten; jeder Sunshine/Apollo/Sonnenschein-Host funktioniert.

> **Track-Kontext:** Dies ist Client-Track **C1** (Client-Fundament) der Sonnenschein-
> Roadmap. Ziel: ein Client, der sich mit einem Klick zum Host verbindet und das
> Geräteprofil (Auflösung/Refresh/HDR/Decoder) **automatisch** perfekt konfiguriert.

## Verhältnis zu Upstream

- `upstream` remote → `moonlight-stream/moonlight-qt` (regelmäßiger Sync).
- Branch `sonnenschein` → unsere Änderungen.
- Rebrand: Binary `sonnenschein-client`, QSettings-Identität `Sonnenschein/sonnenschein-client`
  (eigene Config/Pairing, getrennt von einer parallelen Moonlight-Installation).

## Sonnenschein-Mehrwerte (Stand)

### Geräteprofil-Autokonfiguration (`app/backend/autoconfig.{h,cpp}`)

Erkennt **lokal** (ohne Host-Verbindung) die optimalen Stream-Settings für das Gerät:

- **Display**: native Auflösung + höchste Bildwiederholrate (SDL-Display-Enumeration,
  Normalisierung um Standard-Raten).
- **Hardware-Decode-Probe** pro Codec (`Session::getDecoderAvailability`): H.264, HEVC,
  HEVC Main10, AV1, AV1 Main10 — Best-first-Auswahl (AV1 → HEVC → H.264).
- **HDR**: aktiviert, wenn das Display HDR meldet *und* ein 10-bit-Codec HW-dekodierbar ist.
- **Bitrate**: `StreamingPreferences::getDefaultBitrate()` für die gewählte Geometrie.

Die eigentliche Codec-Wahl bleibt bei `VCC_AUTO`, damit die Stream-Aushandlung den
besten Codec wählt, den **Client und Host** beide können.

Verifizierbar headless — kein Stream, kein Host nötig:

```bash
sonnenschein-client detect-profile
```

Gibt das erkannte Profil als JSON aus (Display, HW-Decode-Fähigkeiten, empfohlene
Settings). Auf dem CachyOS-Test-Target (RTX 3070) gegen echte Hardware verifiziert.

## Build (CachyOS / Arch)

Deps: `qt6-base qt6-declarative qt6-svg qt6-wayland sdl2 sdl2_ttf ffmpeg libva
libvdpau opus openssl libplacebo vulkan-headers` (qt6-quickcontrols2 steckt in
qt6-declarative).

```bash
git submodule update --init --recursive
mkdir build && cd build
qmake6 ../sonnenschein-client.pro    # (bzw. moonlight-qt.pro bis zum Rename)
make -j$(nproc)
./app/sonnenschein-client --version
```

## Lizenz

GPL-3.0 (wie Moonlight-Qt). Siehe `LICENSE`.
