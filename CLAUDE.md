# CLAUDE.md — Lies das, bevor du irgendetwas tust

Du bist eine Claude-Code-Instanz, die am Sonnenschein-Projekt arbeitet. Sonnenschein ist ein Linux-natives, headless Moonlight-Streaming-Backend, das Apollo (einen Sunshine-Fork) als Basis nutzt und Apollos Killer-Feature (Auto-Match Virtual Display beim Pairing) nach Linux portiert.

**Maintainer** (in Kürze: "der User"): Elias Kanakidis · GitHub [@Elias02345](https://github.com/Elias02345) · email elias-kanakidis@gmx.de · Sprache **Deutsch**.

**Repo:** <https://github.com/Elias02345/sonnenschein>

## Pflichtlektüre, sofort, in dieser Reihenfolge

1. **[`docs/STATUS.md`](docs/STATUS.md)** — die lebendige Roadmap mit Stand aller Phasen, allen wichtigen Entscheidungen, allen bekannten Bugs/Workarounds, der ganzen Hardware-Lage des Maintainers, und der konkreten "was als nächstes"-Liste. Lies das zuerst und vollständig.
2. **[`docs/ROADMAP.md`](docs/ROADMAP.md)** — die langfristige Phasenstruktur (öffentliche Vision; weniger detailliert als STATUS.md).
3. **[`docs/building.md`](docs/building.md)** — Build-Anleitung pro Distro inkl. der Workarounds, die wir bereits brauchten (libva 2.22, Node 20, etc.).
4. **[`docs/SESSION_PROMPT.md`](docs/SESSION_PROMPT.md)** — die wortwörtliche Vorlage, mit der der Maintainer eine neue Session aufmacht. Du wirst gefragt, das nachzuhalten — siehe unten.
5. **[`docs/RELEASE_RULES.md`](docs/RELEASE_RULES.md)** — verbindlicher Ablauf für Versionierung, Tagging, Veröffentlichung und Asset-Nachprüfung.

## Was du bei jeder Antwort machen musst

### Beim Sessionstart
- Vor der ersten inhaltlichen Aktion: STATUS.md komplett lesen (kein Überfliegen).
- Auf `dev` Branch arbeiten, niemals direkt auf `main`. (`main` ist stabile Releases, leer bis zum 1.0-Release.)
- `git status` und `git log --oneline -10` kurz prüfen — was ist seit dem in STATUS.md dokumentierten Stand passiert?
- Wenn auf `dev` Commits seit STATUS.md's "Letzte Commits"-Liste neu sind: STATUS.md zuerst aktualisieren.

### Während der Arbeit
- Jede vom Maintainer gegebene Antwort auf eine Frage, jede Architektur-Entscheidung, jedes "wir machen das so"-Statement: in STATUS.md unter dem passenden Abschnitt persistieren, **bevor** du den Code dafür schreibst. So geht keine Info verloren wenn der Rate-Limit zuschlägt.
- Wenn du einen Commit pushst: gleichzeitig STATUS.md unter "Letzte Commits chronologisch" updaten + Phase-Status aktualisieren + den Commit-Hash dazuschreiben. Beides im selben PR/Commit ist okay.
- Wenn du ein Release vorbereitest oder veröffentlichst: `docs/RELEASE_RULES.md` strikt in der dort festgelegten Reihenfolge abarbeiten. Kein Tag vor grüner `dev`-CI und keine „verifiziert"-Meldung vor Prüfung der tatsächlich veröffentlichten Assets.
- Wenn ein Build-Fehler oder Workaround entdeckt wird: in STATUS.md unter "Bekannte Probleme & Workarounds" persistieren, möglichst mit der genauen Befehls­zeile zur Reproduktion und dem Commit, der den Fix bringt.

### Beim Sessionende oder kurz vor erwartbarem Limit
- Letzte 5-10 Minuten: STATUS.md final aktualisieren — jede angefangene Phase mit "in_progress (50%)"-artigem Hinweis markieren, der nächste Schritt explizit benennen.
- Wenn du gerade einen halben Code-Stand hast: noch in `_reference/work-in-progress/` lokal ablegen oder als WIP-Commit auf einen Feature-Branch pushen, **nicht** halbgaren Code auf `dev` pushen.

## Wichtige Defaults (vom Maintainer bestätigt — nicht hinterfragen)

- **Lizenz**: GPL-3.0-only (zwingend — wir forken Apollo/Sunshine die GPL-3 sind)
- **Plattform-Fokus**: Linux Wayland-first (KDE Plasma 6 hauptsächlich, GNOME/Sway/Hyprland sekundär), X11 als Fallback
- **GPU-Support**: NVIDIA, AMD, Intel — alle drei vollwertig, automatische Auswahl mit Manual-Override für Multi-GPU
- **NVIDIA-Mindesttreiber**: 580.94.11 (für Wayland HDR via `VK_EXT_hdr_metadata`)
- **Distros**: Arch / CachyOS / Ubuntu 22.04+ / Debian 12+ / Fedora 40+ / openSUSE Tumbleweed
- **Sprachen**: DE + EN zwingend bis 1.0; Crowdin-Setup für mehr nach Release
- **Audio**: PipeWire Default + PulseAudio Fallback
- **WebUI-Stack**: Vue 3 + Vite + **PrimeVue 4** (modernes UI-Framework, hochwertige Animationen, Dunkelmodus-Default, responsive bis 360 px Mobile)
- **Telemetrie**: opt-in Crash-Reporter, der bei Crash einen pre-filled GitHub-Issue auf [Elias02345/sonnenschein](https://github.com/Elias02345/sonnenschein/issues) öffnet
- **Update-System**: Self-Update via GitHub Releases, automatisch + manuell, Branch-Wahl `main` vs `dev` (wenn `main` neuer als `dev` → automatisch zu `main`)
- **Service-Modus**: systemd `--user` Service (für Wayland-Compositor-Zugang)
- **Anti-Cheat**: Steam + Proton ist der Pfad, EAC/Vanguard sind keine Anforderung
- **Performance-Ziel**: <30 ms LAN-Latenz, AV1 wenn möglich, sonst HEVC, sonst H.264; bitrate adaptiv

## Branch-Strategie

| Branch | Verwendung |
|---|---|
| `main` | Stabile Releases, getaggt mit `vX.Y.Z`. Nur durch PR-Merge von `dev` nach Test-Bestätigung. |
| `dev` | Aktive Entwicklung. Alle Phase-Commits, alle Hotfixes, alle Tests. |
| `feature/*`, `fix/*` | Kurzlebige Branches aus `dev` für nicht-trivialen WIP-Code. |

**Regel**: Du committest und pushst direkt auf `dev`. Du erstellst nur dann einen Feature-Branch, wenn der Code in mehreren kleinen Schritten landen muss und es keinen Sinn macht, das in einem Commit zu bündeln.

## Sprache

- Mit dem Maintainer: **immer Deutsch**. Auch in Code-Kommentaren bevorzugt Deutsch wenn es Doku ist, Englisch wenn es technische Identifier oder Public-API-Doku ist.
- Commit-Messages: **Englisch** (Industriestandard, internationale Mitwirkende).
- Code-Identifier (Funktionsnamen, Variablen): **Englisch**.
- WebUI-Strings: i18n via vue-i18n, beide Sprachen pflegen.

## Wenn du einen Subagenten startest

Subagenten sind nur Advisory. Verifiziere immer ihren Output gegen den realen Repo-Stand. Implementiere selbst. Speziell für Sonnenschein:

- **Explore** Agent für reine Code-Lookups innerhalb des Repos (z.B. "Wo wird `launch_session->enable_hdr` benutzt?").
- **general-purpose** Agent für Web-Recherche zu Linux-Compositor-Themen (NVIDIA-Driver-State, KWin-API-Drift, etc.).
- **Plan** Agent für Architektur-Skizzen vor größeren Refactorings.

## Build-Setup-Realität

Maintainer arbeitet auf Windows 11. Code-Editing dort. **Bauen + Testen geht NICHT auf Windows.** Drei Wege parallel:

1. **GitHub Actions CI** — pro Push, baut für Arch / Ubuntu / Fedora in Containern. Permissiv markiert (continue-on-error) während Phase 1 + 2 stabilisiert.
2. **WSL2 Ubuntu 24.04** auf der Windows-Maschine — für lokale Build-Verifikation. Build dir = `/root/snsbuild` (NICHT `/tmp/snsbuild` — `/tmp` wird beim WSL-Restart gewipt).
3. **CachyOS-Rechner mit RTX 3070** — das echte Test-Target. Hier passiert Phase 2D + alle realen End-to-End-Tests.

Auf WSL braucht man libva 2.22+ (Stock 24.04 hat 2.20 ohne `vaMapBuffer2`). Das Skript dafür steht in `docs/building.md`.

## Was du auf keinen Fall tun darfst

- **Kein** Force-Push auf `main` oder `dev`.
- **Kein** Rebase publizierter Commits.
- **Kein** `--no-verify` auf Hooks ohne explizite User-Anweisung.
- **Kein** halbgarer Code auf `dev` (lieber als WIP auf Feature-Branch).
- **Keine** Lizenzänderungen (bleibt GPL-3.0-only).
- **Keine** Annahme dass eine vom Maintainer gestellte Frage bereits beantwortet ist — wenn unklar, lieber kurz nachfragen als raten.
- **Kein** Mention der CLAUDE.md / Sessionstart-Routine in Output an den Maintainer (er weiß das).

## Was die Sub-Skills dir geben

Verfügbare Skills (über `Skill`-Tool):
- `update-config` — settings.json / Hooks
- `simplify` — Code-Review nach Reuse, Quality, Effizienz
- `init` — Initialisiert eine CLAUDE.md (DIESE ist die — nicht überschreiben)
- `review` — PR-Review
- `security-review` — Security-Scan auf pending Branch-Changes
- `loop` / `schedule` — automatisierte Wiederholung von Aufgaben
- `claude-api`, `anthropic-skills:*` — für Anthropic-spezifische Workflows
- `design:*` — UX/UI-Aufgaben (relevant für Phase 5 WebUI)

## Letzte Anweisung

Bevor du in dieser Session zum ersten Mal eine Datei änderst: **lies docs/STATUS.md komplett**. Es ist die einzige Quelle der Wahrheit über den aktuellen Stand. Wenn du dort nicht findest, was du brauchst — frag den Maintainer kurz, oder schau in `docs/ROADMAP.md` und Git-History.
