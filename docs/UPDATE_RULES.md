# UPDATE_RULES.md — Sacred Paths & Update-Vertrag

Dieses Dokument definiert, was der Updater (`installer/update.sh`, WebUI
`POST /api/update`) **niemals anfassen darf**, und wie ein Update abläuft.

## Sacred Paths — der Updater lässt sie in Ruhe

| Pfad | Inhalt |
|---|---|
| `~/.config/sunshine/` | Benutzer-Konfiguration (`sunshine.conf`), Apps (`apps.json`), Zugangsdaten (`sunshine_state.json`), Pairing-Zertifikate |
| `~/.local/state/sonnenschein/` | Laufzeit-State: Portal-Restore-Token, Display-Recovery-Lockfile, `update.log` |
| `${PREFIX}/install-state.env` | Installer-Zustand (PREFIX, SRC_DIR, Firewall/Avahi/WoL-Tracking) — wird nur vom Installer selbst geschrieben |
| `${PREFIX}/installed-packages.txt` | Paket-Manifest für die saubere Deinstallation |

Der Updater installiert nach `${PREFIX}` (Default `/opt/sonnenschein`) über
`cmake --install` — er ersetzt Binaries und Assets, **keine** Nutzerdaten.
On-Disk-Pfade bleiben aus Kompatibilitätsgründen unter dem Namen `sunshine`
(bewusste Entscheidung, siehe STATUS.md Phase 1.6: funktionierendes Pairing
des Maintainers nicht brechen).

## Update-Ablauf (`installer/update.sh [branch]`)

1. **Quelle auflösen** — läuft das Skript aus `${PREFIX}/installer`, wird
   `SRC_DIR` aus `install-state.env` gelesen (persistenter Checkout unter
   `~/.local/share/sonnenschein/src`).
2. **Pull** — `git fetch` + `checkout <branch>` + `pull --ff-only` +
   Submodule. Kein Force-Reset: lokale Änderungen am Checkout brechen das
   Update bewusst sichtbar ab statt still verworfen zu werden.
3. **Rebuild** — CMake/Ninja Release-Build im Checkout.
4. **Install** — `sudo cmake --install`, Manifest-Kopie, Installer-Kopie
   nach `${PREFIX}/installer` synchronisieren.
5. **Caps-Guard** — stale File-Capabilities auf dem Binary entfernen
   (setcap bricht PipeWire-Capture, siehe STATUS.md §9).
6. **Restart** — user- oder system-Service, je nachdem was aktiv ist.
7. **Health-Check** — `doctor.sh --repair`; Fehlschläge sind Warnungen,
   das Update selbst gilt als angewendet.

## WebUI-Update (`POST /api/update`, Body optional `{"branch":"main"}`)

- Auth: Session-Cookie wie alle `/api/*`-Routen.
- Der Endpoint startet den Updater über `systemd-run --user --collect
  --unit=sonnenschein-update` — als transiente Unit **außerhalb** der
  Service-cgroup, damit er den `systemctl --user restart sonnenschein` am
  Ende des Updates überlebt.
- Log: `~/.local/state/sonnenschein/update.log` (Response-Feld `log`).
- Läuft bereits ein Update, schlägt `systemd-run --unit` fehl → kein
  Doppel-Update möglich.
- **Voraussetzung NOPASSWD-sudo**: Der Install-Schritt braucht sudo; ein
  headless Service kann nicht nach dem Passwort fragen. Ohne
  `sudo -n` liefert der Endpoint `status:false` mit dem manuellen Befehl
  (`bash ${PREFIX}/installer/update.sh <branch>`). Wer den Ein-Klick-Weg
  will, hinterlegt eine sudoers-Regel für seinen Benutzer.
- Branch-Namen werden serverseitig strikt validiert
  (`[A-Za-z0-9._/-]{1,100}`), bevor sie eine Shell erreichen.

## Rollback

- Manuell: `git -C <SRC_DIR> checkout <alter-commit>` + `bash
  ${PREFIX}/installer/update.sh` — der Updater baut, was ausgecheckt ist.
- Die Nutzerdaten (Sacred Paths) sind von Updates unberührt, ein Rollback
  des Binaries gefährdet daher keine Pairings/Configs.
- Automatisches Rollback bei fehlgeschlagenem Health-Check: geplant
  (ROADMAP Phase 6, offen).

## Regeln für Entwickler

- **Niemals** einen Migrationschritt einbauen, der Sacred Paths löscht
  oder umbenennt.
- Config-Format-Änderungen müssen alte Werte weiterlesen können
  (additive Änderungen only).
- Wenn eine Änderung ein In-Place-Update auf einer Live-Installation
  brechen könnte: explizit in STATUS.md flaggen, bevor sie auf `dev`/`main`
  landet.
