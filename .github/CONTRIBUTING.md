# Beitragen zu Sonnenschein

Schön, dass du Sonnenschein verbessern willst. Dieses Dokument fasst alles zusammen, was du wissen musst, um einen sinnvollen PR einzureichen.

> *English-speaking contributors: scroll down for the English version. Issues and PRs are accepted in either language.*

---

## Deutsch

### Bevor du anfängst

- Lies die [README.md](../README.md) — sie sagt, was Sonnenschein ist und nicht ist.
- Lies die [Roadmap](../docs/ROADMAP.md) — sie sagt, woran wir gerade arbeiten und was bewusst aufgeschoben ist.
- Schau in die [Issues](https://github.com/Elias02345/sonnenschein/issues) — vielleicht arbeitet schon jemand an deinem Thema.

### Wo Hilfe besonders willkommen ist

- **Distro-Tests**: Du hast eine Distro, die unten unter "Unterstützte Distros" steht? Probier den Installer aus und melde, was bricht.
- **Compositor-Backends**: Wenn du Sway/Hyprland/Cosmic/Mutter/KWin tief kennst — das Virtual-Display-Backend für deinen Compositor ist immer Gold wert.
- **GPU-Tests**: HDR auf NVIDIA/AMD/Intel ist eine Reife-Frage. Reale Test-Reports sind wertvoller als Theorie.
- **Übersetzungen**: Deutsch und Englisch sind Pflicht zur 1.0. Weitere Sprachen über Crowdin (kommt mit Phase 5).
- **Dokumentation**: Setup-Guides für jede Distro, Troubleshooting-Knowledge-Base.

### Workflow

1. **Issue zuerst (für nicht-triviale Sachen).** Bevor du Tage in eine Architektur-Änderung investierst, mach ein Issue auf und diskutier den Ansatz. Spart Frust.
2. **Fork** dieses Repo, **branche** aus `dev` (nicht aus `main`).
3. **Implementiere** deine Änderung. Halte Commits klein und thematisch.
4. **Teste** lokal — siehe unten.
5. **Pull Request** gegen `dev`. Verlinke das Issue (`Fixes #123` oder `Refs #123`).
6. **Sei geduldig im Review.** Wir geben Feedback, manchmal viel. Es geht nie gegen dich persönlich.

### Branch-Konventionen

- `feature/<kurzbeschreibung>` — neue Funktion
- `fix/<kurzbeschreibung>` — Bugfix
- `docs/<kurzbeschreibung>` — nur Doku
- `refactor/<kurzbeschreibung>` — Umbau ohne Funktionsänderung

### Commit-Message-Stil

Wir folgen [Conventional Commits](https://www.conventionalcommits.org/) lose:

```
<typ>(<scope>): <kurze Zusammenfassung>

<optionaler längerer Body, was und WARUM>

<optionale Footer: Refs, Fixes, Co-authored-by>
```

`<typ>` ist eines von: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`, `perf`. `<scope>` ist optional, üblich: `installer`, `webui`, `vdisplay`, `cmake`, `nvenc`, `vaapi`.

Beispiele:
- `feat(vdisplay): add Mutter headless backend`
- `fix(installer): handle missing pacman gracefully on derivates`
- `docs(roadmap): clarify HDR phase scope`

### Code-Stil

- **C++**: `clang-format` mit der Projekt-Config (`.clang-format` aus Apollo geerbt). Vor PR: `clang-format -i $(geänderte Dateien)`.
- **Vue/JS**: `eslint` + `prettier` (`.prettierrc.json` aus Apollo geerbt). `npm run lint` (sobald in Phase 5 eingeführt).
- **Bash**: `shellcheck` muss grün sein. Strict-Mode (`set -euo pipefail`) ist Pflicht.
- **CMake**: konsistent in der Indentation, lieber kleinere Module als Mega-Files.

### Lokales Testen

Sonnenschein baut aktuell nur auf Linux. Auf Windows: nutze WSL2 (`wsl --install -d Ubuntu-24.04`).

```bash
# Submodule holen
git submodule update --init --recursive

# Build (Beispiel: Ubuntu 24.04 mit AMD-GPU)
mkdir -p build && cd build
cmake -DSUNSHINE_ASSETS_DIR=assets ..
cmake --build . -j$(nproc)

# Run (entwicklungs-Modus, ohne Installation)
./sonnenschein
```

Detaillierte Distro-spezifische Build-Anleitungen kommen in `docs/building.md` (geerbt aus Sunshine, wird in Phase 1 angepasst).

### Pull-Request-Checkliste

- [ ] Branche aus `dev`, nicht aus `main`.
- [ ] Code formatiert (clang-format / prettier).
- [ ] Lokal getestet.
- [ ] CI ist grün (oder Fehler kommentiert).
- [ ] Verknüpftes Issue erwähnt.
- [ ] Bei Verhaltensänderungen: ROADMAP / docs aktualisiert.
- [ ] Bei neuen Pfaden: gefährliche Operationen (Datei-Löschung, Service-Restart) sind klar gekennzeichnet.

### KI-generierter Code

KI-generierter Code ist erlaubt — aber:
- Du musst jede Zeile verstanden und nachvollzogen haben.
- Du musst getestet haben.
- Du bist verantwortlich, als hättest du es selbst geschrieben.

Wir lehnen PRs ab, die offensichtlich aus einem Modell rausgekippt sind ohne Verifikation.

### Verhaltensregeln

Siehe [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Kurz: respektvoll bleiben, im Zweifel nett sein.

### Lizenz

Mit deinem Beitrag stimmst du zu, dass dein Code unter GPL-3.0-only veröffentlicht wird.

---

## English

### Before you start

- Read the [README](../README.md) — it tells you what Sonnenschein is and isn't.
- Read the [Roadmap](../docs/ROADMAP.md) — it tells you what we're working on and what is intentionally deferred.
- Check [Issues](https://github.com/Elias02345/sonnenschein/issues) — someone might already be on it.

### Where help is especially welcome

- **Distro tests**: Got a distro listed under "Supported distros"? Try the installer and report what breaks.
- **Compositor backends**: Deep knowledge of Sway/Hyprland/Cosmic/Mutter/KWin? A virtual-display backend for your compositor is gold.
- **GPU tests**: HDR on NVIDIA/AMD/Intel is a maturity question. Real test reports beat theory.
- **Translations**: German + English are mandatory at 1.0. Other languages via Crowdin (Phase 5).
- **Documentation**: per-distro setup guides, troubleshooting knowledge base.

### Workflow

1. **Issue first** for non-trivial work. Don't invest days into an architecture change without aligning first.
2. **Fork** this repo, **branch** from `dev` (not `main`).
3. **Implement**. Keep commits small and topical.
4. **Test** locally.
5. **Pull request** against `dev`. Link the issue (`Fixes #123` or `Refs #123`).
6. **Be patient in review.** We will give feedback, sometimes a lot. It is never personal.

### Commit message style

We loosely follow [Conventional Commits](https://www.conventionalcommits.org/):
`<type>(<scope>): <summary>` followed by optional body and footers. Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`, `perf`.

### Code style

- **C++**: `clang-format` (config inherited from Apollo).
- **Vue/JS**: `eslint` + `prettier`.
- **Bash**: `shellcheck` must pass; strict mode (`set -euo pipefail`).

### License

By contributing, you agree your code is published under GPL-3.0-only.
