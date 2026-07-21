# RELEASE_RULES.md — Verbindlicher Release-Ablauf

Diese Regeln gelten für alle Sonnenschein-Host-, Client- und Decky-Releases.
Sie ergänzen `UPDATE_RULES.md`; der tagesaktuelle Arbeitsstand bleibt in
`STATUS.md`.

## Unveränderliche Grundregeln

- Test-Releases sind unveränderliche Tags `vX.Y.Z-test` auf einem vollständig
  verifizierten `dev`-Commit. Stabile Tags `vX.Y.Z` entstehen erst nach
  bestätigtem Test und Merge von `dev` nach `main`.
- Tags und veröffentlichte Assets werden niemals nachträglich verschoben,
  ersetzt oder unter demselben Namen neu gebaut.
- Vor dem Tag werden alle Runtime-/Paketversionen gemeinsam aktualisiert:
  mindestens `client/app/version.txt` und `decky-plugin/package.json`.
- Jede nutzersichtbare Änderung steht vor dem Tag in `CHANGELOG.md` und im
  aktuellen Abschnitt von `docs/STATUS.md`.
- Bei öffentlichen Releases werden signierte annotierte Tags verwendet, sofern
  der lokale Signing-Key verfügbar ist. Fehlt er, wird nicht still auf einen
  unsignierten Tag ausgewichen; der Blocker wird dem Maintainer gemeldet.

## Reihenfolge vor dem Tag

1. Die kleinsten zielgerichteten Tests zuerst ausführen, danach Lint/Build und
   die für den geänderten Pfad relevanten End-to-End-Harnesses.
2. Decky-Releases zusätzlich prüfen auf:
   - erfolgreicher TypeScript-/Rollup-Build,
   - `SP_JSX` statt nackter `React.createElement`-Referenzen,
   - exaktes Zip-Layout `sonnenschein/{plugin.json,package.json,main.py,
     sonnenschein-run.sh,dist/index.js,LICENSE}`,
   - Import und Methodenaufrufe durch die echte Decky-Loader-Maschinerie,
   - Frozen-Python-Kompatibilität und Offline-/Timeout-Verhalten.
3. Client-Releases zusätzlich auf einkompilierte Version, Discovery-/Config-
   Kompatibilität und die plattformspezifischen Paketnamen prüfen.
4. Änderungen einschließlich `CHANGELOG.md` und `STATUS.md` auf `dev`
   committen und pushen. Erst wenn die `dev`-CI vollständig grün ist, darf der
   Release-Tag auf genau diesem Commit erzeugt und gepusht werden.

## Veröffentlichung und Nachprüfung

1. Der Tag-Workflow muss vollständig grün sein und mindestens AppImage,
   nativen Windows-Installer, macOS-DMG, Decky-Plugin-Zip und die vorgesehenen
   Debug-Symbole veröffentlichen.
2. Die Release-Seite erhält vor dem Upload eine kuratierte englische
   Beschreibung aus dem passenden `CHANGELOG.md`-Abschnitt: kurze Testbuild-
   Warnung, nutzersichtbare Änderungen, Downloadtabelle für AppImage, Decky,
   Windows und macOS, Link auf `docs/steam-deck.md`, verifizierter One-Shot-
   Installationsbefehl und Test-/Auditstand. Automatisch generierte Commit-
   Notizen dürfen ergänzen, ersetzen diese Beschreibung aber nicht.
3. Nicht nur CI-Artefakte prüfen: die tatsächlich veröffentlichten GitHub-
   Release-Assets herunterladen, Dateinamen/Größe/Version/Zip-Struktur und
   Prüfsummen kontrollieren. Das veröffentlichte Decky-Zip erneut durch Bundle-
   Audit und Loader-/Frozen-Python-Harness laufen lassen.
4. Genau das neue getestete Release muss `releases/latest` sein, weil
   `deck-install.sh` und der Client-Updater davon abhängen. Den direkten
   Plattformdownload des Client-Updaters und die Plugin-Zip-Auflösung des
   Installers prüfen.
5. Erst nach erfolgreicher Asset-Prüfung das vorherige Test-Release als
   Pre-Release/überholt markieren und auf das neue Release verweisen.
6. Abschließend `STATUS.md` mit Release-URL, Actions-Run-ID, Commit/Tag,
   Prüfsummen bzw. Prüfergebnissen und dem verbleibenden Hardware-Teststand
   aktualisieren und auf `dev` pushen.

Ein Release darf niemals als „live und verifiziert“ dokumentiert werden,
bevor die veröffentlichten Assets selbst geprüft wurden.
