# Sonnenschein — Aktueller Projektstand (STATUS.md)

> **Lebendes Dokument.** Diese Datei wird in jeder Session nach jedem
> abgeschlossenen Schritt aktualisiert. Sie ist die einzige Quelle der
> Wahrheit für Multi-Session-Arbeit. Wenn etwas hier fehlt, weiß die
> nächste Session es nicht.

> ## ⏸ HIER WEITERMACHEN (2026-07-16, Session auf dem CachyOS-Target)
>
> **📋 MAINTAINER-PRÄZISIERUNG (2026-07-16, Runde 2)**: RD-Tests macht der
> Maintainer auf dem **Windows-Laptop mit der .exe** (nicht am CachyOS-PC) —
> es braucht also downloadbare GitHub-Builds mit M1+M2. **Deck = nur
> Gaming-Version + Decky-Plugin** (keine RD-Features nötig). Auftrag: alles
> fertig bauen → **ein Release mit allen Apps** (Windows: **nativer
> Installer statt zip**, klar als Client benannt) + **Deck-Anleitung auf
> GitHub** (2 Teile: Client-App + Decky-Plugin). Danach eine große
> Testrunde (Gaming am Deck, RD auf Windows).
> **Decky-Scope-Entscheidung**: **Voll-Sync automatisch** — Host-Spiele
> werden automatisch in die Steam-Bibliothek gesynct (diff-basiert,
> Restart-Helper als Sicherheitsnetz).
>
> **🟡 RUNDE-2-UMSETZUNG (2026-07-16, alles auf `dev`, CI-Ergebnis offen):**
> 1. **M1+M2 auf `dev` gemerged** (`85bfbce`) — Sichttest verlagert in die
>    große Testrunde (Windows-Laptop).
> 2. **M4/M5 (`1bdf189`)**: WiX-Bundle → „Sonnenschein Client" (eigene
>    Upgrade-GUIDs, x64-only, Install nach `Program Files\Sonnenschein
>    Client`, Registry/AppData unter Sonnenschein), Output
>    **`SonnenscheinClientSetup-<ver>.exe`**, CI lädt Installer statt zip
>    hoch; dmg → `Sonnenschein_Client-<ver>.dmg`; **AutoUpdateChecker →
>    GitHub `releases/latest`** (Button zeigt direkten Plattform-Download,
>    Windows = Installer; Versionsparser toleriert `-test`-Suffixe);
>    **eigene Versionslinie `0.1.0`** (version.txt, war Moonlights 6.1.0).
> 3. **M6 (`81b446a`)**: **Decky-Plugin** (`decky-plugin/`) — Python-Backend
>    liest das Pairing der Client-App (`~/.config/Sonnenschein/…conf`:
>    certificate/key/uniqueid/hosts, QSettings-INI-Format verifiziert) und
>    spricht die cert-auth Moonlight-API (`/applist`, `/appasset`); Frontend
>    (React/TS, `@decky/ui`+`@decky/api`) **auto-synct** Host-Spiele als
>    Non-Steam-Shortcuts (diff-basiert, Boxart, Launch-Options-Env →
>    `sonnenschein-run.sh` → Client-AppImage aus `~/Applications`); CI-Job
>    `build-decky-plugin` (pnpm 9 + rollup) baut das Plugin-Zip. **Doku:
>    `docs/steam-deck.md`** (2-Teile-Anleitung, im README verlinkt).
>    Release-Job: exe/zip direkt als Assets, Tag-Name als Version.
> 4. **✅ RELEASE v0.1.0-test LIVE** (2026-07-16, Run 29497827949 komplett
>    grün): <https://github.com/Elias02345/sonnenschein/releases/tag/v0.1.0-test>
>    — `SonnenscheinClientSetup-0.1.0-test.exe` (nativer Win-Installer, 26 MB),
>    AppImage, dmg (jetzt `Sonnenschein_Client-*`), Decky-Plugin-Zip
>    (Struktur verifiziert: `sonnenschein/` mit plugin.json/main.py/dist/
>    Runner/LICENSE), Release-Notes mit Download-Tabelle + Deck-Anleitung.
>    **→ Jetzt: große Testrunde des Maintainers** (Deck: AppImage +
>    Decky-Plugin nach docs/steam-deck.md; Windows-Laptop: Installer,
>    Easy/Advanced, RD-Abfrage, Update-Banner beim nächsten Release).
> **Decky-Plugin komplett ungetestet auf echter Hardware** (kein Deck-Zugriff
> aus der Session) — SteamClient-API-Aufrufe (AddShortcut etc.) sind nach
> MoonDeck-Vorbild implementiert, erste Fehlerrunde am Deck einplanen.
>
> **🔧 DECK-FEHLERRUNDE 1 (2026-07-16, Maintainer-Test)**: Plugin crashte
> beim Laden mit `ReferenceError: React is not defined`
> (PluginLoader.importReactPlugin). **Ursache verifiziert** (lokaler
> Rebuild + @decky/rollup-Quellcode + Template-Recherche): tsconfig hatte
> `"jsx": "react"` (classic) → Bundle enthielt nackte `React.createElement`-
> Referenzen; @decky/rollup mappt aber nur Imports auf die Loader-Globals
> (`react`→SP_REACT, **`react/jsx-runtime`→SP_JSX**, `@decky/ui`→DFL) und
> Steams CEF hat kein `window.React`. **Fix `c2913df`**: `"jsx": "react-jsx"`
> (exakt wie decky-plugin-template + MoonDeck) + Deps template-konform
> (@types/react 19.x — Steam shipped React 19; tslib/react-icons/@decky/api
> als runtime-deps). Lokal verifiziert: Bundle nutzt SP_JSX (17 Stellen),
> null nackte React-Referenzen. Weitere Template-Checks bestanden:
> `api_version: 1` (Pflicht für @decky/api-callable-Dispatch, positional
> args auf Plugin-Klasse), `"type": "module"` (ESM-Import-Pfad), Zip-Layout
> (plugin.json auf Tiefe 1), decky-Konstanten existieren alle.
> → **✅ RELEASE v0.1.1-test LIVE** (Run 29502085858 komplett grün):
> <https://github.com/Elias02345/sonnenschein/releases/tag/v0.1.1-test> —
> alle 4 Apps + Debug-Symbols; **veröffentlichtes Plugin-Bundle
> nachgeprüft** (SP_JSX ×17, kein React.createElement). v0.1.0-test als
> Pre-Release markiert mit Hinweis auf v0.1.1-test (damit `releases/latest`
> und der Client-Update-Check auf die gefixte Version zeigen).
> **Testrunde des Maintainers startet mit v0.1.1-test.**
>
> **🔧 DECK-FEHLERRUNDE 2 (2026-07-16)**: Plugin hing bei „Verbinde…".
> **Backend diesmal komplett lokal E2E-getestet** (Harness mit decky-Stub
> gegen echte Client-Config + laufenden Host auf dem CachyOS-Rechner) —
> 3 echte Bugs gefunden + gefixt (`0031447`):
> 1. **Conf-Parser las die Hosts nicht**: reale QSettings-INI hat eine
>    eigene `[hosts]`-Sektion mit `N\feld`-Keys (Feld heißt `hostname`,
>    nicht `name`) — Parser war auf `hosts\N\name` unter [General] gebaut
>    → 0 Hosts → paired=false.
> 2. **Falscher Port**: gespeichert ist der HTTP-Port (47989); die
>    cert-auth-API läuft auf HTTPS — jetzt live via unauth `/serverinfo`
>    aufgelöst (wie der Client, `HttpsPort`-Feld), Fallback Port−5.
> 3. **Fehler unsichtbar**: Moonlight-Hosts liefern Fehler als
>    `status_code`-Attribut bei HTTP 200 (401 = nicht gepairt) — wird
>    jetzt als klare Meldung geworfen (lokal verifiziert, da hiesiger
>    Client nur mit der alten Test-Instanz gepairt ist).
> **Frontend kann nicht mehr hängen**: Ping-Probe (6 s) unterscheidet
> totes Backend von Host-Problemen, alle Calls mit Deadline, Fehler
> inline im Panel mit Retry-Button (statt verpassbarem Toast), Backend
> loggt Import-OK. App-Parsing zusätzlich synthetisch getestet.
> **🔧 DECK-FEHLERRUNDE 3 (2026-07-19)**: v0.1.2 meldet sauber „Backend
> antwortet nicht" (Frontend-Härtung wirkt) — das Python-Backend kommt auf
> dem Deck also nie hoch. **Beweis-Arbeit dieser Runde**:
> 1. Das veröffentlichte Plugin durch die **exakte Import-Maschinerie des
>    Loaders** geladen (echtes decky-Modul aus dem Loader-Repo, env-
>    Injektion, spec_from_file_location, Plugin(), Dispatch) — läuft
>    fehlerfrei inkl. Host-Zugriff. **Der Plugin-Code ist es nicht.**
> 2. Recherche im Loader-Quellcode (Stable v3.2.6): Backend-Startup-Crash
>    ⇒ stilles `sys.exit(0)` vor dem Socket-Server ⇒ **alle callables
>    hängen ewig ohne Fehler** — exakt unser Symptom. Traceback landet NUR
>    in `journalctl -u plugin_loader`. Häufigste Real-Ursachen: Reste
>    alter Versionen, **root-owned Files nach sudo-Unzip** (Loader-eigener
>    Perm-Fixer überspringt root-owned Ordner!), fehlender Loader-Restart.
>    `import decky` existiert seit v3.0.0, Flags sind egal (MoonDeck: []).
>    **Backend-less keine Option**: Loader-fetchNoCors verifiziert TLS
>    strikt (certifi) — selbstsignierte Host-Zertifikate scheitern.
> 3. Fixes (`d80891e`): main.py minimale Import-Fläche (alles lazy) +
>    decky_plugin-Fallback; Frontend pingt 3×; **neues
>    `decky-plugin/deck-install.sh`** — sauberer Reinstall (Loader-Stop,
>    Reste+__pycache__ weg, Latest-Release-Zip, deck-owned Perms) und
>    **beweist danach den Backend-Zustand**: Prozess-Check, Journal-Grep
>    mit Traceback-Ausgabe, Plugin-Log. Ein-Zeiler in steam-deck.md
>    (curl | bash von raw.githubusercontent). Loader-Maschinerie-Test
>    erneut grün. → **✅ RELEASE v0.1.3-test LIVE** (2026-07-19, Run
>    29694763009 grün): veröffentlichtes Zip erneut durch die
>    Loader-Maschinerie verifiziert; v0.1.2 als überholt markiert.
>    **Maintainer-Testpfad: Install-Script im Desktop-Modus ausführen**
>    — es beweist den Backend-Zustand; falls rot, liefert es den
>    Traceback für Fehlerrunde 4.
>
> **🎯 DECK-FEHLERRUNDE 4 (2026-07-19) — ROOT CAUSE GEFUNDEN**: Das
> Install-Script hat wie designed den Original-Traceback geliefert:
> **`ModuleNotFoundError: No module named 'xml.etree'`**. Der Decky-Loader
> shippt ein **PyInstaller-frozen Python**, das nur die vom Loader selbst
> genutzten Stdlib-Module bündelt — `xml.etree` fehlt (darum liefen alle
> lokalen System-Python-Tests durch!). Das war von Anfang an die Ursache
> von „Backend antwortet nicht". Beweisbar OK sind alle anderen genutzten
> Module (v0.1.2-Traceback: alle Imports VOR xml.etree kamen durch).
> Zweiter Script-Bug: `~/homebrew/plugins` ist **root-owned** → unzip ohne
> sudo scheiterte still. **Fixes (`e314f61`)**: applist/serverinfo-Parsing
> via `re` + manuellem Entity-Unescape (kein xml-Import mehr im ganzen
> Backend); Script nutzt sudo-unzip + Loader-Ownership-Konvention
> (Inhalte deck, Top-Dir root), räumt alte Logs, prüft plugin.json vor
> dem Verdikt. **Verifiziert mit Frozen-Python-Simulation** (meta_path-
> Blocker: xml-Import wirft) — kompletter Loader-Maschinerie-Lauf inkl.
> Live-serverinfo/applist gegen den echten Host + synthetischem
> 200er-Parse mit Entities. → **✅ RELEASE v0.1.4-test LIVE** (2026-07-19,
> Run 29696225160 grün): veröffentlichtes Zip unter Frozen-Python-
> Simulation erneut komplett verifiziert; v0.1.3 als überholt markiert.
> **Lektion für alle künftigen Decky-Backends**: nur Stdlib-Module
> verwenden, deren Präsenz im Loader-Python belegt ist (os, json, re,
> ssl, http.client, urllib.parse, uuid, base64, glob, tempfile ✓;
> xml.etree ✗) — oder Module nach MoonDeck-Art selbst bündeln.
>
> **🎯 DECK-RUNDE 5 (2026-07-19, Maintainer-Test v0.1.4)**: **Backend läuft
> jetzt** — Panel zeigt die Host-Spiele ✓. Zwei Befunde: (1) **Client-App
> wird nicht gefunden** (Panel-Warnung), obwohl die AppImage in
> ~/Applications liegt und startbar ist → die gesyncten Shortcuts sind
> „völlig leere Platzhalter" und schließen sich sofort (Runner findet
> keinen Client, Exit 1). (2) **Maintainer-Vision präzisiert (verbindlich)**:
> Spiele, die in der Deck-Steam-Bibliothek **sichtbar** sind (installiert
> ODER nur im Account/Download-Button, Beispiel Portal), bekommen auf der
> nativen Spieleseite **einen zusätzlichen Button „Stream via Sonnenschein"**
> neben Spielen/Installieren — kein Platzhalter-Duplikat, fühlt sich wie
> nativer Start an. Nur Host-Spiele, die gar nicht in der Bibliothek sind,
> werden (wie bisher) als Einträge mit Artwork angelegt.
> **Umsetzungsplan Runde 5**: (a) Client-Discovery robust (mehr Pfade,
> case-insensitiv, Datei-Picker via openFilePicker, Pfad in Settings +
> `SONNENSCHEIN_CLIENT` in den Launch-Options); (b) Game-Page-Button via
> routerHook-Patch nach MoonDeck-Vorbild (GPL-3), Matching Host-Spiel ↔
> Deck-appid via Titel (v1; Host-seitiges SteamAppId-Feld in applist als
> späteres Upgrade notiert); Start über EIN wiederverwendetes verstecktes
> Stream-Shortcut (Launch-Options vor RunGame umgeschrieben);
> (c) Shortcut-Sync nur noch für Nicht-Bibliotheks-Spiele.
> **Ausführung: Sonnet-Subagenten implementieren, Fable orchestriert +
> verifiziert (Maintainer-Anweisung, Kosten sparen).**
>
> **🟡 RUNDE 5 UMGESETZT (`0b5ae6e`, Deck-Test offen)**: (a) Backend:
> Client-Discovery case-insensitiv über Applications/Downloads/Desktop/
> .local/bin/HOME + manueller Pfad via Datei-Picker (persistiert in
> Plugin-Settings) + `SONNENSCHEIN_CLIENT` in allen Launch-Options +
> Diagnose-Liste gefundener AppImages im Panel (von mir). (b) Frontend
> (Sonnet-Agent nach MoonDeck-Quellcode-Recherche, von mir reviewt):
> `steamlib.ts` (Titel-Normalisierung, Bibliotheks-Matching über
> `appStore.m_mapApps` nur app_type 1, EIN verstecktes wiederverwendetes
> „Sonnenschein Stream"-Shortcut mit Launch-Options-Rewrite vor RunGame),
> `gamepage.tsx` (Route-Patch `/library/app/:appid` nach MoonDeck-Rezept,
> Button nur bei Host-Treffer), Sync erstellt nur noch Shortcuts für
> Nicht-Bibliotheks-Spiele (Diff räumt jetzt-gematchte automatisch ab).
> Review-Fix von mir: Guard gegen leeren runnerPath im Stream-Shortcut.
> Verifiziert: Build grün, Bundle nur SP_JSX, Backend durch Loader-
> Maschinerie (frozen-python) inkl. neuer Methoden end-to-end.
> **Nur am Deck testbar**: Game-Page-Patch-Rendering, Titel-Matching
> gegen echte Bibliothek, RunGame-Fokus. Sonnet-Agent musste 1× nach
> Session-Limit per SendMessage fortgesetzt werden (Kontext blieb).
> **✅ RELEASE v0.2.0-test LIVE** (2026-07-20, Run 29707037957 grün):
> veröffentlichtes Zip verifiziert (Frontend-Strings + Backend durch
> Loader-Maschinerie), v0.1.4 als überholt markiert. Maintainer-Test:
> Install-Script → Deck-Neustart → Spieleseite eines Host-Spiels
> (z. B. Portal) → „Stream via Sonnenschein"-Button.
>
> **🎯 DECK-RUNDE 6 (2026-07-20) — echter Deck-Test, 3 Befunde + Root-Cause
> gefunden.** Maintainer-Report: Button zu groß/UI kaputt; Klick öffnet
> Client, zeigt „Starting Portal", schließt sofort ohne zu streamen,
> UND legt ein leeres Platzhalterspiel „Sonnenschein Stream" an;
> außerdem Wunsch nach Verfügbarkeits-LED am Button. **Auf Maintainer-
> Anweisung: Fable als Opus-Rolle orchestriert + verifiziert, Sonnet-
> Subagenten implementieren (Kostenersparnis).**
>
> **Vollständige Code-Analyse (von mir, C++/QML kritisch):**
> 1. **Root Cause „schließt sofort, kein Fehler"**: `AutoConfig::
>    applyEasyMode()` (M1, Easy-Mode-Default) lief bei JEDEM frischen
>    CLI-Prozess — jeder Decky-Stream-Start ist einer — eine VOLLSTÄNDIGE
>    Hardware-Decoder-Sondierung (bis zu 9 Decoder-Erstellen/Zerstören-
>    Zyklen über ein Wegwerf-Testfenster), UNMITTELBAR gefolgt von
>    `Session::initialize()`s EIGENER, ebenfalls vollständiger Decoder-
>    Initialisierung mit einem ZWEITEN Testfenster — doppelter GPU-/
>    Decoder-Auf-/Abbau im selben Prozess, direkt hintereinander, genau
>    beim heikelsten Moment des Stream-Starts. Auf dem PC nie aufgefallen
>    (Prozess läuft über mehrere manuelle Starts weiter, Sondierung ist
>    einmalig gecacht) — CLI/Decky erzwingt hingegen jedes Mal einen
>    frischen Prozess. **Unabhängig davon**: alle drei Fehlerpfade in
>    `Session::initialize()` (SDL-Video-Init, Testfenster-Erstellung,
>    Decoder-Eigenschaften-Population via `populateDecoderProperties`)
>    haben NIE das existierende `displayLaunchError`-Signal gefeuert —
>    zusammen mit `quitAfter=true` (CLI-Start) macht das JEDEN Fehler zu
>    100% unsichtbar: die App beendet sich ohne jede Anzeige. Das erklärt
>    exakt das gemeldete Symptom.
> 2. **Root Cause „Platzhalterspiel Sonnenschein Stream"**: Das
>    gemeinsame, wiederverwendete Shortcut hat nie einen Namen/Artwork
>    des Zielspiels — Steams eigener „App wird gestartet"-Übergangsbild-
>    schirm zeigt daher zwangsläufig den generischen Platzhalter.
> 3. **Button-Styling**: Sonnet-Agent hatte generischen `DialogButton`
>    mit eigenem Markup gesplict statt Steam-eigene Klassen/Größe zu
>    nutzen wie MoonDeck.
>
> **Fixes (`776cd7d`, `feature/client-stream-fixes`, von mir selbst
> gemacht — C++/QML-kritisch):**
> - Neue `AutoConfig::detectProfileCheap()`: nur Display-Geometrie
>   (Auflösung/Refresh), KEINE Decoder-Erstellung. Easy Mode ruft jetzt
>   diese statt der vollen `detectProfile()`. Volle Sondierung bleibt
>   exklusiv dem eigenständigen `detect-profile`-Diagnosebefehl
>   vorbehalten (verifiziert unverändert: läuft weiterhin die volle
>   Multi-Codec-Sondierung, ~30 s auf dieser Dev-Maschine ohne echte
>   Deck-GPU-Decoder).
> - Alle drei stillen Fehlerpfade in `Session::initialize()` feuern jetzt
>   `displayLaunchError` mit Log-Verzeichnis-Hinweis.
> - DE-Übersetzung ergänzt (278/278).
> - Verifiziert: Client-Rebuild sauber, `detect-profile`-CLI-Befehl
>   unverändert voll funktional.
>
> **Frontend-Fixes (`cb5ab29`, Sonnet-Agent, von mir reviewt + selbst
> verifiziert):**
> - **Button**: MoonDeck nutzt für die Größe/Platzierung einen
>   MutationObserver-Mechanismus (zu riskant zum 1:1-Portieren); daraus
>   übernommen: (1) Steams eigenes `DialogButton`-CSS gewinnt gegen
>   Inline-Styles → gescopte `!important`-Style-Klasse fixiert Höhe
>   (40px)/Padding/Margin, (2) Button wird als Geschwister-Element NEBEN
>   dem bestehenden Play/Install-Panel gerendert (das App-Panel-Element
>   wird in eine Flex-Row mit unserem Button gewrappt) statt als
>   zusätzliche volle Zeile drunter gesplict — Splice-Fallback bleibt
>   für den Fall, dass Steam-UI-Drift das App-Panel nicht mehr findet.
>   Label auf „Stream" gekürzt (analog „Play"/„Install"). Nebenbei einen
>   Rules-of-Hooks-Bug gefixt (früher `return null` vor einem Hook).
> - **Rename**: `streamGame()` ruft jetzt `SetShortcutName(appId, title)`
>   direkt vor `SetAppLaunchOptions` — Steams Start-Übergangsbildschirm
>   zeigt „Portal" statt „Sonnenschein Stream".
> - **Verfügbarkeits-LED**: neue Backend-Methode
>   `check_host_available(address, port)` (unauth `/serverinfo`-Probe,
>   gleiches Muster wie `_https_port`, kein `xml.etree`), Frontend pollt
>   alle 8s: grüner Punkt (erreichbar+frei), orange (erreichbar+
>   beschäftigt), rot (nicht erreichbar).
> - **Verifiziert von mir**: Build grün, Bundle nur SP_JSX, Backend durch
>   Loader-Maschinerie inkl. `check_host_available` live gegen den
>   echten Host UND gegen eine unerreichbare Adresse getestet — beide
>   Fälle korrekt.
> **Beide Feature-Branches gemerged auf `dev`** (`57bab50` Client,
> `cad173f` Decky). Nach CI-Grün → Release v0.2.1-test.
> **Bekanntes Risiko (nur am Deck prüfbar)**: das Flex-Row-Wrapping des
> App-Panels ist rein visuell nur auf echter Hardware verifizierbar.
>
> **✅ RELEASE v0.2.1-test LIVE** (2026-07-20, Run 29726550816 grün):
> veröffentlichtes Zip verifiziert (Bundle enthält alle Fixes, Backend
> durch Loader-Maschinerie), v0.2.0 als überholt markiert.
> **Arbeitsweise dieser Runde (Maintainer-Vorgabe)**: Fable analysiert/
> orchestriert/verifiziert, Sonnet-Subagent implementiert Frontend;
> C++/QML-kritischer Teil (Session::initialize, AutoConfig) von Fable
> selbst umgesetzt.
>
> **🔧 DECK-RUNDE 7 (2026-07-20): `deck-install.sh` zeigte irreführenden
> Fehler beim Update.** Maintainer-Report: „Could not find the plugin
> zip in the latest release". **Root Cause sofort reproduziert** (von
> mir, ein einzelner unauthentifizierter curl-Call): die unauthenti-
> fizierte GitHub-API erlaubt nur 60 Requests/Stunde pro IP — bei
> Erschöpfung antwortet `api.github.com` mit HTTP 403 + Rate-Limit-JSON,
> das der alte Code ungeprüft in den `browser_download_url`-Grep
> pipete → leeres Ergebnis → irreführende generische Meldung statt der
> echten Ursache. **Fix von Codex-Subagent (GPT-5.6, `cd58f54`)**: HTTP-
> Status + Body getrennt erfasst, bis zu 4 Versuche mit 15/30/60s
> Backoff, optionaler `GITHUB_TOKEN`-Header für höheres Limit (5000/h),
> bei endgültigem Fehlschlag echter HTTP-Status + Body-Ausschnitt statt
> der generischen Meldung. **Von mir verifiziert**: isolierte Stub-Tests
> für Rate-Limit-dann-Erfolg UND permanenten Fehlschlag (beide korrekt),
> plus ein echter authentifizierter Call gegen die reale GitHub-API mit
> der neuen Logik (Status 200, `v0.2.1-test` + Plugin-Zip korrekt
> erkannt). **Kein neues Release nötig** — das Script wird live von
> `dev` per `raw.githubusercontent.com` geladen, der Fix ist sofort
> wirksam.
>
> **🔴 DECK-RUNDE 8 (2026-07-20, Regression in aktueller Testversion):**
> Maintainer meldet nach Installation der neuesten Client-AppImage UND des
> neuesten Decky-Plugins: Host wird nicht gefunden, Bibliothek wird nicht
> synchronisiert, Spiele starten nicht und der zuvor vorhandene
> „Stream via Sonnenschein"-/„Stream"-Button fehlt auf nativen
> Steam-Spieleseiten. Auftrag: Regression vollständig gegen den letzten
> funktionierenden Stand analysieren, beheben, auditieren und verifizieren;
> erst danach Commit + Push + neue Testversion im bestehenden Release-Stil.
> **Verbindliche UX-Regel:** streambare Spiele tragen bereits in der
> Bibliothekskapsel ein kleines Sonnenschein-Symbol. Auf der Spieleseite
> steht der Sonnenschein-Stream-Button neben Steams Play/Install/Download-
> Element. Sein Indikator ist grün, wenn der Host erreichbar und frei ist,
> sonst rot (offline ODER ein anderes Spiel läuft). Der rote Zustand ist
> rein informativ und blockiert keinen manuellen Startversuch. Vor der
> Implementierung wird außerdem der reale Release-/Update-Ablauf auditiert
> und als dauerhafte Projektregel dokumentiert.
>
> **🟡 DECK-RUNDE 8 — DECKY-ROOT-CAUSE + FIX IMPLEMENTIERT (Release ausstehend):**
> Das veröffentlichte v0.2.1-Zip wurde heruntergeladen und gegen den Tag
> geprüft (SHA-256 `59942154…f89f9da`, `main.py` bytegleich mit Tag). Gemeinsame
> Nachgewiesene Ursache für den zusammenhängenden Decky-Ausfall: Deckys Backend nutzt EINEN
> asyncio-Event-Loop, unsere `async`-Methoden führten aber synchrones
> `http.client`-I/O aus. Schon `get_apps` konnte bei offline/langsamem Host
> ~15 s alles blockieren; v0.2.1 ergänzte zusätzlich pro Button eine
> blockierende 5-s-Statusprobe alle 8 s. Parallel wartete das Frontend nur
> 6 s auf `ping` → Backend wirkte tot, Katalog blieb leer. Zweiter
> deterministischer Bug: `hostGameIndex` war eine nackte globale Map; ein
> nachträglicher async Katalog-Load löste keinen React-Rerender aus → eine
> früh geöffnete Spieleseite behielt dauerhaft keinen Button.
> **Fix:** sämtliches Host-Netzwerk-I/O läuft über einen Frozen-Python-
> kompatiblen Worker außerhalb des Loader-Loops; Availability-Polls können
> nicht überlappen; der bestehende Spieleindex ist via `useSyncExternalStore`
> abonnierbar; Route-Patches werden dedupliziert und beim Unload vollständig
> entfernt; Route-Patch-Fehler können das QAM-Plugin nicht mehr abwürgen.
> Streambare native Steam-Spiele erhalten per defensivem DOM-Observer ein
> Sonnen-Badge. Der Spielseiten-Indikator berücksichtigt `<currentgame>`:
> offline oder mit einem anderen Spiel beschäftigt = rot, erreichbar und frei
> (oder bereits dasselbe Spiel aktiv) = grün; Start bleibt immer erlaubt.
> Regression-Harness beweist parallele langsame Calls + sofortigen Ping,
> Worker-Fehlerweitergabe und Busy-Spiel-ID unter blockiertem `xml.etree`;
> Rollup sauber (SP_JSX, keine nackte React-
> Referenz); echte Loader-Maschinerie importiert Backend, liest reale
> Client-Config und erkennt Pairing/Host. Live-Host aus Sandbox nicht routbar
> (`No route to host`) — Offlinepfad bleibt responsiv.
> **AppImage-Audit:** Die tatsächlich veröffentlichten v0.2.0/v0.2.1-
> AppImages wurden extrahiert und verglichen. QSettings-Identität, Host-
> Discovery-/Pairing-Code und Paketlayout sind unverändert; der Quell-Diff
> betrifft nur Easy-Mode-Decoderinitialisierung/Übersetzung, nicht die
> Hostsuche. Beide Binärdateien enthalten dieselbe QSettings-/Host-Identität;
> ein headless CLI-Lauf scheitert bei beiden bereits identisch an der fehlenden
> Grafik-/DRM-Umgebung und kann deshalb keinen realen Hostvergleich ersetzen. Damit
> ist eine eigenständige AppImage-Discovery-Regression derzeit **nicht
> belegt**; ein realer Deck+Host-Test ist vor der Freigabe zwingend.
> Dauerhafte Release-
> Regel: `docs/RELEASE_RULES.md`; Versionsdrift behoben (Client+Plugin 0.2.2),
> `CHANGELOG.md` angelegt, Release-Workflow verlangt vollständiges Asset-Set.
> **Noch vor Tag:** unabhängiges Code-Audit + kompletter lokaler Paket-/Loader-
> Audit, danach echter Deck+Host-Test. Erst bei vollständig grünem Ergebnis:
> Commit/Push, grüne `dev`-CI und Tag `v0.2.2-test`.
> **Release-Entscheidung 2026-07-20:** Der Maintainer hat ausdrücklich die
> Veröffentlichung als Testbuild angefordert, damit genau dieser fehlende
> Deck+Host-E2E durchgeführt werden kann. `v0.2.2-test` bleibt daher bis zur
> Hardware-Rückmeldung unbestätigt. Mangels lokal konfiguriertem Signing-Key
> wird dieser Test-Tag auf explizite Anweisung im bisherigen leichtgewichtigen,
> unsignierten Tag-Stil veröffentlicht; stabile Releases bleiben davon
> unberührt.
>
> → **✅ RELEASE v0.1.2-test LIVE** (2026-07-17, Run 29537965862 grün):
> <https://github.com/Elias02345/sonnenschein/releases/tag/v0.1.2-test> —
> veröffentlichtes Plugin-Zip nachgeprüft (Parser-Fix + Timeouts im
> Bundle), v0.1.1-test als überholt markiert. **Hinweis**: das
> „Verbinde…" der v0.1.1 hätte jede der drei Ursachen sein können — mit
> v0.1.2 zeigt das Panel im Fehlerfall die exakte Ursache an.
>
> **🔧 Client-CI-Fix im Haupt-Repo (2026-07-16, Runde 13)**: Der erste „Client
> Build"-Run auf `dev` (nach `dff9b93`) war rot. Der Repo-Umzug `c36c20b` hatte
> **zweierlei verschluckt**: (1) die Exec-Bits aller `client/scripts/*.sh`
> (Achtung: Repo hat `core.fileMode=false`, chmod allein reicht nicht → `git
> update-index --chmod=+x`) → AppImage „Permission denied"; (2) die vendored
> Windows-Build-Tools `client/scripts/{jom,vswhere}.exe` wegen `*.exe` in der
> Root-`.gitignore` → Windows-Build Exit 9009. Dazu: `cl.exe` nicht im PATH,
> seit der Client im Unterordner liegt. Fixes: `577ce22` (Exec-Bits für 7
> Scripts, `bash scripts/build-appimage.sh`, MSVC via `ilammy/msvc-dev-cmd@v1`,
> Windows-ARM64-Build ersatzlos raus — Roadmap-Ziel ist x64-.exe + Installer) —
> damit **AppImage + macOS grün**; danach jom/vswhere.exe aus Upstream
> re-added (Blob-Hashes verifiziert, `18500c7`) + gitignore-Ausnahmen; dritte
> Regression: WiX-Bundle verlangte den entfernten ARM64-Build → Package-Step
> macOS-only, Output wurde auf Windows eh nie hochgeladen (`7c2fda9`).
> **✅ Client-CI KOMPLETT GRÜN auf `dev@7c2fda9`** (AppImage + Windows x64 +
> macOS, Run 29477549618). **✅ Erstes Client-Release im Haupt-Repo LIVE**
> (Maintainer-Go 2026-07-16): <https://github.com/Elias02345/sonnenschein/releases/tag/v0.0.3-test>
> — `Sonnenschein_Client-*-x86_64.AppImage` + `Sonnenschein-Windows-x64-*.zip`
> + dmg + Debug-Symbols. **✅ Alter Fork `sonnenschein-client` ARCHIVIERT**
> (Bedingung erfüllt: CI grün + Release im Haupt-Repo). Kosmetik-TODO: die
> macOS-dmg heißt noch `Moonlight-<ver>.dmg` (generate-dmg.sh/Target-Name
> nicht rebranded) — bei nächster Client-Runde mitnehmen.
>
> **✅ DECK-TESTS BESTANDEN (Maintainer, 2026-07-16)**: (1) Spiele-Grid mit
> Boxart erscheint im Client (Ansatz a end-to-end inkl. Cover); (2) RD-1
> Remote-Desktop Single-Monitor E2E funktioniert. Beide offenen Punkte aus
> Runde 12 damit verifiziert.
>
> **📋 MAINTAINER-AUFTRAG (2026-07-16, verbindlich — „Client-Modernisierung")**,
> bestätigt + erweitert die Entscheidungen vom 13.07.:
> 1. **Client-Rebrand + moderne UI**: Moonlight-Erbe raus, durchgängig
>    Sonnenschein, moderner Look.
> 2. **Windows installierbare .exe** (echter Installer), die **selbst nach
>    Updates sucht** (GitHub Releases).
> 3. **Auto-Settings**: beste Settings pro Gerät automatisch erkennen +
>    anwenden. **Default = Easy-Modus** (kaum Einstellmöglichkeiten, alles
>    automatisch). **Advanced-Modus** optional, gibt alle Controls zurück.
> 4. **RD-Abfrage**: Verbindet sich ein Nicht-Gaming-Client mit dem
>    Desktop-Modus/der Desktop-App → Abfrage: Remote-Desktop an/aus +
>    Absolut- oder Single-Monitor-Modus.
> 5. **Konstanter Stream in voller Bildwiederholrate zu allen Zeiten**
>    (immer smooth, unabhängig vom Bildinhalt).
> 6. **Decky-Plugin als Killer-Feature**: gräbt sich tief in den Gaming Mode
>    ein; wenn ein verbundener Host das Spiel streamen kann, ist es direkt
>    vom Deck aus streambar (Game-Mode-nativ).
> **Design-Entscheidungen dazu (Maintainer, 2026-07-16)**: Easy-Modus zeigt
> **Qualitätsregler (Auto/Qualität/Flüssigkeit) + Audio-Ausgabe-Wahl**, sonst
> nichts; Windows-Updater = **melden + 1-Klick-Install**; Decky-Plugin
> **nutzt die installierte Client-App** für Pairing/Streaming (Plugin = reine
> Game-Mode-UI + Bibliotheks-Integration).
> Ausgearbeiteter Plan: **ROADMAP „Client-Modernisierung" (M1–M6)** — M1
> Easy/Advanced + Auto-Settings, M2 RD-Abfrage, M3 konstante volle FPS, M4
> Rebrand/moderne UI, M5 Windows-Installer+Updater, M6 Decky-Plugin.
> Implementierung läuft ab 2026-07-16 in dieser Reihenfolge (M1–M3 PC-testbar,
> M6 Deck-gebunden).
>
> **🟡 M1+M2 IMPLEMENTIERT (2026-07-16, `feature/client-easy-mode` @ `9647e34`,
> Maintainer-Sichttest offen)**: Easy/Advanced-Modus (`settingsMode`, Default
> Easy) + Auto-Settings-Engine (`AutoConfig::applyEasyMode`, gecachte Probe,
> Qualitätsregler Ausgewogen/Bildqualität/Flüssigkeit — letzterer cappt bei
> 1080p; Hook in `Session::initialize`) + Easy-Settings-UI (nur Qualität,
> Audio, RD; Advanced blendet alle 7 Gruppen wieder ein) + RD-Abfrage-Dialog
> beim Start der Desktop-App von einem Client mit Desktop-Umgebung
> (`hasDesktopEnvironment`, 3 Optionen + „Auswahl merken" →
> `rememberRdChoice`). **DE-Übersetzung komplett** (qml_de.ts 275/275, inkl.
> 27 vorher fehlender Alt-Strings; via `lupdate6 app.pro` + lrelease).
> Lokal gebaut (CachyOS, qmake6; Vulkan-Header fehlten systemweit → nach
> `~/Dokumente/.localdeps/prefix/include` gelegt; Build-Dir
> `~/Dokumente/sns-client-build`) + `detect-profile`-Smoke ✅ (4K60/AV1 wie
> Runde 10) + GUI-Start ohne QML-Fehler. **Merge auf `dev` erst nach
> Maintainer-Sichttest** (Settings-Seite Easy/Advanced, RD-Dialog, Stream).
>
> **M3-Analyse (2026-07-16)**: Client-Teil durch M1 erledigt (Easy fordert
> native Refresh an). Host-Pacing (Runde 3, pwgrab.cpp ~2058-2132): auf dem
> **SHM-Pfad** wird der letzte Frame bei Damage-Stille re-encodet (voller
> Takt) — auf dem realen Host aktiv (BGRx-SHM-Negotiation, von Maintainer
> als „komplett funktional" bestätigt). **Bekannte Lücke**: auf dem
> **DMA-BUF-Zero-Copy-Pfad** wird bei ausbleibendem Frame nur
> `frame_captured=false` signalisiert → video.cpp raised nichts → Encoder
> pausiert → FPS sinken bei Statik, falls ein Setup dmabuf negotiated.
> Nächster M3-Schritt: Live-Messung (Moonlight-Overlay auf Idle-Desktop);
> nur bei realem Einbruch dmabuf-Frame-Repeat implementieren.
> **Umgebung heute**: Session läuft direkt auf dem CachyOS-Test-Target,
> Repo-Pfad neu **`~/Dokumente/sonnenschein`**, Deck ist verfügbar. Merke:
> Push während laufendem Client-Build-Run cancelt den Run (Concurrency-Gruppe)
> — Doku-Pushes bündeln!
>
> **✅ Game-Launch VERIFIZIERT (2026-07-13)**: Maintainer hat vom Deck ein
> spezifisches Spiel direkt aus Moonlight gestartet — Ansatz (a) läuft end-to-end.
> `dev`→`main` **gemerged** (`main == dev == 1914c33`), Host auf `dev` deployed.
> **✅ Client-Rebrand** (`b1e92ff9`): Fenstertitel „Sonnenschein Client", UI-Strings +
> Windows-Metadaten + CI-Artefaktnamen auf Sonnenschein (interner .exe-Name bleibt
> vorerst Moonlight wg. Wix-Installer). Remote-Desktop + Gaming sind
> cross-platform (auch Windows).
>
> **✅ Repo-Konsolidierung (2026-07-13, `c36c20b`)**: Client-Code lebt jetzt **im
> Haupt-Repo unter `client/`** (kein Moonlight-Fork mehr — der alte
> `sonnenschein-client`-Fork wird archiviert), Submodule sauber unter `client/`,
> GPL-3+Attribution (`client/NOTICE.sonnenschein.md`). Baut + läuft aus `client/`.
> **Client-CI + Releases im Haupt-Repo** aufgesetzt (`.github/workflows/build-client*.yml`
> → AppImage + Windows + macOS, Tag → Release). Host-Lint exkludiert `client/`.
>
> **Nächste Phase (Maintainer-Entscheidungen 2026-07-13, siehe ROADMAP „Client-Track
> Präzisierungen"):** Easy/Advanced-RD-UX (Easy=Auto+Abfrage, Advanced=alle Toggles),
> **Auto-Auflösung/Skalierung/Refresh für ALLE Screens**, moderne UI, **Windows echte
> .exe + Installer**, **Decky-Plugin** (Deck Game Mode). Vieles Deck-/Stream-testgebunden.
> Alter Fork-Repo `sonnenschein-client` → archivieren, sobald Haupt-Repo-Client-CI grün +
> erstes Release dort.
>
> **CI + Test-Apps (2026-07-13) ✅ FERTIG**: Host-Repo grün. Client-Repo war rot
> (Rebrand brach AppImage: `.desktop`-Exec + Upload-Pfad; SteamLink irrelevant) →
> **komplett gefixt** (`ec478cc6`/`6d39511c`/`c798b558`): Exec + Glob-Upload,
> SteamLink-Job raus, **Release-Job** (Tag→GitHub-Release) hinzugefügt. **CI jetzt
> voll grün** (AppImage + Windows + macOS). **Release live** mit downloadbaren
> Test-Apps: <https://github.com/Elias02345/sonnenschein-client/releases/tag/v0.0.2-test>
> — `Sonnenschein_Client-*.AppImage` (Deck, verifiziert lauffähig), `Sonnenschein-Windows-x64-*.zip` (Windows),
> `.dmg` (macOS). Neue Releases per Tag (`git tag vX.Y.Z && git push origin vX.Y.Z`).
> Test-App-Doku: `sonnenschein-client/PACKAGING.sonnenschein.md`.
>
> **Neue Maintainer-Aufgaben (2026-07-13, teils schon in Arbeit — siehe Nachtrag Runde 12):**
> - **Remote-Desktop-Modus (C4)**: Host als Computer nutzen, Client mit **einem
>   oder mehreren Screens** wie nativ (Multi-Monitor, Maus/Tastatur-first, Fenster).
> - **Steam-Deck-Controller nativ**: aktuell emuliert der Host einen **Xbox**-Pad;
>   der Deck-eigene **Steam-Controller** soll nativ genutzt werden (Trackpads/Gyro/
>   Back-Buttons), **Deck-Layouts** einstellbar, und **erkennen welches Spiel offen
>   ist**, obwohl gestreamt wird.
> - Diese Punkte sind teils Deck-gebunden — was ohne Deck geht, mache ich vor;
>   den Rest machen wir zusammen, wenn der Maintainer zurück ist.
>
> ---

**Stand:** 2026-07-13 (Runde 11) — **Host-APIs live verifiziert + Artwork-Fix live auf `main` ausgerollt** (`main == dev == fc793f4`). `/api/library` liefert die echte Steam-Bibliothek (31 Spiele); Artwork gegen neueren Steam-Cache (Content-Hash-Layout + `library_capsule.jpg`) **gefixt** und per `update.sh main` live geschaltet (doctor **15/15**, deployte Binary re-getestet: HL2/Cyberpunk/Elden Ring → Cover). **Update-System dabei gehärtet** (2 Bugs: sudo-Kontext-Re-Exec `f189a19` + root-vergiftetes Build-Dir `fc793f4`; der Auto-Rollback hat sich zwischendurch real bewährt). **Client `sonnenschein-client`** (eigenes GitHub-Repo): Moonlight-Qt-Fork baut + läuft + **pairt** (voller Apollo-Handshake, App-Liste über mutual-TLS) + **Geräteprofil-Auto-Konfiguration** (`detect-profile`, live 4K60/AV1) — Video-Stream offen (2-Geräte-Test), dann Library-Ansicht. Umgebung: `sudo` braucht Passwort (Details Nachtrag Runde 11). Voriger Stand siehe unten. — **Phase 6 (Update-System) fertig**: Auto-Rollback bei Health-Fail + manuelles `revert-update.sh` + Auto-Update-Timer (systemd --user) + Branch-Selector + `GET /api/update-state`. **Scope (fix)**: KDE-only + kein natives Packaging. HDR = KWin-Upstream-Blocker (Code selbstaktivierend). Details Runde 10 darunter.

**Vorherige Stand-Zeile (2026-05-28):** Overhaul-Session: Phase 1.6 Rebrand komplett, Phase-3-Installer-Gerüst + Phase-5-PrimeVue-Fundament gebaut, Code-Review der Laufzeit-Fixes erledigt, erste Vorab-Version nach `main` gepusht.

**Vorherige Stand-Zeile (2026-05-14 spätnacht):** **60-Hz-Fix v3 implementiert mit Cleanup-Hardening, WSL2-Build grün (16/16), CachyOS-Test ausstehend.** Drei Commits oben auf dem v2-Rollback: `b9f431b` (Crash-safe monitor restoration: SIGSEGV/SIGABRT-handler + RAII-Destruktor + Boot-Recovery-Lockfile in `~/.local/state/sonnenschein/disabled-outputs.lock`), `b0f4fd1` (60-Hz-Fix v3: KDE-output-management mit fünf Hardenings — Mode-list/Configuration Destroy-Order, Stream-State-Validation nach apply, log_flush vor jedem Wayland-Call, Timeout 1500ms→800ms, 3rd Roundtrip in init), und der `<this commit>` STATUS.md-Update. **Im schlimmsten Fall werden physische Monitore IMMER wieder eingeschaltet** — entweder im SIGSEGV/SIGABRT-Handler (vor `_Exit`), oder beim nächsten Sonnenschein-Start via `recover_on_boot()` Lockfile-Recovery (auch nach SIGKILL/Force-Shutdown). Force-Shutdown sollte beim Test nicht mehr nötig sein → Logs verfügbar. Test-Schritte siehe §12.A.

---

## TL;DR — Wo stehen wir gerade

**Eine-Zeile-Wahrheit**: Stream + Ton + Eingaben + Headless funktionieren auf CachyOS Plasma 6.6.4 + RTX 3070. Auflösung wird respektiert. **Refresh-Rate hängt immer bei 60 Hz**, egal was der Client (z.B. SteamDeck OLED 90 Hz) anfordert.

### Was Codex am 2026-05-13 verbockt hat — und der Stand jetzt

| # | Commit | Was es war | Ergebnis |
|---|---|---|---|
| 1 | `bf7d939` (10.05) | Maintainer-Stand: Bild+Ton+Eingaben+Headless ✅, 60 Hz ❌ | **letzter wirklich guter Code-Stand** |
| 2 | `6504268` (13.05) | Codex: KScreen-Resolver, Mode-Set NACH `stream_virtual_output` | 60 Hz blieb (KWin akzeptiert Mode-Set nicht nachträglich) |
| 3 | `723537a` (13.05) | Codex: eskaliert auf `kde_output_management_v2` (+850 Zeilen) | KWin Direct Capture brach komplett ab → Moonlight Ping Timeout |
| 4 | `edc144e` (13.05) | Codex: Portal-Fallback bei fehlendem ScreenCast | Mini-Hotfix |
| 5 | `501431a` (13.05) | Codex: revert „stable KWin direct capture path" (-884 Zeilen) | erster Rollback-Versuch |
| 6 | `74c63cf` (13.05) | Codex: revert „pre KScreen resolver path" (-491 Zeilen) | **pwgrab.cpp jetzt byte-identisch mit `bf7d939`** |
| 7 | `41fa9ba` (13.05) | Maintainer: Portal-Fallback statt fatalem `return -1` | letzter Push vor Pause; nur 8 Zeilen über `bf7d939` |
| 8 | **`29cd4b6` (14.05)** | **60-Hz-Fix-Versuch**: `kscreen-doctor add-virtual-output` + `mode-set` zurück VOR Stream-Start | gepusht, WSL2-Build 3/3 grün, CachyOS-Test ausstehend |

`git diff bf7d939 HEAD -- src/platform/linux/pwgrab.cpp` = **nur 28 Zeilen**. Codex' destruktive Experimente sind rausgerollt. Der laufzeit-relevante Code ist heute effektiv `bf7d939` + Portal-Fallback (`41fa9ba`).

### Der 60-Hz-Fix-Versuch (uncommitted, in Arbeit am 2026-05-14)

**Root Cause (verstanden)**: `zkde_screencast_unstable_v1::stream_virtual_output` hat **keinen Refresh-Rate-Parameter** (siehe `third-party/plasma-wayland-protocols/src/protocols/zkde-screencast-unstable-v1.xml`). KWin erstellt den Virtual Output beim Stream-Start mit fixem 60-Hz-Default, dessen Mode-Liste *nur* 60 Hz enthält. Nachträgliche `kscreen-doctor output.NAME.mode.WxH@HZ`-Aufrufe werden ignoriert, weil der Modus nicht in der Mode-Liste existiert.

**Fix-Strategie**: Output **vorher** via `kscreen-doctor add-virtual-output NAME W H` erstellen, dann `mode WxH@HZ` setzen, dann erst Stream öffnen. `pwgrab.cpp::find_target()` findet den existierenden Output → nutzt `stream_output` (mit `wl_output*`) statt `stream_virtual_output`. Refresh-Rate kommt durch die Mode-Liste, nicht durch Argument.

Codex hat das Pattern abgelehnt (vermutlich weil es `e453afa`/`d84072e` widerspricht, die `kscreen-doctor add-virtual-output` als „doppelt-gemoppelt" entfernt haben). Aber: damals war pwgrab.cpp noch nicht in der `find_target → stream_output` / `stream_virtual_output`-Hybrid-Form. Heute (HEAD pwgrab.cpp Z. 209-228) wird der Fall korrekt unterschieden.

**Implementierung im 60-Hz-Fix-Commit**:
- `src/platform/linux/virtual_display/backends/kwin_wayland.cpp::create()` bringt `kscreen-doctor add-virtual-output` + `mode-set` + `hdr-enable` zurück.
- Neues Feld `ActiveDisplay::added_via_kscreen` trackt, ob wir den Output erstellt haben → `destroy()` ruft nur dann `remove-virtual-output`.
- Bei `add-virtual-output`-Failure (Plasma < 6.4, kscreen-doctor unreachable): **kein Abort** — wir fallen durch und `pwgrab.cpp` nutzt seinen `stream_virtual_output`-Fallback (60 Hz, aber funktionierender Stream).

**Erwartung**: Stream startet wie bisher (Bild+Ton+Eingaben+Headless), aber jetzt mit der vom Client angeforderten Refresh-Rate. SteamDeck-OLED 90 Hz wird durchgereicht.

### Was beim letzten CachyOS-Stand erreicht ist

- ✅ KWin Direct Capture (`zkde_screencast`) statt Portal-Dialog
- ✅ Virtual Display wird transparent erstellt, kein KDE-Auswahldialog
- ✅ Physische Monitore werden beim Stream deaktiviert, beim Disconnect wieder eingeschaltet
- ✅ Bild läuft, Ton läuft, Eingaben gehen durch
- ✅ Client-Auflösung wird respektiert (1280x800 vom SteamDeck arrived als 1280x800)
- 🟡 **Refresh-Rate steckt bei 60 Hz** (Fix-Versuch oben)
- 🟡 HDR theoretisch im Encoder-Metadata, aber nicht KWin-aktiviert

### Hauptanwendungsfall (Maintainer)

Physische Monitore am CachyOS-PC. Beim Streaming → alle physischen Outputs aus → Virtual Display als einziger aktiver Output → PipeWire/KWin captured ihn → Moonlight zeigt das Bild auf dem Client (z.B. SteamDeck oder TV via Moonlight für Android). Beim Disconnect → physische Outputs wieder an.

---

## Session 2026-07-11 — Produktionsreife: Installer-Umbau + Steam-Deck-Fixes

> Ausgeführt auf der Windows-Dev-Maschine (WSL2-Build-Verifikation, kein
> GPU/Compositor). Maintainer-Feedback zu Sessionbeginn: „es gibt noch keinen
> richtigen Installer und ich bekomme nur Fehler, virtuelle Displays mit
> falscher Auflösung und falschen Eigenschaften, System mit Paketen
> zugemüllt, Bild im Stream ist gedreht." Maintainer-Entscheidungen:
> **kein Container** (nativer Build, aber restlos entfernbar), **direkt auf
> main pushen** sobald WSL2-Build + Installer-Checks grün.

### Root-Cause-Analyse „nichts funktioniert"

1. **Installer setzte `cap_sys_admin` aufs Binary** → xdg-desktop-portal
   verweigert privilegierte Caller → PipeWire-Capture (der EINZIGE Weg,
   Virtual Displays zu capturen, §9.13) tot. Der Installer hat also genau
   das Kern-Feature deaktiviert. → §9.22
2. **Installer installierte komplette Build-Toolchain systemweit** nach
   `/usr/local` ohne Aufzeichnung → „System zugemüllt", Uninstaller
   entfernte nur einen Bruchteil.
3. **Steam Deck OLED Panel ist nativ 800x1280 Portrait** → Client kann
   Portrait-Mode anfordern → Virtual Display wird hochkant angelegt →
   Bild im Stream 90° gedreht. → §9.23
4. **Portal-Restore-Token nur im RAM** → bei jedem Start Auswahldialog →
   Fehlauswahl „Virtueller Bildschirm" (XDG-VIRTUAL) erzwingt 1920x1080
   (§9.13) → „falsche Auflösung und Eigenschaften".

### Was gebaut + verifiziert wurde

**Installer-Komplettumbau ✅ (shellcheck grün)**
- Default-Prefix **`/opt/sonnenschein`** — alles in einem Verzeichnis.
  PATH-Symlinks `/usr/local/bin/{sonnenschein,sunshine}`.
- **CMake-`install_manifest.txt` wird nach `${PREFIX}/install_manifest.txt`
  kopiert** → `uninstall.sh` entfernt exakt jede installierte Datei.
- **Paket-Snapshot vor/nach Installation** (`pacman -Qq`/`dpkg-query`/`rpm -qa`
  diff) → `${PREFIX}/installed-packages.txt` → Uninstaller bietet an, genau
  die Pakete zu entfernen, die der Installer neu installiert hat.
- **Kein `setcap` mehr per Default** (§9.22). Opt-in via `--kms` nur für
  reines Physisch-Monitor-Mirroring; Installer/Updater entfernen stale
  Capabilities aktiv.
- **`doctor.sh` neu**: 9 Check-Gruppen ([OK]/[FAIL]): Binary, File-Caps
  (Konflikt-Detektor!), Wayland-Session, kscreen-doctor, Plasma ≥ 6.6,
  PipeWire, Portal, /dev/uinput, NVIDIA ≥ 580, Service, WebUI-Health.
  `--repair` entfernt Caps, lädt uinput, reinstalliert udev-Rule,
  restarted Service.
- `install-state.env` (PREFIX, SRC_DIR, LINGER_ENABLED, KMS_CAP, User,
  Datum) + Installer-Kopie unter `${PREFIX}/installer/` → Uninstall/Update/
  Doctor funktionieren auch ohne Source-Checkout.
- Bootstrap (`curl | bash`) klont jetzt **persistent** nach
  `~/.local/share/sonnenschein/src` (statt mktemp) → Updates möglich.
  Kein Root-Run erlaubt (Guard), non-TTY → automatisch `--yes`.
- Linger wird nur enabled wenn vorher aus, und beim Uninstall nur dann
  revertiert (Tracking via LINGER_ENABLED).

**Portrait-Rotation-Fix ✅ (Steam Deck „Bild gedreht")**
- `nvhttp.cpp::make_launch_session`: Nach Mode-Parsing — wenn
  `height > width` → swap + Log „normalizing to landscape". → §9.23
- `rtsp.cpp`: gleiche Normalisierung für `config.monitor` (RTSP-Viewport).
- Bewusst OHNE Config-Toggle (Game-Streaming ist immer Landscape;
  Portrait-Request kommt nur von Handheld-Panels).

**Portal-Restore-Token-Persistenz ✅ (§9.15 gelöst)**
- `pwgrab.cpp`: Token wird nach `Start` atomar (write+rename) nach
  `~/.local/state/sonnenschein/portal-restore-token` geschrieben, in
  `portal_session_t::init()` geladen, bei SelectSources-Fehler mit Token
  einmal ohne Token retried + Datei gelöscht.
- Effekt: Der KDE-Auswahldialog erscheint nur noch EINMAL (erste Portal-
  Nutzung); danach dialog-frei → keine Fehlauswahl-Gefahr mehr.

**package-lock.json committed ✅ (§9.24)**
- War in `.gitignore` (!) — `npm ci` unmöglich, Flatpak-Manifest
  referenzierte es ins Leere. Jetzt erzeugt + committed, .gitignore-Zeile
  entfernt. `npm run build` grün (inkl. Diagnostics-Bundle).

### Nachtrag: CachyOS-Test des Installers (2026-07-11, Maintainer) ✅ + Runde 2

**Installer-Run erfolgreich**: `curl | bash` lief komplett durch (Klon →
32 Pakete, davon nur 12 neu → Build → /opt-Install → Manifest → udev →
User-Service). `doctor.sh --repair` : 12/14 grün auf Anhieb. Plasma 6.7,
NVIDIA 610.43.03, keine File-Caps. Gefundene Rest-Probleme → Runde 2:

1. **Service lief nach Install nicht** (Installer enabled nur, startete
   nicht; doctor-Repair startete ihn). → Fix: `install.sh` startet den
   Service jetzt direkt nach der Installation.
2. **WebUI-Check falsch-negativ**: Service-Unit hat `ExecStartPre=sleep 5`,
   doctor prüfte sofort. → Fix: Retry-Loop (bis ~15 s).
3. **WebUI zeigt Fatal-Banner „sudo setcap cap_sys_admin+p …"**: Apollo-Erbe
   in `kmsgrab.cpp` — auf Wayland wurde ein fehlgeschlagener KMS-Probe als
   `fatal` geloggt, obwohl PipeWire unser Primärpfad ist und setcap ihn
   sogar zerstören würde (§9.22). → Fix: nur noch fatal wenn
   `capture = kms` explizit konfiguriert; Meldung erklärt jetzt den
   PipeWire-Sachverhalt statt zur setcap-Falle zu raten.
4. **Steam Deck findet den Host nicht (Discovery)**: mDNS-Publish braucht
   laufenden avahi-daemon (wird per dlopen genutzt) + offene Ports.
   → Fix: neue `installer/lib/firewall.sh` — öffnet bei aktivem **ufw**
   oder **firewalld** die Moonlight-Ports (TCP 47984,47989,47990,48010;
   UDP 47998-48000,5353/mDNS), enabled avahi-daemon (`ensure_avahi`).
   Beides wird in `install-state.env` protokolliert (FIREWALL_CONFIGURED,
   FIREWALL_MDNS_ADDED, AVAHI_ENABLED) und beim Uninstall exakt revertiert.
   avahi-Runtime-Paket in alle vier Distro-Listen aufgenommen.
5. **Updates selbstheilend**: `update.sh` läuft am Ende automatisch
   `doctor.sh --repair` (Caps, avahi, Firewall, Service, WebUI).
6. doctor prüft jetzt zusätzlich avahi-daemon + Firewall-Ports (mit
   `--repair`-Pfad für beide).

**Bewusst verschoben** (nächste Session / Phase 5): WebUI-Vereinfachung +
intuitiveres Pairing-UI (PrimeVue-Migration der Bestands-Seiten). Die
funktionalen Pairing-Blocker (Discovery, Firewall, Fatal-Banner) sind mit
Runde 2 adressiert.

### Nachtrag Runde 3 (2026-07-11): Stream-Feintuning nach Maintainer-Report

**Maintainer-Report vom laufenden Stream**: „1280x800 71fps [schwankt
zwischen 13 und 71] (Codec: HEVC 10-bit SDR)" — FPS schwankt je nach
Bildinhalt/Mausbewegung.

1. **FPS-Schwankung 13–71 → Frame-Pacing-Fix (§9.25)**: KWins Screencast
   ist damage-getrieben (Frames nur bei Bildänderung). Der Capture-Loop in
   `pwgrab.cpp` wartete bei ausbleibendem Frame bis zu **500 ms** und
   pushte dann ein Leer-Frame → Encoder-FPS bricht bei statischem Bild ein.
   Fix: Wartefenster = 1 Frame-Intervall; bei Timeout wird der letzte
   SHM-Frame erneut encodet (`have_prev_frame`-Pfad). Erwartung: konstante
   ~90 fps, unabhängig vom Bildinhalt. Dass Moonlight max. 71 statt 90
   zeigte, war dieselbe Ursache (Damage-Rate des Desktops).
2. **HDR-Analyse**: „HEVC **10-bit** SDR" beweist: serverinfo bewirbt
   Main10 korrekt, Encoder-Pfad ist 10-bit-fähig. Das Deck fordert aber
   `dynamicRangeMode=0` an → Client-seitig HDR nicht aktiv. Nötig auf dem
   Deck: (a) SteamOS Gaming Mode → Anzeige → HDR AN, (b) Moonlight →
   Einstellungen → Grundlagen → „HDR (experimentell)" AN (Moonlight-Flatpak
   kann HDR im Gaming Mode seit v5). DANN fordert der Client HDR an — aber
   host-seitig fehlt noch der echte HDR-Pfad (Phase 4): KWin-Virtual-Output
   auf HDR schalten + 10-bit-PipeWire-Negotiation (aktuell BGRx 8-bit) +
   PQ/BT.2020-Signalisierung. Ohne das wäre HDR nur „getunnelt", Farben
   falsch. **Phase 4 ist damit der nächste Laufzeit-Meilenstein.**
3. **Steam-Controller/Deck-Eingaben (Touchpads, Gyro, Rücktasten)**:
   Host-seitig bereits vollständig — `gamepad = auto` (Default) emuliert
   via inputtino ein **DualSense (DS5)** sobald der Client Motion/Touch
   meldet (`motion_as_ds4`/`touchpad_as_ds4` Default true, uhid via
   installer geladen). Client-seitig: Moonlight-Einstellungen auf dem Deck
   → Eingabe → Motion-Sensoren/Touch aktivieren; Rücktasten (L4/L5/R4/R5)
   sind Steam-Input-Sache und werden im Deck-Layout frei gemappt. Kein
   Code nötig, nur Doku (→ Phase 5/7 Doku-Task).

### 9.25 (Referenz) FPS-Schwankung im Virtual-Display-Stream — GELÖST siehe Nachtrag Runde 3

### Nachtrag Runde 4 (2026-07-11): 90 Hz BESTÄTIGT ✅ + HDR Stufe 1 implementiert

**Maintainer**: „der 90hz fix ist komplett funktional laut moonlight. immernoch kein hdr."

**HDR-Root-Cause gefunden**: `video.cpp` wählt den HDR-Farbraum (BT.2020/PQ)
nur wenn `config.dynamicRange > 0` **UND** `display->is_hdr()` — und
`pw_display_t` hatte keinen `is_hdr()`-Override → immer `false` → „HEVC
10-bit **SDR**" exakt wie beobachtet, selbst wenn der Client HDR anfordert.

**HDR Stufe 1 (implementiert, CachyOS-Test offen)**:
1. `kwin_session_t::apply_hdr_settings()`: schaltet den virtuellen Output
   via `kde_output_configuration_v2::set_high_dynamic_range` (+ WCG wenn
   Capability 0x10, + `set_sdr_brightness(203)` BT.2408-Referenz) in den
   HDR-Modus. Capability-guarded (0x8), non-fatal, Protokoll v4+ (haben v19+).
2. `start(..., want_hdr)` ← `config.dynamicRange > 0`; nach Mode-Apply wird
   HDR angewendet, Ergebnis in `hdr_active`. Der bestehende v3-3b
   failed-Check danach fängt einen evtl. Stream-Close durch den HDR-Switch.
3. `pw_display_t::is_hdr()` → `kwin_direct->hdr_active`;
   `get_hdr_metadata()` liefert synthetisierte HDR10-Defaults (BT.2020-
   Primaries, D65, 1000 nits Peak) — virtuelle Outputs haben kein EDID.

**Diagnose-Zeilen für den Test** (`journalctl --user -u sonnenschein`):
- `Client dynamicRange: 1, Display is HDR: true` → volle Kette aktiv
- `Client dynamicRange: 0, ...` → Deck fordert kein HDR an (Moonlight-
  HDR-Toggle + SteamOS-HDR prüfen)
- `output ... does not advertise HDR capability (caps=0x...)` → KWins
  virtueller Output kann (noch) kein HDR → Stufe-2-Entscheidung nötig

**Bekannte offene Frage (Stufe 2)**: Capture negotiated aktuell 8-bit BGRx.
Wenn KWin den HDR-Output für 8-bit-Streams nach sRGB tone-mapped, sind die
Farben im HDR-Stream ggf. flau → dann 10-bit-PipeWire-Negotiation
(SPA ABGR_210LE) + `sws`-Source-Format-Plumbing (video.cpp:243 hat BGR0
hartkodiert) als nächster Schritt. Maintainer-Sichttest entscheidet.

### Nachtrag Runde 5 (2026-07-11): HDR-Test-Ergebnis — UPSTREAM-BLOCKER identifiziert

**Maintainer-Logs zeigen die komplette Diagnose:**
1. ✅ `Client dynamicRange: 1` — das Deck fordert HDR korrekt an (Toggles
   richtig gesetzt), unsere Request-Kette funktioniert Ende-zu-Ende.
2. 🔴 `output 'Virtual-pipewire-virtual' does not advertise HDR capability
   (caps=0x2000)` — **0x2000 = ausschließlich `custom_modes`** (deshalb
   funktionieren die 90 Hz!). Kein `high_dynamic_range` (0x8), kein
   `wide_color_gamut` (0x10).

**Bedeutung**: KWins per `stream_virtual_output` erzeugte virtuelle Outputs
unterstützen in **Plasma 6.7 kein HDR** — Upstream-Limitierung, kein
Sonnenschein-Bug. Genau wie Custom-Modes erst durch KWin MR !8766 kamen,
braucht HDR-auf-Virtual-Outputs einen KWin-Patch (vermutlich
`VirtualOutput::updateCapabilities` + Colorimetry im Screencast-Pfad).
Interessanter Nebenfund: KWin registriert den Screencast-Virtual-Output
in output-management als **`Virtual-pipewire-virtual`**.

**Unser Code ist zukunftssicher**: Capability-guarded — sobald ein
Plasma-Update das Bit 0x8 setzt, schaltet Sonnenschein HDR automatisch an,
ohne dass wir etwas ändern müssen.

**Nächste Schritte HDR**:
- [ ] Maintainer: Feature-Request bei KDE einreichen (Text von Claude
  vorbereitet, siehe Session-Chat 2026-07-11 — Kurzfassung: „Add
  high_dynamic_range/wide_color_gamut capability to screencast virtual
  outputs, analog zu custom_modes aus MR !8766")
- [ ] Bei jedem Plasma-Update: Stream mit HDR-Request testen — Log-Zeile
  „HDR+WCG enabled" erscheint automatisch sobald KWin es kann
- [ ] Option für später evaluieren: eigener KWin-MR (C++/KWin-Kenntnisse
  nötig, `src/backends/virtual/virtual_output.cpp` + Screencast-Colorimetry)
- ⏸ Stufe 2 (10-bit-Capture) pausiert bis Output-HDR upstream existiert —
  ohne HDR-Output gibt es keinen HDR-Content zu capturen.

**HDR ist damit sauber geparkt. Nächster Fokus laut Plan: Phase 5 (WebUI).**
(Maintainer hat den KDE-Feature-Request eingereicht ✅.)

### Nachtrag Runde 6 (2026-07-11): Phase 5 WebUI — Setup-Wizard + PIN-first Pairing

**Umgesetzt + im Browser gegen das gebaute Bundle verifiziert** (Formular-
Validierung interaktiv getestet, DE-Rendering bestätigt):
1. **`welcome.html` → PrimeVue-Setup-Wizard** (neue `SetupWizard.vue`):
   Zwei Schritte mit Step-Indikator — (1) Zugangsdaten (Default-User jetzt
   `sonnenschein` statt `apollo`, Live-Passwort-Match-Validierung,
   Password-Strength-Feedback), (2) Erfolg + Pairing-Anleitung in drei
   Schritten mit „Pairing-Seite öffnen"-Button.
2. **`pin.html`: PIN-Pairing ist jetzt Default-Tab** (war OTP — Moonlight-
   Standardflow ist PIN!), große zentrierte numerische PIN-Eingabe
   (inputmode=numeric, 2rem, letter-spacing), Inline-Erfolgsmeldung statt
   blockierendem `alert()`. Permission-Editor unangetastet (funktioniert).
3. **`locale.js`-Robustheit**: Wenn `/api/configLocale` fehlschlägt, blieb
   die GESAMTE WebUI weiß (unhandled rejection vor `app.mount`). Jetzt
   Fallback auf Browser-Sprache. (Beim Static-Preview-Test gefunden.)
4. Neue `welcome.*`-i18n-Keys in DE + EN (Rest fällt auf EN zurück).

**Bewusst auf nächste Runde verschoben**: `index.html`-Dashboard + Navbar-
Shell-Migration (Blind-Rewrite von ClientCard/ResourceCard/Versions-Logik
ohne Live-Backend-Test wäre gegen die Projektregeln). Task #17 im Tracker.
→ **In Runde 7 doch erledigt** — Live-Backend-Test wurde möglich, siehe unten.

### Nachtrag Runde 7 (2026-07-11): Dashboard live-getestet, WoL, CI hart, Ubuntu-E2E

**Durchbruch fürs WebUI-Testen**: Das sonnenschein-Binary läuft in WSL2
(Software-x264-Encoder, kein Display nötig) und serviert die echte WebUI
auf 47990. Test-Setup: Web-Bundle nach `/usr/local/assets/web` stagen +
lokaler HTTP→HTTPS-Proxy (Scratchpad) mit Session-Cookie-Injection für
den Browser-Pane. Damit sind ab jetzt ALLE WebUI-Seiten live testbar.

1. **Dashboard (`index.html` → `Dashboard.vue`) ✅ LIVE-GETESTET**:
   PrimeVue-Topbar (Links zu allen Seiten + Dark-Toggle), Fatal-Banner
   (echte Fatals aus /api/logs gerendert), Versions-/Update-Karte
   (GitHub-Check), Live-„Gekoppelte Geräte"-Karte (/api/clients/list),
   Moonlight-Client-Links. Wizard-E2E gegen echtes Backend: Passwort
   real gesetzt → Session-Login → Dashboard rendert alle Datenquellen,
   /pin defaultet live auf PIN-Tab. Null Console-Errors.
2. **Wake-on-LAN (Boot-to-Ready Stufe 2) ✅**: `installer/lib/wol.sh` —
   Default-Route-NIC-Detection (nur wired), `ethtool -s <nic> wol g`,
   Persistenz via oneshot-Unit `sonnenschein-wol.service`, MAC in der
   Summary, Tracking in install-state.env (WOL_CONFIGURED), Uninstall
   entfernt die Unit (lässt NIC-Setting bewusst in Ruhe). doctor prüft
   „Wake-on: g" + repariert. `ethtool` in allen vier Paketlisten.
3. **CI hart geschaltet (Phase 7) ✅**: Alle drei Matrix-Jobs (Arch,
   Ubuntu 24.04, Fedora 41) sind real grün (per gh API verifiziert,
   null failed Steps) → `continue-on-error` entfernt.
4. **Ubuntu-24.04-E2E-Test** läuft via Subagent in WSL (frischer Klon
   von main, kompletter Installer-Durchlauf inkl. doctor + uninstall).
   Bekannte Erwartung: libva-2.20-Problem (§9.1) — Ergebnis folgt.
5. **libva-Backport automatisiert ✅**: Neue `installer/lib/libva.sh` —
   Debian-Familie mit libva < 2.22 bekommt libva 2.22 automatisch from
   source (meson, ~2 min), sonst Linker-Fehler gegen das prebuilt FFmpeg
   (§9.1). Präventiv vor dem E2E-Ergebnis implementiert, shellcheck grün.
6. **Crash-Reporter v1 ✅**: Fatal-Banner im Dashboard hat jetzt „Auf
   GitHub melden" — öffnet pre-filled `crash.yml`-Issue mit Version +
   sichtbaren Fatal-Zeilen. Keine Telemetrie, Nutzer sieht exakt was
   gesendet wird (ROADMAP-Konzept, Frontend-only-v1).
7. **Login-Seite auf PrimeVue ✅ LIVE-GETESTET**: `LoginCard.vue` im
   Wizard-Look, Passwort-merken erhalten, i18n-Fehlermeldungen. Live
   verifiziert: falsches Passwort → 401-Meldung, korrektes → Session-
   Cookie → landet auf dem neuen Dashboard.

**Phase-5-Reststand**: Noch Bootstrap: `apps.html`, `config.html`,
`password.html`, `troubleshooting.html` (funktionieren unverändert;
Migration mit dem etablierten Live-Test-Setup in nächster Runde).
Live-Log-Tab + WebUI-Update-Knopf (Phase 6) ebenfalls offen.
→ `password.html` in Runde 8 erledigt, siehe unten.

### Nachtrag Runde 8 (2026-07-12): Ubuntu-E2E bestanden, npm-Konflikt-Fix, Passwort-Seite

1. **Ubuntu-24.04-E2E-Test ✅ KOMPLETT BESTANDEN** (WSL, frischer Klon
   `/root/sns-e2e`, PREFIX=/opt/sonnenschein):
   - **Install**: EXIT=0, Binaries in `/opt/sonnenschein/bin`, Symlink
     `/usr/local/bin/sonnenschein`, install-state.env korrekt. Der
     libva-2.22-Backport (`lib/libva.sh`) griff wie designed.
   - **doctor.sh**: Strukturell sauber — 5 umgebungsunabhängige Checks
     grün (Binary, keine File-Caps, avahi, uinput, WAYLAND_DISPLAY);
     7 FAILs sind alle WSL-headless-bedingt (kein Wayland-Session-Type,
     kein KDE, kein PipeWire/Portal, Service nicht gestartet) — auf
     echtem Desktop-Ubuntu erwartbar grün.
   - **uninstall.sh** (`SONNENSCHEIN_ASSUME_YES=1`): **restlos** —
     /opt/sonnenschein, Symlink, Source-Checkout, ~/.config, ~/.local/
     share, systemd-Units alle weg; installer-added Pakete entfernt.
2. **npm/NodeSource-Konflikt-Fix ✅** (einziger E2E-Befund): Auf Systemen
   mit NodeSource-nodejs bricht `apt-get install npm` mit „held broken
   packages". Fix in `installer/lib/packages.sh`: wenn `node` + `npm`
   schon existieren, werden die Distro-Pakete nodejs/npm aus der Liste
   gefiltert (generisch für alle vier Familien). shellcheck grün.
3. **Passwort-Seite auf PrimeVue ✅** (`password.html` →
   `PasswordCard.vue`): Card-Look wie Login/Wizard, Live-Mismatch-
   Validierung (Feld invalid + Meldung + Save-Button disabled, reused
   `welcome.password_mismatch`), toggle-mask-Passwortfelder, i18n-Keys
   `password.back` (DE+EN) mit Link zurück zum Dashboard, Success-
   Message + Auto-Reload wie zuvor. Statisch getestet (Vite-Build +
   Browser: DE-Rendering komplett, Mismatch-Logik in beide Richtungen
   verifiziert). API unverändert (`POST ./api/password`).

4. **Troubleshooting-Seite auf PrimeVue ✅** (`troubleshooting.html` →
   `Troubleshooting.vue`): Action-Cards (App schließen erzwingen /
   Neustart / Beenden mit Confirm), Log-Viewer mit Filter, Copy-Button
   und 5s-Auto-Refresh — das deckt gleichzeitig den geplanten
   „Live-Log-Tab" ab. Windows-only DD-Reset-Card entfernt (war auf
   Linux nie sichtbar). Fehlende DE-Übersetzungen
   `troubleshooting.quit_apollo*` ergänzt. Statisch getestet.
5. **Topbar als Shared Component ✅**: Dashboard-Topbar (Nav + Theme-
   Toggle) nach `Topbar.vue` extrahiert, von `Dashboard.vue` und
   `Troubleshooting.vue` genutzt. Dashboard nach Refactor im Browser
   gegengeprüft (Topbar, 6 Nav-Links, 3 Cards rendern).

**Phase-5-Reststand nach Runde 8**: `apps.html` (953 Zeilen, Modal-
Editor) und `config.html` (546 Zeilen + 17 Tab-Komponenten in
`configs/tabs/`) sind bewusst noch Bootstrap — jede der beiden
Migrationen ist eine eigene Session mit Live-Backend-Test (Blind-
Rewrite = verbotener halbgarer Code). Beide funktionieren unverändert
mit der alten Navbar. WebUI-Update-Knopf (Phase 6) braucht einen neuen
Backend-Endpoint (POST /api/update → spawnt `installer/update.sh`
detached; SRC_DIR via `$PREFIX/install-state.env` relativ zum Binary
auflösen) + echten Update-E2E-Test — eigene Session.
→ Beides in Runde 9 erledigt, siehe unten.

### Nachtrag Runde 13 (2026-07-16): CI-Fixes, §12-Neufassung, Deck-Controller-Recherche

**Session auf dem CachyOS-Target** (Details im HIER-WEITERMACHEN-Block oben):
Client-CI-Fixes (Exec-Bits/MSVC `577ce22`, jom+vswhere.exe re-added), AGENTS.md
committed (`a4746f0`), §12 komplett neu geschrieben (alte erledigte Einträge
raus, Widerspruch Phase 1.6 aufgelöst — CMake-Rebrand war real längst
erledigt), §6-Phase-1.6-Zeile korrigiert. Host läuft aktiv auf `dev@1914c33`
(funktional aktuell inkl. Spiele-als-Apps — Commits seither nur CI/Doku).

**Deck-Controller-Recherche (Web + Repo, 2026-07-16) — Kernbefunde:**
- **Deck-Controller „Neptune" = USB `28de:1205`**; Steam erkennt ihn rein über
  VID/PID via hidraw (SDL `SDL_hidapi_steamdeck.c`, Valve-udev-Regeln matchen
  pauschal VID `28de`). Desktop-Steam erkennt ein solches Gerät auch auf
  Nicht-SteamOS nativ (steam-for-linux #11215) — kein Hardware-Bindungs-Check.
- **uhid-Emulation ist prinzipiell machbar**: sc-ble-bridge emuliert erfolgreich
  einen Steam Controller via /dev/uhid; inputtino hat mit dem DS5-uhid-Backend
  (`src/uhid/joypad_ps5.cpp`) die passende Vorlage. Upstream-inputtino hat
  keinen Deck-Typ (auch keine PRs). Protokoll gut dokumentiert (64-Byte-Reports
  inkl. L4/L5/R4/R5, 2 Trackpads, Gyro; Feature-Reports für Lizard-Mode etc.).
- **Valve selbst** tunnelt bei Remote Play auf Steam-Input-Ebene (Host sieht
  „Steam Virtual Gamepad" `28de:11ff`) — Rear-Buttons/Gyro dort notorisch
  kaputt. Hier könnte Sonnenschein besser sein.
- **ABER — eigentlicher Blocker ist client-seitig**: Im Deck Game Mode claimt
  Steam den Controller und reicht Moonlight nur das virtuelle Pad weiter —
  Paddles/Trackpad-Rohdaten kommen im Client gar nicht an (moonlight-qt #936,
  #1123). Moonlight-Protokoll hat zwar `PADDLE1–4_FLAG`s, aber host-seitig
  konsumiert sie kein Backend, und es kennt nur EIN Touchpad → für 2 Trackpads
  bräuchte es eine eigene Protokollerweiterung (wir kontrollieren beide Enden).
- **Einschätzung: möglich mit großem Aufwand.** Empfohlener Pfad: (1) Host-PoC
  standalone (uhid-Testtool `28de:1205`, Report-Descriptor vom echten Deck
  dumpen, Erfolgskriterium: Desktop-Steam zeigt „Steam Deck Controller"),
  (2) Steam↔Controller-Traffic am echten Deck mit hid-recorder mitschneiden,
  (3) inputtino-Fork `SteamDeckJoypad` nach DS5-uhid-Vorbild (+ Upstream-PR),
  (4) Host: `gamepad = steamdeck` + PADDLE-Flags → L4/L5/R4/R5,
  (5) Client: Raw-Capture der Deck-Extras + Protokollerweiterung — ohne
  Schritt 5 bringt Host-Emulation gegenüber DS5 fast nichts.
- **Nur am Gerät klärbar**: unbekannte Feature-Report-Abfragen (Serial/
  Firmware), Firmware-Update-Prompt-Risiko, hid-steam-Treiber-Verhalten am
  uhid-Gerät, ob Game-Mode-Raw-Zugriff überhaupt möglich ist (Steam claimt
  hidraw exklusiv), ob Desktop-Steam externe Deck-Controller voll bedient.

**Decky-Plugin-Recherche (Web, 2026-07-16) — Kernbefunde für M6:**
- **Stack**: `decky-plugin-template` (React/TS-Frontend via `@decky/ui` +
  `@decky/api`, Rollup, **pnpm v9 zwingend**; optional Python-Backend).
  Deploy: SSH nach `/home/deck/homebrew/plugins/` + `systemctl restart
  plugin_loader`; Decky-Dev-Mode für Sideload + CEF-Debugging (Port 8081).
- **Maßgebliches Vorbild: MoonDeck** (GPL-3 → kompatibel, aktiv maintained
  v1.11.3): legt Non-Steam-Shortcuts auf einen **Shell-Wrapper** (nötig für
  Steam-Fokus), kodiert die App-Identität als Env-Var in den Launch-Options
  (`X=Y %command%`), startet Moonlight als Subprozess (Flatpak/AppImage, CLI
  `stream <host> <app>` + `--resolution/--bitrate/--hdr`), Spiele in eigener
  Steam-Collection. Braucht „MoonDeck Buddy" auf dem Host — **unsere
  `/api/library` ersetzt das nativ inkl. Artwork.**
- **Game-Mode-Start**: `SteamClient.Apps.RunGame(gameId, ...)` auf den
  Shortcut → Steam/gamescope managt die Session (Overlay + Controller-Config
  funktionieren). Library-Injection via `AddShortcut`/`SetShortcutName`/
  `SetAppLaunchOptions`/`SetCustomArtworkForApp` (produktiv bewiesen durch
  SteamGridDB-Plugin) — APIs sind Valve-intern/undokumentiert (Drift real,
  z. B. AddShortcut setzt Namen nicht mehr). Bekannter Bug: viele Add/Remove-
  Zyklen korruptieren den Steam-Client → diff-basiert syncen + Restart-Helper.
- **Architektur (beschlossen: Plugin nutzt unsere Client-App)**: Python-
  Backend = REST-Client für `/api/library` + Runner-Wrapper (`sonnenschein-
  run.sh` + `runner.py` startet unsere Client-App); TS-Frontend = Quick-
  Access-Panel (Hosts, Spiele, Sync-Button) + Shortcut-Sync + Collection.
  Unser Client braucht dafür stabile Stream-CLI + deterministische Exit-Codes.
- **Store-Anforderung**: kein Remote-Code-Nachladen — Client-App muss separat
  installiert sein (Plugin verweist auf Installation). Risiken: SteamClient-
  API-Drift, SteamOS-Updates entfernen Decky gelegentlich.

### Nachtrag Runde 12 (2026-07-13): Ansatz-a Spiele-als-Apps + Pläne Remote-Desktop & Deck-Controller

**Einheitliche Liste (Ansatz a)** — siehe TL;DR-Client-Abschnitt oben + Commit
`a9bbaef` (host-seitig, auf `dev`, Listen-Teil verifiziert, Game-Launch offen).

**Maintainer-Aufgaben 2026-07-13 (recherchiert, Pläne für die Zusammenarbeit):**

**1. Remote-Desktop-Modus (C4)** — *„Host als Computer, Client mit 1+ Screens wie nativ".*
- **Solo erledigt (`19733c20`, Client-Repo)**: Remote-Desktop-**Profil** im Client (Settings-Toggle `remoteDesktopMode`) — Fenster + absolute Maus, kompiliert + Pref persistiert. Verhalten braucht Live-Desktop-Stream zur Bestätigung.
  Die nötigen Settings existieren schon in `StreamingPreferences`
  (`windowMode` = `WM_WINDOWED`, `absoluteMouseMode`, `captureSysKeysMode`,
  `swapMouseButtons`). Profil = Fenster + absolute Maus + Maus/Tastatur-first,
  streamt die Desktop-App. Zweiter Launcher-Eintrag „Remote Desktop" vs „Gaming"
  (Roadmap C1). Auf dem PC testbar.
- **Groß / später zusammen**: **Multi-Monitor** („mehrere Screens wie nativ").
  Moonlight streamt genau *einen* Host-Display. Mehrere Screens gleichzeitig =
  große Architektur-Erweiterung (Host müsste N Displays parallel encoden +
  streamen, Client N Fenster/Screens mappen). Kein Quick-Win — eigener Track.

**2. Steam-Deck-Controller nativ** — *aktuell emuliert der Host Xbox.*
- **Ist-Zustand** (`src/platform/linux/input/inputtino_gamepad.cpp`): inputtino
  kann **XboxOne / Switch / PS5** — Auswahl per Client-Typ (`LI_CTYPE_*`) oder
  Config-Override. Kein Deck-/Steam-Controller-Typ.
- **Nötig für „Deck-Controller nativ"**: neuer inputtino-Joypad-Typ, der sich als
  **Steam-Deck-Controller** ausgibt (passende USB-VID/PID, damit Steam Input ihn
  als Deck erkennt + Deck-Layouts anwendet; inkl. Trackpads/Gyro/Back-Buttons).
  `inputtino` ist ein Submodul → Upstream-Beitrag oder lokaler Typ. **Nur am Deck
  verifizierbar** → zusammen, wenn Maintainer zurück ist.
- **Deck-Layouts einstellbar**: folgt aus (2) — sobald der Pad als Deck-Controller
  erscheint, greifen Steams native Per-Spiel-Layouts auf dem Deck.
- **„Erkennen welches Spiel offen ist obwohl gestreamt": schon da.** Der Host
  meldet die laufende App über `/serverinfo` (`root.currentgame` +
  `currentgameuuid`); mit Ansatz (a) ist die laufende App ein bekanntes Spiel →
  der Deck/Client kann's auslesen (für Rich-Presence / Game-Mode-Anzeige).

### Nachtrag Runde 11 (2026-07-13): CachyOS-Verifikation Host-APIs + Client C1-Fundament

Session direkt auf dem CachyOS-Test-Target (RTX 3070, Plasma **6.7**,
NVIDIA-Treiber **610.43.03** — beide neuer als bisher dokumentiert).
Repo-Checkout war 54 Commits hinter origin/dev (lokale STATUS.md noch auf
Stand 2026-05-14); per `git pull --ff-only` auf Runde 10 gebracht. Die
uncommitteten Working-Tree-Änderungen davor waren reine CRLF-Zeilenende-Churn
(Windows-Editier-Artefakt, `git diff --ignore-all-space` leer) → verworfen.

**Umgebungs-Realität (wichtig für künftige Sessions):**
- **`sudo` verlangt ein Passwort** — nicht-interaktiv nicht nutzbar. Damit
  sind `update.sh` (braucht `cmake --install` als root) und `pacman -S`
  aus einer Agent-Session **blockiert**. Workaround für Builds: fehlende
  Dev-Deps lokal ohne root nach `~/Dokumente/.localdeps/prefix` bauen.
- **Chrome-Extension nicht verbunden** → keine Browser-Automation; visuelle
  WebUI-Checks nur manuell.

**Step 1a — Host-Stand ✅ (kein Rebuild nötig):**
- Die **installierte** Binary (`/opt/sonnenschein/bin`, 12.07. 18:21) enthält
  bereits alle Runde-10-Endpoints (`strings`: `/api/library`,
  `/api/library/artwork`, `/api/update`, `/api/update-state`). Der SRC-Checkout
  (`~/.local/share/sonnenschein/src`) steht auf `3661495`. `update.sh` wäre
  also ein reiner Rebuild desselben Codes (+ am sudo-PW hängend) → übersprungen.
- **`doctor.sh`: alle 14 Checks grün.** WebUI antwortet, User-Service läuft,
  uinput/PipeWire/Portal/avahi ok, Plasma 6.7 (custom refresh rates ok).

**Step 1b — Update-Controls ✅ (Asset-Ebene):** Branch-Selector (main/dev) +
„Jetzt aktualisieren"-Button + „Aktuell"-Logik sind ins deployte Bundle
`assets/web/assets/index-DBSRB4W6.js` kompiliert (`branchOptions`,
`commits_behind`, `update_now`, `update-state`) und fetchen `/api/update-state`.
Backend liefert unauth **401** (korrekt gated). Rein visuelle Bestätigung
steht noch aus (Browser-Extension).

**Step 1c — Library-API LIVE gegen echte Steam-Installation ✅ + Fix:**
- Auth-Weg ohne Maintainer-Passwort: eine **isolierte Wegwerf-Instanz** der
  echten Binary auf Port 48989/Web 48990 mit eigener Config/State/Creds
  (Test-User, separate Pfade) — rührt `~/.config/sunshine` + Pairings **nicht**
  an, liest aber dasselbe `$HOME` → dieselbe echte Steam-Bibliothek.
- **`GET /api/library` → 31 Spiele, alle `installed:true`** (Cyberpunk, Elden
  Ring, RDR2, Witcher 3, Forza, Spider-Man, GTA V … + Runtimes), `steam_found:
  true`. `steam_roots()` deckt die CachyOS-Pfade exakt ab (`~/.steam/steam` +
  `/mnt/Games/SteamLibrary` via `libraryfolders.vdf`) → **kein Pfad-Fix nötig**.
- **`GET /api/library/artwork/<appid>`: Steam-Cache-Layout-Drift gefunden +
  gefixt.** Neuere Steam-Clients legen Cover nicht mehr nur als
  `librarycache/<appid>/library_600x900.jpg` ab, sondern verschachtelt unter
  einem Content-Hash-Ordner `librarycache/<appid>/<sha1>/…` und teils als
  `library_capsule.jpg` statt `library_600x900.jpg`. Der alte Code fand nur
  die flachen Namen → 404 für HL2 (220) und Cyberpunk (1091500), während Elden
  Ring (1245620) direkt lag und ging. **Fix in `getLibraryArtwork`**
  (confighttp.cpp): Portrait-first (`library_600x900.jpg` → `library_capsule.jpg`)
  über alle drei Layouts (flach benannt / Content-Hash-Subdir / alt-flach
  `<appid>_…`), Header (`header.jpg`/`library_header.jpg`) als Fallback, 404 nur
  wenn wirklich nichts lokal existiert (Client → SteamGridDB). **Neu gebaut +
  live gegen die isolierte Instanz re-getestet**: 220/1091500/546560/292030/
  1174180 liefern jetzt 300×450-Portrait-JPEGs, 1245620 weiter ok (Regression),
  nicht existente appid weiter 404. Commit siehe unten.
- ✅ **Artwork-Fix LIVE auf dem deployten Host, auf `main`** (2026-07-13):
  `dev → main` fast-forward (`fc793f4`), `update.sh main` erfolgreich
  durchgelaufen („✓ All 15 checks passed", „✓ Update complete"). Deployte
  `/opt`-Binary verifiziert: `/api/library` = 31 Spiele, Artwork für HL2 (220)
  + Cyberpunk (1091500) + Elden Ring → 200 JPEG, nicht-existent → 404.
- **Update-System gehärtet (2 Bugs, entdeckt beim Live-Schalten):**
  1. **Sudo-Kontext (`f189a19`)**: `sudo bash update.sh` lief komplett als root
     → `doctor.sh`-Health-Check scheiterte (root hat kein `WAYLAND_DISPLAY`,
     sieht den `--user`-Service nicht) → **Auto-Rollback** (Phase-6-Rollback
     hat sauber funktioniert!). Fix: `update.sh` re-exect sich bei sudo-Start
     als aufrufender User (Session-Env via `systemctl --user show-environment`
     wiederhergestellt), elevatet nur intern für Install. `common.sh`:
     Per-UID-Log-Pfad (behebt `/tmp/sonnenschein-install.log`-Kollision).
  2. **Root-vergiftetes Build-Dir (`fc793f4`)**: der root-Lauf hinterließ
     61 root-owned Dateien in `SRC_DIR/build` → nächster User-Build scheiterte
     an Vite `emptyOutDir` (EACCES). Fix: `build_and_install` holt sich per
     `chown` die Ownership des Build-Trees zurück, wenn Fremd-Dateien liegen
     (erhält den Cache statt `rm -rf`), und chownt `install_manifest.txt` nach
     jedem Install zurück (verhindert Wiederauftreten).
  - **Lektion für STATUS**: `update.sh` **als normaler User** starten (kein
    `sudo` davor) — es fragt beim Install nach dem Passwort. Seit `f189a19`
    ist `sudo bash update.sh` aber auch abgesichert (Re-Exec).

**Step 2 — Client-Track C1-Fundament (Moonlight-Qt-Build) ✅:**
- Upstream `moonlight-qt` v6.1.0-526 rekursiv geklont (`~/Dokumente/moonlight-qt`),
  Out-of-tree-Build `~/Dokumente/mqt-build`.
- Fehlende Deps ohne sudo gelöst: **`sdl2_ttf` 2.22** lokal aus Quelle gebaut
  (freetype+harfbuzz vorhanden) nach `~/Dokumente/.localdeps/prefix`; **Vulkan-
  Headers** (für libplacebo/vulkan.h) lokal geklont. qt6-quickcontrols2 ist in
  Arch Teil von qt6-declarative (schon da).
- **Binary baut + läuft**: `app/moonlight --version` → „Moonlight 6.1.0", alle
  HW-Renderer aktiv (FFmpeg/VAAPI/VDPAU/DRM/libplacebo-Vulkan/EGL/Wayland/X11),
  `ldd` sauber (SDL2_ttf via `LD_LIBRARY_PATH=~/Dokumente/.localdeps/prefix/lib`).
- **Pairing + App-Liste LIVE verifiziert** (autonom, ohne Maintainer-Timing):
  Client findet den Host per mDNS (`cachyos-gaming-pc.local`), spricht das
  volle Apollo-Pairing-Protokoll (getservercert → clientchallenge →
  serverchallengeresp → clientpairingsecret). Gegen eine **isolierte
  Instanz** der echten Binary (Port 48989, Test-Creds) getestet, bei der der
  PIN autonom via `POST /api/pin` bestätigt wird: Server verifiziert das
  Client-Zert (`/CN=NVIDIA GameStream Client -- verified, device name:
  sonnenschein-client`) und `moonlight list` liefert die Host-App-Liste
  (Desktop + Steam Big Picture) über mutual-TLS. Wichtig: die Isolation
  schreibt **nur** in die Tmp-Config, der reale Host bleibt bei 0 gepairten
  Devices — Pairings/Config unberührt. moonlight parst `host:port`, was das
  Testen auf Nicht-Default-Ports erlaubt.
- **Video-Stream** bewusst noch offen (Maintainer-Entscheidung Runde 11:
  „Pairing jetzt abschließen", kein Self-Loop-Stream) → echter Stream im
  2-Geräte-Test (Deck/Handy ↔ Host) oder Client-auf-Deck später.
- **Rebrand + Auto-Konfiguration erledigt** (eigenes Repo, Maintainer-Wahl
  2026-07-13 „neues eigenes Repo"): Fork lokal als `~/Dokumente/sonnenschein-client`
  (Branch `sonnenschein`, `upstream`-Remote → moonlight-qt). Rebrand: Binary
  `sonnenschein-client`, eigene QSettings-Identität `Sonnenschein/sonnenschein-client`.
  **Geräteprofil-Auto-Konfig** (`app/backend/autoconfig.{h,cpp}` + CLI
  `detect-profile`): erkennt Display (native Auflösung/höchster Refresh) + probt
  pro Codec die HW-Decode-Fähigkeit (`Session::getDecoderAvailability`, dafür in
  session.h public gemacht) → optimales Profil (Auflösung/fps/Bitrate/HDR),
  Codec bleibt `VCC_AUTO`. **Live auf CachyOS verifiziert**: `detect-profile` →
  3840×2160@60, HW-Decode H.264/HEVC/AV1, Empfehlung AV1 80 Mbit/s (10-bit/HDR
  meldet Moonlights eigener Probe auf der VAAPI-NVIDIA-Kette als nicht verfügbar
  → ehrliches Ergebnis). Commit `071fa952` im Client-Repo (lokal; GitHub-Remote
  = Maintainer-Aktion, Repo `sonnenschein-client` anlegen + pushen).
- **Library-Datenschicht im Client ✅** (`a596e7b5`): `HostLibrary`
  (`app/backend/hostlibrary.{h,cpp}`) spricht die Host-Web-API (login →
  `/api/library` → `/api/library/artwork/<appid>`), CLI-Action `library <host>
  --user --pass [--port]`. Live gegen laufenden Host verifiziert: **31 Spiele
  mit korrekten Namen + installed-Flags**.
- **Library-QML-Ansicht ✅ VISUELL VERIFIZIERT** (`a10707fa`, Client-Repo):
  `LibraryView.qml` + `HostLibraryModel` (QAbstractListModel), Cover aus
  `/api/library/artwork` in Datei-Cache (wie BoxArtManager), Portrait-Grid mit
  Namen-Caption / Namens-Fallback. Deep-Link `library-gui <host> --user --pass
  [--port]`. **Auf dem CachyOS-Desktop gestartet + Screenshot**: „Host-Bibliothek
  — 31 Spiele", HL2/Half-Life/Psychonauts/VR2 mit echten Portrait-Cover, 31
  Cover gecacht. Offen (nächste Schritte): eine **einheitliche Liste** (App-Liste
  cert-auth + Bibliothek in EIN Grid), async Cover-Laden (aktuell ~2 s blockierend),
  Toolbar-Eintrag aus einem gepairten Host, Creds-Speicherung, `/api/library`
  auf cert-Auth (Deck-Flow).
- **Einheitliche Liste (Ansatz a) ✅ HOST-SEITIG, auf `dev` (`a9bbaef`)**:
  Der Host injiziert jedes installierte Steam-Spiel als streambare App in die
  Stream-App-Liste (`process.cpp` parse: eigener Steam-Scan, Runtimes gefiltert,
  Launch via `setsid steam steam://rungameid/<appid>` detached + virtual_display,
  deterministische UUID, librarycache-Cover als Boxart; jpg-Boxart via
  `validate_app_image_path`/`appasset`). **Live verifiziert** (`moonlight list`
  über gepairten Client): Desktop + Big Picture + **26 echte Spiele**, Runtimes
  gefiltert. WebUI-`/api/apps` liest apps.json direkt → **keine Regression** der
  App-Verwaltung. **Noch offen (braucht Maintainer/Deck): Spiel wirklich
  starten+streamen** (Steam-Launch + Virtual Display) + Boxart-Rendering im
  Client-Grid + Re-Scan pro Connect (aktuell bei parse/refresh). Deshalb noch
  **nicht auf `main`** — erst Game-Launch vom Deck bestätigen.
- **C2-Richtung entschieden (Maintainer 2026-07-13): Decky-Plugin (Weg B)** —
  native Game-Mode-UX mit Live-Toggles (in ROADMAP C2 verankert).
- **Offene Auth-Entscheidung**: `/api/library` liegt aktuell hinter der
  WebUI-Passwort-Auth → der Client braucht die WebUI-Credentials. Für den
  nahtlosen Deck-Flow sollte ein **gepairter** Client die Bibliothek per
  Streaming-Zertifikat abfragen können (ohne separates Passwort) — dafür ein
  host-seitiger cert-authentifizierter Endpoint nötig. Entscheidung, bevor die
  GUI/Deck-Integration fest verdrahtet wird.

### Nachtrag Runde 10 (2026-07-12): Phase 6 fertig + Client-Track begonnen

**Scope-Entscheidungen (Maintainer, in ROADMAP.md persistiert):**
- **KDE-only** — keine weiteren Desktop-Environment-Backends (Mutter,
  wlroots, Xorg, EVDI). KWin funktioniert bestätigt.
- **Kein natives Packaging** — der One-Liner-Installer ist der
  Distributionsweg (Phase 8 = nur noch Release-Tag + Announcement).

**Phase 6 (Update-System) abgeschlossen:**
1. **Auto-Rollback** (`update.sh` v2): erfasst Vor-Update-Commit, sichert
   die Config, schreibt eine JSON-State-Datei durch jede Phase, und rollt
   bei fehlgeschlagenem Health-Check automatisch zurück (Vorgänger-Commit
   neu bauen + Config restaurieren + neu starten). Offline-Selbsttest via
   `SONNENSCHEIN_SELFTEST=1`. shellcheck grün.
2. **`revert-update.sh`**: Operator-Rollback auf beliebigen Commit,
   restauriert das jüngste Config-Backup.
3. **Auto-Update-Timer** (`update-check.sh` + `lib/updater.sh`): systemd
   `--user`-Timer prüft alle 10 min (`available.json`), wendet nur bei
   `AUTO_UPDATE=1` an. Installer-Wiring (nur User-Mode) + Uninstall
   (idempotent). State-Keys `UPDATER_TIMER`/`AUTO_UPDATE` in
   install-state.env.
4. **`GET /api/update-state`**: surft die beiden State-Dateien (Fortschritt
   + Verfügbarkeit) für die WebUI.
5. **Branch-Selector** im Dashboard (main/dev) füttert den Update-Trigger;
   „X Updates verfügbar" / „Aktuell"-Anzeige aus update-state. i18n DE+EN.
   Live getestet: Selector-Auswahl `dev` → Button sendet `{"branch":"dev"}`.
6. **Doku**: neue `docs/UPDATE_RULES.md` (Sacred Paths + Update-Vertrag);
   Migration-Framework als N/A markiert (Flat-Config, keine DB-Schemas).

Offen (Maintainer-Test, wie alle Laufzeit-Pfade): der komplette
Rollback-E2E auf einer echten Installation (Fehl-Update → Auto-Rollback)
— Logik ist shellcheck-grün + selbstgetestet, aber ein echter Build-Zyklus
lässt sich in WSL nicht sinnvoll durchspielen.

**Client-Track begonnen (C2-Fundament):**
- **`GET /api/library`** (confighttp.cpp): scannt alle Steam-Library-Folders
  (nativ `~/.steam/steam`, `~/.local/share/Steam`, flatpak, plus die in
  `libraryfolders.vdf` gelisteten Laufwerke), parst `appmanifest_*.acf`
  (VDF-Lite-Extraktor für appid/name/StateFlags), liefert die installierten
  Spiele mit Dedupe + `installed`-Flag (StateFlags-Bit 4). Non-Steam-Apps
  bleiben auf `/api/apps` — der künftige Client merged beide Listen.
  Live gegen Fixtures getestet: HL2 (installiert) + Dota 2 (nicht) korrekt,
  `steam_found`, Auth erzwungen (401).
- **`GET /api/library/artwork/<appid>`** streamt das lokale Steam-Cover
  (`appcache/librarycache/`, Portrait-Grid, beide Cache-Layouts: per-appid-
  Subdir + altes Flat-Naming), 404 → Client kann auf SteamGridDB
  ausweichen. Live getestet: existierendes Cover → image/jpeg mit
  korrekten JPEG-Magic-Bytes, fehlend → 404, Auth erzwungen (401).
- Das ist der komplette Host-seitige Baustein für die native Steam-Deck-
  Integration (die Bibliothek des Hosts inkl. Artwork im Deck-Game-Mode).
  Der eigentliche **Client (C1, Moonlight-Qt-Fork)** braucht ein echtes
  Linux/Deck-Build-Environment — nicht aus Windows+WSL baubar/testbar,
  daher hier bewusst NICHT begonnen (kein untestbarer Code auf `dev`).
  Nächster realer Schritt: Qt-Client auf einem Linux-Rechner (z.B. das
  CachyOS-System), der `/api/library` + `/api/library/artwork` + das
  bestehende Pairing/Stream-Protokoll nutzt.

### Nachtrag Runde 9 (2026-07-12): apps + config migriert, /api/update, openSUSE-CI

Live-Test-Setup wie etabliert: WSL-Binary + Assets-Staging nach
`/usr/local/assets/web` + HTTP→HTTPS-Proxy 5181 mit Cookie-Injection.
Achtung: Der E2E-Uninstall aus Runde 8 hatte die WSL-Config gewischt →
Backend war im First-Run-Modus; Zugangsdaten per `POST /api/password`
neu gesetzt (sonnenschein/test1234, nur WSL-Testinstanz).

1. **apps.html → `Apps.vue` ✅ LIVE-GETESTET**: App-Liste mit nativem
   Drag-Reorder (Logik 1:1 portiert), Edit/Delete/Launch/Export-Aktionen,
   komplettes Edit-Formular (prep/state/detached-Kommandos, alle Toggles,
   Gamepad-Override, Cover-Finder als PrimeVue-Dialog statt Bootstrap-
   Dropdown). Windows/macOS-only-Blöcke (Elevation, scale-factor,
   ds4/x360, RTSS/displayplacer-Beispiele) entfernt. Neues
   `SettingToggle.vue`: ToggleSwitch, der die Wert-Repräsentation der
   Config erhält (bool/String-Paare + inverse-values, Semantik von
   Checkbox.vue). Live verifiziert: Liste rendert Backend-Apps,
   Edit-Roundtrip persistiert (terminate-on-pause), Anlegen + Löschen
   funktionieren, null Console-Errors.
2. **config.html → `Config.vue` ✅ LIVE-GETESTET (Shell-Migration)**:
   Topbar + PrimeVue-Tab-Leiste + Save/Apply; Logik wortgleich portiert
   (Default-Population, serialize, Hash-Routing). Die 17 Bootstrap-
   Tab-Komponenten unter `configs/tabs/` bleiben unangetastet — deren
   Bootstrap-CSS/JS wird seitenlokal importiert, `data-bs-theme` folgt
   dem PrimeVue-Dunkelmodus via MutationObserver. Live verifiziert:
   Plattform-gefilterte Tabs (Linux: nv/vaapi/sw, amd/qsv/vt raus),
   Tab-Wechsel rendert Inhalte, Save-Roundtrip persistiert
   (sunshine_name), Dark-Styling kohärent, null Console-Errors.
   Voll-Migration der 17 Tabs auf PrimeVue-Formulare bleibt optionales
   Folge-Projekt (kein Funktionsverlust).
3. **Phase 6: `POST /api/update` + Dashboard-Button** (Code fertig,
   Compile-/Live-Verifikation lief zum Schreibzeitpunkt): confighttp.cpp
   `updateSelf` — löst `$PREFIX/installer/update.sh` relativ zum Binary
   auf (`/proc/self/exe` → parent/parent), validiert den Branch strikt
   (`[A-Za-z0-9._/-]{1,100}`, Shell-Injection-Guard), prüft `sudo -n`
   (ohne NOPASSWD → ehrlicher Fehler mit manuellem Befehl) und spawnt
   den Updater via `systemd-run --user --collect
   --unit=sonnenschein-update` — transiente Unit außerhalb der
   Service-cgroup, überlebt den Service-Restart; doppelter Start
   unmöglich (Unit-Name kollidiert). Log:
   `~/.local/state/sonnenschein/update.log`. Dashboard: „Jetzt
   aktualisieren"-Button bei verfügbarem Release + Erfolgs-/Fehler-
   Message (i18n dashboard.update_now/update_started DE+EN).
   Doku: neues `docs/UPDATE_RULES.md` (Sacred Paths + Update-Vertrag),
   ROADMAP Phase 6 abgehakt.
4. **openSUSE-Tumbleweed-CI-Job ✅ GRÜN + HART GESCHALTET** in
   build-linux.yml (Paketliste aus installer/packages/opensuse.list
   abgeleitet). Kurz als `experimental` gestartet, über drei
   CI-Iterationen die Paketnamen gefixt, dann Flag entfernt → jetzt
   Pflicht-Job wie die anderen drei. **Paketnamen-Fixes**: Tumbleweed
   nutzt `libminiupnpc-devel` (nicht miniupnpc-devel),
   `pkgconf-pkg-config`, und `nodejs-default`/`npm-default`
   (versionslose Meta-Pakete — feste Versionen wie nodejs22 driften aus
   dem Repo). Dieselben Namen in installer/packages/opensuse.list
   korrigiert, damit ein echtes openSUSE-Install auflöst. **CI-Matrix
   jetzt: Arch, Ubuntu 24.04, Fedora 41, openSUSE Tumbleweed — alle
   vier hart grün.**

**Hinweis Session-Infrastruktur**: Der Permission-Classifier
(claude-opus-4-8) fiel während Runde 9 zeitweise aus — Shell-Aktionen
(WSL-Build, npm build, git) waren blockiert; Datei-Edits liefen weiter.
Verifikationsschritte wurden nachgeholt, sobald die Shell wieder ging.

### Was weiterhin offen ist (Maintainer-Test auf CachyOS)
- **Nach Runde-2-Update**: `bash /opt/sonnenschein/installer/update.sh` →
  läuft Build + doctor --repair automatisch. Erwartung: alle Checks grün,
  kein Fatal-Banner mehr in der WebUI, Steam Deck findet den Host.
- Stream-Test Steam Deck: Auflösung 1280x800 Landscape, kein Dialog ab
  zweitem Start, 90 Hz (60-Hz-Fix v3 aus Session 2026-05-14), HDR
- Uninstall-Test: `bash /opt/sonnenschein/installer/uninstall.sh` → System sauber

---

## Session 2026-05-28 — Overhaul + erste Vorab-Version auf `main`

> Ausgeführt in einer **Headless-Cloud-Umgebung ohne GPU/Wayland/Compositor/Moonlight-Client**.
> Alles Build-/Lint-/Config-/Rebrand-Verifizierbare ist hier geprüft; alle
> **Laufzeit-Fixes (60 Hz, HDR, Cursor, Stream)** bleiben **CachyOS-Test offen**.

### Was erledigt + verifiziert wurde

**Phase 1.6 — Rebrand Apollo/Sunshine → Sonnenschein ✅**
- Binary heißt jetzt `sonnenschein` (`OUTPUT_NAME`), mit **`sunshine`-Kompat-Symlink** beim Install (`cmake/packaging/unix.cmake`). CMake-Target-Name bleibt intern `sunshine` (quer durch viele Module referenziert) — bewusst, minimiert Bruch.
- FQDN `io.github.elias02345.Sonnenschein`. Alle Packaging-Artefakte umbenannt: `.desktop`/`.terminal.desktop`/`.metainfo.xml` (linux, flatpak, AppImage), `sonnenschein.service.in`, `60-sonnenschein.{rules,conf}`, Arch `PKGBUILD` + `sonnenschein.install`, Logos `sonnenschein.{svg,png,ico,icns}`.
- Alle Build-Skript-/Test-Referenzen auf die umbenannten Dateien gefixt (`tests/CMakeLists.txt`, `tests/integration/test_external_commands.cpp`, `scripts/linux_build.sh`, `special_package_configuration.cmake`).
- Flatpak-Helfer-Skripte (`additional-install.sh`, `remove-additional-install.sh`, `sonnenschein.sh`), `postinst` setcap-Pfad, Publisher-Strings auf Sonnenschein/Elias umgestellt; `SUNSHINE_ASSETS_DIR` flatpak/arch → `share/sonnenschein`.
- C++ user-facing Strings: mDNS-Instance-Fallback (`network.cpp`), Default-Device-Name (`nvhttp.cpp` → `SonnenscheinDisplay`), Tray-Labels (`system_tray.cpp`). **Alle 18 WebUI-Locale-Dateien** (`en.json` … `zh.json`) per Sammel-Replace „Apollo" → „Sonnenschein" (JSON validiert, Locale-Keys unverändert).
- **Bewusst NICHT geändert** (Bruchrisiko vs. Nutzen): On-Disk-Pfade `~/.config/sunshine`, `sunshine.conf`, `sunshine.log`, `sunshine_state.json` (Maintainer hat funktionierendes Pairing); interne `SUNSHINE_*` CMake-Vars und C++-Namespaces; Windows-Service-Namen (`ApolloService`, `SERVICE_NAME`); Web-Image-Dateinamen (`apollo-*.svg`, `logo-apollo-45.png`, `apollo.css`, `apollo_version.js`) + zugehörige `confighttp.cpp`-Route — Platzhalter bis neues Logo (TBD).
- **Verifiziert**: `npm run build` grün; CMake-Configure erreicht `PROJECT_NAME: Sonnenschein` und parst alle `configure_file`-Sektionen; Grep-Audit zeigt keine user-facing „dev.lizardbyte.app.Sunshine"/`60-sunshine`-Referenzen mehr außerhalb Attribution/third-party.

**Phase 3 — Installer-Gerüst ✅ (Gerüst; reale Distro-Runs = Maintainer-Test)**
- `installer/` neu: `install.sh` (curl|bash-Bootstrap via git-clone + re-exec, Distro-/GPU-/Compositor-Detect, From-Source-Build, Service-Install), Libs `lib/{common,distro,gpu,compositor,packages,service,permissions,ui}.sh`, Paketlisten `packages/{arch,debian,fedora,opensuse}.list`, plus `update.sh`/`uninstall.sh`/`post-install.sh`.
- Default **systemd `--user`-Service** (DBus/WAYLAND_DISPLAY, §9.7). `set -euo pipefail` + ERR-Trap + Logfile, idempotent.
- **Verifiziert**: `shellcheck -x -S warning` über alle Skripte sauber (exit 0); `bash -n` ok; Detection-/Paketlisten-Logik per Smoke-Test geprüft.

**Phase 5 — WebUI PrimeVue-4-Fundament ✅ (Fundament; Browser-Test = Maintainer)**
- PrimeVue 4 + `@primevue/themes` (Aura-Preset, amber-gebrandet) + `primeicons` zu `package.json`. Neuer Bootstrap `primevue_init.js` (Dark-Default + System-Folger) **isoliert** von den Bestands-Bootstrap-Seiten → schrittweise Migration ohne Regression.
- Repräsentative `Diagnostics.vue` (Platform/Compositor/GPU/Version-Karten) als eigener Vite-Entry `diagnostics.html`. Bestehende Seiten unangetastet.
- **Verifiziert**: `npm run build` grün inkl. neuer Diagnostics-Bundle.

**Code-Review der Laufzeit-Fixes ✅ (Review; Fixes selbst CachyOS-Test offen)**
- Punch-List siehe **§9.21** (neu). Zwei triviale Härtungen eingebaut (Null-Guard in `factory.cpp`, korrigierter Crash-Handler-Kommentar in `main.cpp`). Verhaltensändernde/HW-abhängige Funde sind dort als „CachyOS-Test/Maintainer-Entscheidung" markiert — **kein** Patch am Capture-Pfad (Lehre aus §9.20 Codex-Rollback).

### Push
- Branch `claude/app-overhaul-rebranding-swIMz` (auf `origin/dev` basiert, thematische Commits) → **`main`** (erste Vorab-Version, vom Maintainer explizit autorisiert) → `dev` synchron.

### Was weiterhin offen ist (Maintainer-Test auf echter Hardware)
- 60-Hz-Fix v3, HDR-Aktivierung, Cursor — realer Stream auf CachyOS Plasma 6.6.4 + RTX 3070 / SteamDeck.
- Installer-Runs auf echten Distros (Arch/CachyOS, Fedora, Ubuntu, openSUSE).
- WebUI-Bedienung im Browser (Bestands-Bootstrap-Seiten + neue PrimeVue-Diagnostics).

---

## Inhaltsverzeichnis

1. [Maintainer & Hardware](#1-maintainer--hardware)
2. [Repo, Branches, CI](#2-repo-branches-ci)
3. [Architektur in 5 Sätzen](#3-architektur-in-5-sätzen)
4. [Tech-Stack-Entscheidungen (Q1–Q28 + Defaults)](#4-tech-stack-entscheidungen-q1q28--defaults)
5. [Build-Setup (drei Wege)](#5-build-setup-drei-wege)
6. [Phasen-Status](#6-phasen-status)
   - [Phase 0 — Vorbereitung](#phase-0--vorbereitung)
   - [Phase 1 — Linux-Build-Pipeline](#phase-1--linux-build-pipeline)
   - [Phase 2 — Virtual-Display-Abstraktion](#phase-2--virtual-display-abstraktion)
   - [Phase 3 — Installer & Service](#phase-3--installer--service)
   - [Phase 4 — HDR & AV1](#phase-4--hdr--av1)
   - [Phase 5 — WebUI v1 (PrimeVue 4)](#phase-5--webui-v1-primevue-4)
   - [Phase 6 — Update-System](#phase-6--update-system)
   - [Phase 7 — Tests, Performance, Doku](#phase-7--tests-performance-doku)
   - [Phase 8 — 1.0 Release](#phase-8--10-release)
7. [Modul-Landkarte (Wo lebt was)](#7-modul-landkarte-wo-lebt-was)
8. [Submodules](#8-submodules)
9. [Bekannte Probleme & Workarounds](#9-bekannte-probleme--workarounds)
10. [Letzte Commits chronologisch](#10-letzte-commits-chronologisch)
11. [Datei-Inventar (was wir geschrieben haben)](#11-datei-inventar-was-wir-geschrieben-haben)
12. [Was als nächstes — konkrete Schritte](#12-was-als-nächstes--konkrete-schritte)
13. [Update-Konventionen für diese Datei](#13-update-konventionen-für-diese-datei)
14. [Deep-Dive: PipeWire & Portal Integration (Phase 4)](#14-deep-dive-pipewire--portal-integration-phase-4)

---

## 1. Maintainer & Hardware

| Feld | Wert |
|---|---|
| Name | Elias Kanakidis |
| GitHub | [@Elias02345](https://github.com/Elias02345) |
| Email | elias-kanakidis@gmx.de |
| Sprache | **Deutsch** (mit Maintainer immer auf Deutsch antworten) |
| Dev-Workstation | Windows 11 + WSL2 Ubuntu 24.04 + Cursor/VSCode |
| **Test-Target** (CachyOS-Gaming-PC) | CachyOS rolling, Plasma 6.6.4 Wayland, KWin 6.6.4, NVIDIA RTX 3070, NVIDIA Driver 595.71.05, CUDA 13.2.1, libva 2.23.0, FFmpeg 8.1.1, Boost 1.91 (modular install — `system` component fehlt im default boost-Paket), CMake 4.3.2, GCC 16.1.1, Git 2.54.0, Node ≥20 |
| Login-Shell | `fish` (relevant für Heredocs/Quoting in Befehlen) |

**Hardware-Implikation**: NVIDIA-Pfad ist Phase-2-Hauptziel. RTX 3070 + Driver 595 hat alles für Wayland-HDR (`VK_EXT_hdr_metadata` ab 580.94.11). Aber der Maintainer will explizit: **alle drei GPU-Vendoren (NVIDIA, AMD, Intel) müssen voll funktionieren**, mit automatischer Backend-Auswahl + Multi-GPU-Manual-Override in der WebUI.

---

## 2. Repo, Branches, CI

- **Repo**: <https://github.com/Elias02345/sonnenschein> (public, GPL-3.0, 12 Topics, Discussions an, Wiki aus, Default-Branch `main`)
- **Branches**:
  - `main` — stabile Releases. Aktuell beim Initial-Commit `235920b`. Bleibt da bis Phase-7-Done + 1.0-Release.
  - `dev` — aktive Entwicklung. **Hier passiert alles.** PRs sollten gegen `dev` gerichtet sein, nicht gegen `main`.
  - keine kurzlebigen Feature-Branches aktuell aktiv.
- **GitHub-Konfiguration**:
  - Topics: `linux gaming streaming moonlight sunshine apollo gamestream wayland kde gnome hdr virtual-display`
  - Description: "Linux-native, headless Moonlight streaming backend with auto-matching virtual displays. Sunshine/Apollo-derived."
  - Discussions enabled.
- **CI** (`.github/workflows/`):
  - `lint.yml` — shellcheck + prettier + clang-format auf Push/PR.
  - `build-linux.yml` — Build-Matrix für Arch / Ubuntu 24.04 / Fedora 41. Aktuell `continue-on-error: true` während Phasen 1+2 stabilisieren — Flag entfernen wenn Distro 100 % grün baut. CI-Logs unter <https://github.com/Elias02345/sonnenschein/actions>.
  - Submodules in CI: `submodules: recursive`, weil KWin Direct ScreenCast das Plasma-Wayland-Protokoll-Submodule braucht.

---

## 3. Architektur in 5 Sätzen

```
            ┌──────────────────────────────────────┐
 Moonlight  │  Pairing → ResExchange → RTSP/UDP    │  Sonnenschein-Server (C++23)
   Client → │  Capabilities (HDR? AV1? Auflösung?) │  ├─ Virtual-Display-Abstraktion (NEU)
            └──────────────────────────────────────┘  │   src/platform/linux/virtual_display/
                                                      │   (Mutter / KWin / AMDGPU /
                                                      │    wlroots / Xorg / EVDI)
                                                      ├─ Capture (PipeWire-Portal /
                                                      │           KMS direct)
                                                      ├─ Encoder (NVENC / VAAPI /
                                                      │           Vulkan-Video)
                                                      ├─ Audio (PipeWire / PulseAudio)
                                                      ├─ Input (inputtino / uinput)
                                                      └─ WebUI (Vue 3 + PrimeVue 4)
                                                          ├─ Setup-Wizard
                                                          ├─ Diagnose-Tab
                                                          ├─ Update-Manager
                                                          └─ Crash-Reporter → GitHub
```

Der **virtual_display**-Layer ist Sonnenscheins eigentliche Innovation. Sechs Backend-Implementierungen (in `src/platform/linux/virtual_display/backends/`), zur Laufzeit gewählt via `factory::select_backend(env, cfg)`. Wir leihen Apollos C++-Stack komplett aus, hängen die Linux-Implementierung in den Windows-`#ifdef _WIN32`-Pfad als `#elif __linux__` ein.

---

## 4. Tech-Stack-Entscheidungen (Q1–Q28 + Defaults)

Vom Maintainer in der initialen Session bestätigt. **Nicht hinterfragen** — wenn du einen davon zur Diskussion stellen willst, frag explizit.

### A — Scope & Plattform
1. **Mindest-Distros**: Ubuntu 22.04+, Debian 12+, Fedora 40+, Arch (rolling) ✅
2. **X11-Support**: ja, als Fallback. Wayland-first.
3. **NVIDIA-Treiber**: neueste (≥ 580.94.11 für Wayland HDR; Maintainers Driver: 595.71.05).
4. **Compositors First-Class**: KDE Plasma (Hauptfokus) + GNOME + Sway + Hyprland + Cosmic + niri (best-effort).
5. **HDR auf NVIDIA**: ja zur 1.0, mit SDR-Fallback wenn nicht verfügbar.

### B — User-Erlebnis
6. **Installer-Frontend**: Text-UI mit interaktiven Elementen (whiptail/dialog).
7. **Auto-Start als Service**: Hintergrund-Dienst, kein Terminal nötig nach Login.
8. **Multi-User**: ein Account pro System (für 1.0).
9. **Auth**: User/Passwort + Login-Screen (Browser-Pass-Save-fähig).
10. **Sprachen**: DE + EN zur 1.0, mehr via Crowdin nach Release.

### C — Technische Detail-Entscheidungen
11. **Fork-Strategie**: Apollo als Basis, Sunshine-Upstream-Sync regelmäßig.
12. **Audio**: PipeWire Default + PulseAudio Fallback.
13. **Gamescope**: opt-in pro App (WebUI-Toggle).
14. **WebUI-Framework**: **PrimeVue 4** (modern, hochwertige Animationen, gamer-tauglich).
15. **Theme**: Dunkelmodus-Default, optional Hell + System-Default-Folger.
16. **Mobile-WebUI**: ja, responsive bis 360 px.

### D — Operational
17. **Update-Modell**: GitHub-Releases, automatisch + auf Knopfdruck.
18. **Telemetrie**: opt-in Crash-Reporter, sammelt anonymisierten Stacktrace + System-Info, öffnet pre-filled GitHub-Issue auf [Elias02345/sonnenschein/issues](https://github.com/Elias02345/sonnenschein/issues/new?template=crash.yml). Kein Server-Backend.
19. **Logging**: journald als Primary, JSON-strukturiert auch unter `~/.local/state/sonnenschein/logs/` mit 7-Tage-Rotation. Live-Log-Tab in WebUI streamt journalctl.
20. **Migrations**: pro Major-Version Up-Script + Backup vor jedem Lauf, Rollback-Befehl.

### E — Branding & Repo
21. **Branding**: modern, gamer-like (Logo TBD — aktuell Apollo-Logos als Platzhalter).
22. **Tagline**: _"Linux-Game-Streaming, das einfach funktioniert."_
23. **Repo-Layout**: Monorepo (server + WebUI + installer + docs in einem Repo).
24. **Branch-Strategie**: `main` stable + `dev` development. Update default = `main`. Wenn `main` neuer als `dev` → automatisch zu `main`.
25. **Release-Kadenz**: rolling.

### F — Hardware & Performance
26. **Steam + Proton**: ja, der Standard-Pfad für Linux-Gaming. Anti-Cheat (EAC/Vanguard) ist kein Ziel.
27. **Performance-Ziel**: <30 ms LAN-Latenz, hohe Qualität, adaptiv ans Netzwerk.
28. **Hardware-Mindeststandard**: so weit wie möglich. RTX 3070 muss explizit perfekt funktionieren (Maintainer-Hardware). Codec-Reihenfolge: AV1 wenn supported → HEVC → H.264.

### Zusatz vom Maintainer (nicht durchnummeriert)
- **Uninstaller**: ja, sauberes Entfernen aller Komponenten.
- **GPU-Auswahl**: automatisch per Detection, manueller Override in WebUI (Multi-GPU-Systeme).
- **NEU 2026-07-11 — Eigener Client (revidiert das frühere Kein-Ziel!)**: Sonnenschein bekommt einen eigenen Client für Linux/Steam Deck (zuerst), Android und Windows. Automatische Geräte-Konfiguration (Auflösung/Refresh/HDR/Decoder je Gerät), zwei Profile: **Gaming** und **Remote Desktop**. Killer-Feature: **native Steam-Deck-Integration** — Host-Bibliothek erscheint im Steam Game Mode des Decks (shortcuts.vdf-Sync + Decky-Plugin), Spiele die auf beiden installiert sind wählbar „Lokal/Stream", Host-only-Spiele streamen direkt — alles über Sonnenschein, NICHT Steam Remote Play. Fokus: Gaming vor Remote Desktop, Deck vor Android/Windows. Ausgearbeitet in `docs/ROADMAP.md` → „Client-Track" (C1–C4).
- **NEU 2026-07-11 — Boot-to-Ready-Host**: Host startet leise als Service beim Boot, idlet lightweight, unterstützt Wake-on-LAN (Installer konfiguriert `ethtool wol g`, Client bekommt Aufweck-Knopf, Internet-WoL via VPN-Empfehlung), optional login-freies Streamen via SDDM-Autologin (opt-in mit Sicherheitshinweis), später evtl. dedizierte Headless-Session. Ausgearbeitet in ROADMAP → „Host-Bereitschaft: Boot-to-Ready".

---

## 5. Build-Setup (drei Wege)

### A) GitHub Actions CI
- Pro Push auf `main` oder `dev` und auf jedem PR.
- Container für Arch (latest), Ubuntu 24.04, Fedora 41.
- Aktuell **permissiv** (`continue-on-error: true`) während Phase 1+2 stabilisiert. Wenn Phase 7 erreicht ist, dieses Flag entfernen und CI hart machen.
- Submodules: seit KWin-Direktcapture `submodules: recursive`, weil `third-party/plasma-wayland-protocols` für `zkde-screencast-unstable-v1.xml` benötigt wird.

### B) WSL2 Ubuntu 24.04 (auf Maintainers Windows-PC)
- Build-Verzeichnis: **`/root/snsbuild`** (NICHT `/tmp/snsbuild` — `/tmp` wird beim WSL-Restart gewipt!).
- Source bleibt unter `/mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein` (Windows-mounted).
- Kommando aus PowerShell:
  ```powershell
  wsl -d Ubuntu --user root -- bash -c "cd /root/snsbuild && cmake -S /mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DSUNSHINE_BUILD_DOCS=OFF -DSUNSHINE_BUILD_HOMEBREW=OFF -DSUNSHINE_BUILD_FLATPAK=OFF -DSUNSHINE_ENABLE_CUDA=OFF && cmake --build . --target sunshine -j8"
  ```
- **Wichtige WSL-Quirks**:
  - `sudo` braucht Passwort; nutze `wsl --user root` direkt.
  - `/tmp` überlebt keinen WSL-Restart — Build-Dir auf `/root` legen.
  - libva 2.20 (Ubuntu Stock) ist zu alt; wir bauten libva 2.22 from source einmalig (Anleitung in `docs/building.md`). Nicht jedes Build wiederholen.
  - Build-Performance: /mnt/c-IO ist langsam (NTFS-via-9P). Cold configure ~6 min, Build ~10-15 min auf 8 Cores. Inkrementell ~3 min nach Code-Änderung.

### C) CachyOS-Rechner (Maintainers Test-Hardware)
- **Test-Target**, das einzige System, auf dem wir den realen End-to-End-Stream testen.
- Pfad zum Repo: `~/sonnenschein`.
- **Build geht über SSH** (kein Display nötig). **Stream-Test geht NICHT über SSH** (kscreen-doctor braucht WAYLAND_DISPLAY + DBus-Session-Bus, deshalb eine Konsole **innerhalb der Plasma-Sitzung** öffnen).
- Build-Sequenz:
  ```bash
  cd ~/sonnenschein
  git fetch && git checkout dev && git pull
  git submodule update --init --recursive   # ~5-10 min Erstmal
  rm -rf build && mkdir build && cd build
  cmake -G Ninja -S .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=OFF \
      -DSUNSHINE_BUILD_DOCS=OFF \
      -DSUNSHINE_BUILD_FLATPAK=OFF
  cmake --build . -j$(nproc)
  ```
- **Ergebnis**: `~/sonnenschein/build/sunshine-0.0.0` (Binary) + `~/sonnenschein/build/sunshine` (Symlink).

---

## 6. Phasen-Status

Legende: ✅ done · 🟡 in_progress · 🔴 blocked · ⏸ pending

### Phase 0 — Vorbereitung ✅

**Ziel**: Repo strukturiert, Lizenz, Doku-Skelett, Community-Files, CI-Skelett.

**Erfolg**: GitHub-Repo public, Branches `main` + `dev`, README/ROADMAP/CONTRIBUTING/SECURITY/Issue-Templates/CODE_OF_CONDUCT alle drin, GitHub Actions skelettiert.

| Aufgabe | Status | Commit | Notes |
|---|---|---|---|
| Apollo-Codebasis als Basis importieren | ✅ | `235920b` | 479 Dateien, 99 k Zeilen, Squash-Import (keine Apollo-Historie) |
| `_reference/` mit Sunshine + docker-steam-headless als Referenz | ✅ | gitignored | im Working-Dir, nicht im Repo |
| LICENSE (GPL-3.0) + NOTICE | ✅ | `235920b` | Attribution für Sunshine + Apollo + docker-steam-headless |
| README.md (DE/EN, modern, Architektur-Diagramm, Status-Tabelle) | ✅ | `235920b` | |
| docs/ROADMAP.md (8 Phasen, Crash-Reporter-Konzept) | ✅ | `235920b` | |
| .github/CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md, PULL_REQUEST_TEMPLATE.md | ✅ | `235920b` | |
| Issue-Templates (bug, feature, **crash** für WebUI-Reporter pre-fill, config) | ✅ | `235920b` | crash.yml hat anonymized-Checkbox |
| .github/workflows/lint.yml | ✅ | `235920b` | shellcheck + prettier + clang-format |
| .github/workflows/build-linux.yml | ✅ | `235920b` | Arch/Ubuntu/Fedora-Matrix, continue-on-error: true |
| package.json: name → "sonnenschein" | ✅ | `235920b` | Vue 3.5.22, Vite 6.3.6 |
| .gitignore erweitert (`_reference/` etc.) | ✅ | `235920b` | |
| .gitmodules ursprünglich geparkt (Phase 0), in Phase 1 wiederhergestellt | ✅ | `a95f2ee` | siehe Phase 1 |
| Initial-Commit + Push zu `dev` und `main` | ✅ | `235920b` | |
| GitHub Repo-Settings (Topics, Description, Default-Branch, Discussions) | ✅ | via gh CLI | 12 Topics, default `main`, Discussions an, Wiki aus |

### Phase 1 — Linux-Build-Pipeline ✅

**Ziel**: Apollo-Codebasis baut auf Linux ohne harte Patches, sunshine-Binary läuft, WebUI-Bundle wird produziert.

**Erfolgskriterium erreicht**: 270/270 Build-Steps grün auf WSL2 Ubuntu 24.04. Binary 33 MB, `--help` zeigt korrektes Usage. WebUI-Bundle in `build/assets/web/`.

| Aufgabe | Status | Commit | Notes |
|---|---|---|---|
| Submodules eingebunden via `_reference/add_submodules.sh` + später Plasma-Protokolle | ✅ | `a95f2ee` + `4c63d36` | inkl. moonlight-common-c/enet rekursiv, tray gepinnt auf `7936cb35`, neu `third-party/plasma-wayland-protocols` für KWin Direct ScreenCast |
| Build-Deps in WSL2 installiert | ✅ | (manuell) | Liste in `docs/building.md` |
| **libva 2.22 from source** in WSL2 | ✅ | (manuell) | Ubuntu 24.04 Stock libva 2.20 fehlt `vaMapBuffer2`, das build-deps' FFmpeg braucht |
| Node 20 in WSL2 (statt Node 18) | ✅ | (manuell) | Vite 6 + @vitejs/plugin-vue 6 brauchen Node ≥20.19 (`crypto.hash`) |
| WebUI-Build (npm install + npm run build) | ✅ | (zur Verifikation) | 17 Module via Vite, ~2 min |
| Fix: `tray_linux.c` (nicht `.cpp`) — Apollo erwartete .c, master-tray hat zu Qt+`.cpp` migriert; deshalb tray-Submodule auf `7936cb35` (letzter GTK-Commit) gepinnt | ✅ | `a95f2ee` | |
| cmake configure grün | ✅ | (verifikation) | `-DSUNSHINE_ENABLE_CUDA=OFF` für WSL (kein NVIDIA) |
| cmake build grün, sunshine-Binary kompiliert | ✅ | (verifikation) | 33 MB ELF, läuft mit `--help` |
| docs/building.md komplett neu für Sonnenschein | ✅ | `539d3a5` | Per-Distro-Lists (Arch/Ubuntu/Fedora/openSUSE), libva-2.22-Anleitung, WSL2-Workflow, Troubleshooting-Sektion |
| Phase 1.6: CMake `project(Apollo)` → `project(Sonnenschein)` Rebrand | ✅ (Rest kosmetisch) | — | Real erledigt (verifiziert 2026-07-16): `project(Sonnenschein)` in CMakeLists.txt:7, eigene FQDN-Service/Desktop-Files. Nur noch intern offen: „Sunshine Branch:"-Logzeile (build_version.cmake:57) + Target-Name `sunshine` (common.cmake:4) → §12 C. |

### Phase 2 — Virtual-Display-Abstraktion 🟡

**Ziel**: Apollos Killer-Feature (Auto-Match Virtual Display beim Pairing) auf Linux portieren. Multi-Backend-Abstraktion mit automatischer Auswahl + Manual-Override.

#### Phase 2A — Foundation ✅ ([`7cdd3f0`](https://github.com/Elias02345/sonnenschein/commit/7cdd3f0))

| Datei | LOC | Inhalt |
|---|---|---|
| `src/platform/linux/virtual_display/types.h` | 135 | Enums (DisplayServer, Compositor, GpuVendor) + GpuInfo / Environment / CreateRequest / Handle |
| `src/platform/linux/virtual_display/interface.h` | 59 | `IBackend` Vertrag (name, display_name, available, create, destroy, destroy_all) |
| `src/platform/linux/virtual_display/detection.h+cpp` | 29+431 | `detect()` Environment-Detection: WAYLAND_DISPLAY/DISPLAY/XDG_*/`/sys/class/drm/card*`/`/proc/driver/nvidia/version`/lspci → Environment-Struct |
| `src/platform/linux/virtual_display/factory.h+cpp` | 52+94 | `select_backend(env, cfg)` Auto-Selection mit `preferred_backend`-Override aus sonnenschein.conf |
| `src/platform/linux/virtual_display/backends/all.h` | 53 | `make_*()` Factory-Function-Deklarationen |
| 7 Backend-Stub-Dateien (`backends/{kwin_wayland, mutter_headless, wlroots_headless, xorg_nvidia, xorg_dummy, amdgpu_param, evdi}.cpp`) | je ~50 | Jeder Stub: korrektes `available()` + leeres `create()/destroy()` + ausführlicher `Plan:`-Doc-Block für die spätere Implementierung |
| `src/platform/linux/virtual_display/README.md` | ~150 | Architektur, File-Map, Status-Tabelle, Wiring-Point für Phase 2C |
| `cmake/compile_definitions/linux.cmake` | +14 | Alle 14 Files in `PLATFORM_TARGET_FILES` aufgenommen |

**Build-Verifikation**: 270/270 Steps grün, Binary verlinkt mit 135 `sonnenschein::vdisplay`-Symbolen (via `nm`).

#### Phase 2B — KWin-Wayland Backend ✅ ([`509e87a`](https://github.com/Elias02345/sonnenschein/commit/509e87a))

| Datei | LOC | Inhalt |
|---|---|---|
| `src/platform/linux/virtual_display/subprocess.h+cpp` | NEU, ~210 | `run_shell(cmd, desc)` (popen-basiert) + `run_args(argv, desc)` (fork+execvp). Beide capturen stdout+stderr (≤64 KB), decoden wstatus, BOOST_LOG-Diagnostik. |
| `src/platform/linux/virtual_display/backends/kwin_wayland.cpp` | REWRITE, ~250 | `KwinWaylandBackend`: ruft `kscreen-doctor add-virtual-output NAME WIDTH HEIGHT`, dann `kscreen-doctor output.NAME.mode.WxH@HZ` für genaue Refresh-Rate, dann `output.NAME.hdr.true` wenn Client HDR-fähig. Stable per-client display-naming `Sonnenschein-<8-hex-chars-aus-client-uid>`. Thread-safe `std::map<display_id, ActiveDisplay>` für Cleanup. "already exists"-Recovery durch remove + re-add. |
| `cmake/compile_definitions/linux.cmake` | +2 | subprocess.h/.cpp in PLATFORM_TARGET_FILES |

**Build-Verifikation**: 10/10 incremental steps grün, `nm` zeigt `make_kwin_wayland`, `run_shell`, `run_args` im Binary.

**Zielsystem**: Plasma 6.4+ (`add-virtual-output` Subcommand). User hat Plasma 6.6.4 → kompatibel. Falls Plasma <6.4: KWin-Plugin-Backend nötig (Phase 2B.5, noch nicht implementiert).

#### Phase 2C — process.cpp Wiring ✅ ([`72cf5c1`](https://github.com/Elias02345/sonnenschein/commit/72cf5c1))

Apollo's `process.cpp` hatte einen `#ifdef _WIN32`-Block für Virtual Display (SudoVDA / `VDISPLAY::createVirtualDisplay`). Wir haben einen Sibling `#elif __linux__`-Block hinzugefügt.

| Datei | Änderung |
|---|---|
| `src/process.h` | `#elif __linux__ #include "platform/linux/virtual_display/interface.h"` + zwei neue private Member unter `#ifdef __linux__`: `std::unique_ptr<sonnenschein::vdisplay::IBackend> _vdisplay_backend;` und `std::optional<sonnenschein::vdisplay::Handle> _vdisplay_handle;` |
| `src/process.cpp` | Zwei `#elif __linux__`-Blöcke: einer in `execute()` (~70 LOC, ruft detect → select_backend → create), einer in `terminate()` (~10 LOC, ruft `backend->destroy(handle->display_id)`). Gleichzeitig Restrukturierung des bestehenden `#ifdef _WIN32 ... #else ... #endif` zu `#ifdef _WIN32 ... #elif __linux__ ... #else ... #endif`, sodass `display_device::configure_display()` + `reset_persistence()` für beide Plattformen mit Virtual Display laufen. macOS-Pfad bleibt unverändert. |

**Build-Verifikation**: 3/3 incremental Steps grün auf WSL2, Binary funktioniert.

**Effekt**: Wenn Sonnenschein auf KDE Plasma 6+ Wayland läuft und ein Moonlight-Client paart, der virtuelle Display angefordert hat (oder `headless_mode` global ist, oder die App-Config `virtual_display=true` hat), wird ein KWin Virtual Output mit der genauen WxH@HZ + HDR des Clients erzeugt — wie auf Windows mit SudoVDA, aber Linux-nativ.

#### Phase 2D — End-to-End Test auf CachyOS 🟡 (in Arbeit, blockiert)

**Ziel**: Maintainer testet das Sonnenschein-Binary auf seinem RTX-3070 + Plasma-6.6.4-Wayland-System mit einem realen Moonlight-Client.

**Build-Fixes** (iterativ gelöst):

1. **Boost ALIAS-Target-Kollision** (gefixt in [`b6f263e`](https://github.com/Elias02345/sonnenschein/commit/b6f263e)) — Pre-Flight-Check + FetchContent-Fallback.
2. **CUDA nicht gefunden auf Arch/CachyOS** (gefixt in [`b6f263e`](https://github.com/Elias02345/sonnenschein/commit/b6f263e)) — `/opt/cuda/bin` Auto-Detection.
3. **CUDA 13.2 inkompatibel mit GCC 16.1.1** (gefixt in [`5e1b1cf`](https://github.com/Elias02345/sonnenschein/commit/5e1b1cf)) — GCC-Versionsprüfung vor `check_language(CUDA)`, graceful skip.
4. **CUDA_FAIL_ON_MISSING feuerte trotz intentionalem Skip** (gefixt in [`8126f26`](https://github.com/Elias02345/sonnenschein/commit/8126f26)) — Guard-Bedingung erweitert.

**Erster E2E-Test (2026-05-10) — Ergebnis: STREAMING FUNKTIONIERT! ✅**

- Binary erfordert `sudo setcap cap_sys_admin+p` (siehe §9.10) → danach fehlerfrei.
- WebUI erreichbar auf `https://localhost:47990` → User+Passwort gesetzt.
- **SteamDeck via Moonlight erfolgreich gepairt und gestreamt!** Bildschirmspiegelung des Hauptmonitors (HDMI-A-1, Samsung 4K).
- Encoder: nicht im Log welcher griff, aber Stream lief → mindestens einer (wahrscheinlich VAAPI oder Software) funktionierte nach `cap_sys_admin`.
- **Virtual Display wurde NICHT erzeugt** — das ist **erwartet und korrekt**: der Code-Pfad in `process.cpp:346-350` triggert nur wenn:
  - `config::video.headless_mode == true` (Config-Datei: `headless_mode = true`), ODER
  - `launch_session->virtual_display == true` (Client-Request), ODER
  - `_app.virtual_display == true` (per-App-Setting in WebUI/apps.json)
  - Default für alle drei: `false`.

**Hauptanwendungsfall (geklärt 2026-05-10)**: Physische Monitore sind verbunden, sollen sich beim Streaming deaktivieren und Platz für einen virtuellen Monitor machen. Virtual Display wird dann der einzige aktive Output. Headless (kein physischer Monitor) ebenfalls unterstützt.

### Phase 2E — Capture-Routing Fix ✅

**Problem 1**: `map_display_name()` gibt auf Linux `{}` zurück (Windows-only `sm_instance`). `kmsgrab.cpp` parst den leeren String als Integer → `1105514439` (Müll) → `Couldn't find monitor [1105514439]`.

**Fix (Commit `9baebbf`)**: `config::video.output_name = "0"` statt `map_display_name()`.

**Problem 2**: `video.cpp` Zeile 1180 nutzt `proc::proc.display_name` direkt für KMS-Capture, NICHT `config::video.output_name`. `this->display_name` war auf `"Sonnenschein-00E8F1E1"` gesetzt → `util::from_view()` parsete Müll.

**Fix (Commit `71cdac3`)**: `this->display_name = "0"` statt `_vdisplay_handle->output_name`. KMS findet nun Monitor 0 (HDMI-A-1) fehlerfrei.

### Phase 2 — Architektur-Erkenntnis 🔴

**Zentrale Erkenntnis nach CachyOS-Tests**: KMS-Capture kann Virtual Displays **prinzipbedingt nicht** capturen.

| Schicht | Was sie sieht | Capture möglich? |
|---------|--------------|------------------|
| DRM/KMS (`kmsgrab.cpp`) | Physische Connectors (HDMI, DP) | ✅ Nur physische Displays |
| KWin Compositor | Alle Outputs inkl. Virtual | ❌ Keine Capture-API |
| PipeWire Portal | Alle Outputs via `xdg-desktop-portal` | ✅ Auch Virtual Displays |
| wlr-screencopy | Compositor-Outputs | ❌ KDE unterstützt es nicht |

**Konsequenz**: Für den Virtual-Display-Capture-Use-Case muss ein **PipeWire-Capture-Backend** implementiert werden (Phase 4). Das ist der einzige Weg auf KDE Wayland.

### Phase 3 — Installer & Service 🟡 (Komplett-Umbau 2026-07-11, CachyOS-Run offen)

**Ziel**: Ein Bash-Skript, distroübergreifend, robust, klare Fehlermeldungen. Auf vier frischen VMs (Arch, Ubuntu, Fedora, openSUSE) macht `curl ... | bash` jeweils eine funktionierende Sonnenschein-Instanz.

**Stand 2026-07-11**: Komplett-Umbau (siehe Session 2026-07-11 oben): `/opt/sonnenschein`-Prefix, manifest-basierte restlose Deinstallation, Paket-Tracking, `doctor.sh` mit `--repair`, kein Default-setcap mehr (§9.22), persistenter Source-Checkout für Updates. shellcheck-sauber. **Realer Run auf CachyOS = Maintainer-Test.**

**Geplante Struktur** (aus Original-Plan):
```
installer/
├── install.sh              # Entry, Distro-Detect, Sudo-Check, UI-Mode-Wahl
├── lib/
│   ├── distro.sh           # detect_distro() → arch | fedora | debian | ubuntu | opensuse
│   ├── gpu.sh              # detect_gpu() → nvidia | amd | intel | hybrid
│   ├── compositor.sh       # detect_compositor() → kwin | mutter | sway | hyprland | xorg
│   ├── packages.sh         # install_packages() — distro-spezifisch
│   ├── service.sh          # systemd-Unit-Setup (--user mode)
│   ├── permissions.sh      # udev-Rules, Group-Memberships, capabilities (cap_sys_admin für KMS)
│   └── ui.sh               # whiptail/dialog frontend, Fortschrittsbalken
├── packages/
│   ├── arch.list
│   ├── debian.list
│   ├── fedora.list
│   └── opensuse.list
├── post-install.sh         # ersten Pairing-PIN ausgeben, WebUI-URL
├── update.sh               # GitHub-Releases-API, Versionsvergleich, tarball-download, symlink-switch
└── uninstall.sh            # alles spurlos entfernen
```

**Robustheits-Pflichten**:
- Jeder Schritt idempotent
- Jeder Schritt in `set -euo pipefail` mit eigenem trap-Handler
- Logfile: `/var/log/sonnenschein-install.log`
- Jede Fehlermeldung: Kontext + Vorschlag zur manuellen Korrektur

**Service-Layout**:
- `sonnenschein-server.service` (system oder user) — der Streaming-Server. Braucht `/dev/uinput`, `/dev/dri/*`, ggf. `cap_sys_admin` für KMS-Capture.
- Bei Wayland-Compositor-Auto-Match-Virtual-Display: zwingend `systemctl --user`-Modus (DBus-Session-Bus + WAYLAND_DISPLAY).

**Distro-spezifische Pakete**:
- Arch/CachyOS: `nvidia-open-dkms nvidia-utils libva-nvidia-driver` (NVIDIA), `vulkan-radeon` (AMD), `vulkan-intel intel-media-driver` (Intel) — siehe `docs/building.md`.
- Fedora: `akmod-nvidia xorg-x11-drv-nvidia-cuda` (NVIDIA über RPMFusion).
- Ubuntu/Debian: `nvidia-driver-580-open libnvidia-encode-580` (NVIDIA über HWE/PPA).

### Phase 4 — HDR & AV1 ⏸

**Ziel**: HDR10 funktioniert auf KDE Plasma 6 Wayland mit AMD-RDNA3 + NVIDIA-Driver-580+. AV1 wird priorisiert, wenn Client+GPU es können.

**Sub-Tasks**:
- Capability-Negotiation aus Apollo (`nvhttp.cpp:960-989`) übernehmen — die `serverinfo`-Flags (`SCM_HEVC_MAIN10`, `SCM_AV1_MAIN10`).
- VAAPI HDR-Encoder-Pfad (Sunshine ist hier nur Stub — `video.cpp:492-512`).
- NVENC HDR-Encoder-Pfad (Apollos `nvenc/nvenc_utils.cpp:53-90` adaptieren — `nvenc_colorspace_from_sunshine_colorspace()`).
- Vulkan Video Encode (experimental).
- Compositor-Side: HDR-Output-Aktivierung pro Stream (KWin `output.NAME.hdr.true`, Mutter wenn protokoll-stabil).
- Driver-Versionsprüfung im Installer + Warnung bei NVIDIA <580.
- AV1-Preference-Schalter in WebUI.

### Phase 5 — WebUI v1 (PrimeVue 4) 🟡 (Fundament gebaut 2026-05-28, Migration + Browser-Test offen)

**Ziel**: Modernes, gamer-taugliches Frontend. Ein Nutzer ohne Vorwissen kann nach Installer-Run die WebUI öffnen, in <5 min ein erstes Pairing durchführen, ein Spiel streamen — ohne Terminal anzufassen.

**Stack**:
- Vue 3 + Vite (bleibt aus Apollo)
- **PrimeVue 4** (Wechsel von Apollos Bootstrap)
- vue-i18n für DE + EN
- Dunkelmodus-Default + Hell + System-Default-Folger
- Responsive bis 360 px Mobile

**Komponenten**:
- Login-Screen (Browser-Pass-Save-fähig)
- Setup-Wizard: Pairing-PIN, GPU-Bestätigung, Compositor-Test, erstes Spiel hinzufügen
- Dashboard: Live-GPU-Auslastung, aktuelle Streams, Display-Zustand
- Apps-Verwaltung (wie Sunshine + Per-App-Gamescope-Toggle + Per-App-HDR-Override)
- **Virtual-Display-Tab**: Backend-Wahl (auto / kwin_wayland / mutter / ...), GPU-Pin (auto / 0000:01:00.0), Test-Knopf
- Geräte-Verwaltung (gepairte Clients, Permissions)
- Diagnose-Tab: Compositor, GPU, Treiber, Kernel, Mesa, HDR-Capability
- Live-Log-Tab (journald-Stream)
- Update-Manager
- **Crash-Reporter**: anonymisierter Bundle, "An GitHub melden"-Knopf öffnet pre-filled Issue-Form

### Phase 6 — Update-System ⏸

**Sub-Tasks**:
- `update.sh` (CLI): GitHub-Releases-API + Versionsvergleich + Tarball-Download + Symlink-Switch + Service-Restart.
- WebUI-Update-Knopf.
- Branch-Wahl `main` vs `dev`. Wenn `main` neuer als `dev` → automatisch zu `main`.
- Migration-Framework: pro Major-Version Up-Script + Backups vor jedem Lauf, Rollback-Befehl.
- Auto-Update als Service-Option (timer-basiert) oder On-Demand.

### Phase 7 — Tests, Performance, Doku ⏸

**Erfolgskriterium**: ≥4 Distros × 3 GPUs × 2 Compositoren in CI-Smoketests, <30 ms Latenz LAN, vollständige Doku.

**Sub-Tasks**:
- CI-Smoketests in qemu/podman pro Distro.
- Latenz-Benchmark-Suite (Frame-Trace, ENet-Probe).
- mkdocs-material Dokumentation (Sunshines Setup neu strukturiert).
- Optional: Setup-Videos.
- Troubleshooting-Knowledge-Base.

### Phase 8 — 1.0 Release ⏸

**Sub-Tasks**:
- AUR `sonnenschein-bin` und `sonnenschein-git`.
- COPR (Fedora).
- PPA (Ubuntu).
- Flatpak (flathub-Submission).
- AppImage als Fallback.
- Release-Announcement (r/linux_gaming, r/Steamdeck).

---

## 7. Modul-Landkarte (Wo lebt was)

```
sonnenschein/
├── CLAUDE.md                          # Pflichtlektüre für Claude-Code-Sessions
├── README.md                          # Public-Facing, DE/EN
├── LICENSE / NOTICE                   # GPL-3.0 + Attribution
├── CMakeLists.txt                     # project(Apollo) — Phase 1.6 wird zu Sonnenschein
├── package.json                       # name "sonnenschein", Vue+Vite Deps
├── .github/                           # Community-Files + CI
│   ├── CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md
│   ├── PULL_REQUEST_TEMPLATE.md
│   ├── ISSUE_TEMPLATE/{bug,feature,crash,config}.yml
│   ├── workflows/{lint,build-linux}.yml
│   └── copilot-instructions.md
├── cmake/                             # Apollo-CMake-System (modular)
│   ├── compile_definitions/linux.cmake     # ⭐ Sonnenschein-Patches: virtual_display sources, libva, CUDA-/opt/cuda detection, Boost-pre-flight via Boost_Sunshine.cmake
│   └── dependencies/Boost_Sunshine.cmake   # ⭐ Sonnenschein-Patches: pre-flight check + FetchContent fallback
├── docs/
│   ├── ROADMAP.md                     # 8-Phasen Vision
│   ├── STATUS.md                      # ⭐ DIESE DATEI: lebendiger Stand
│   ├── SESSION_PROMPT.md              # ⭐ Vorlage für neue Claude-Sessions
│   ├── building.md                    # Build pro Distro
│   └── (Apollo-Docs: api, configuration, getting_started, etc.)
├── src/                               # C++ Core (Apollo-Fork)
│   ├── platform/
│   │   ├── common.h
│   │   ├── linux/
│   │   │   ├── audio.cpp              # PulseAudio
│   │   │   ├── kmsgrab.cpp            # KMS direct capture
│   │   │   ├── wayland.cpp, wlgrab.cpp # wlroots screencopy
│   │   │   ├── x11grab.cpp
│   │   │   ├── vaapi.cpp              # VAAPI encoder
│   │   │   ├── input/inputtino_*      # uinput-basiert
│   │   │   ├── publish.cpp, misc.cpp, graphics.cpp
│   │   │   └── ⭐ virtual_display/    # ⭐ SONNENSCHEINS NEUE EBENE
│   │   │       ├── types.h, interface.h
│   │   │       ├── detection.h+cpp
│   │   │       ├── factory.h+cpp
│   │   │       ├── subprocess.h+cpp
│   │   │       ├── README.md
│   │   │       └── backends/
│   │   │           ├── all.h
│   │   │           ├── kwin_wayland.cpp     # ✅ implementiert (Phase 2B)
│   │   │           ├── mutter_headless.cpp  # ⏸ Stub
│   │   │           ├── wlroots_headless.cpp # ⏸ Stub
│   │   │           ├── xorg_nvidia.cpp      # ⏸ Stub
│   │   │           ├── xorg_dummy.cpp       # ⏸ Stub
│   │   │           ├── amdgpu_param.cpp     # ⏸ Stub
│   │   │           └── evdi.cpp             # ⏸ Stub
│   │   ├── windows/                   # Apollo-Erbe — wir bauen das nicht
│   │   └── macos/                     # Apollo-Erbe — wir bauen das nicht
│   ├── process.cpp / .h               # ⭐ Phase 2C: #elif __linux__ neben #ifdef _WIN32
│   ├── nvhttp.cpp                     # NvHTTP Pairing Server
│   ├── rtsp.cpp                       # RTSP Stream-Setup, dynamicRangeMode
│   ├── stream.cpp                     # ENet / RTP
│   ├── video.cpp / .h                 # Encoder-Pipeline (HDR-Stubs aktuell)
│   ├── nvenc/                         # NVENC-Wrapper (Windows-zentriert, Linux via CUDA-bridge)
│   ├── confighttp.cpp                 # WebUI-API-Handler
│   ├── display_device.cpp             # Apollo-Display-Layer (libdisplaydevice-Wrapper, Linux-stub)
│   ├── audio.cpp, input.cpp, ...
│   └── main.cpp
├── src_assets/
│   ├── common/assets/web/             # WebUI Quellen (Vue 3 + EJS-Preprocess via Vite)
│   │   └── (Apollos Bootstrap-UI — Phase 5 → PrimeVue 4)
│   ├── linux/misc/                    # 60-sunshine.rules (udev), 60-sunshine.conf (sysctl)
│   ├── linux/assets/                  # apps.json, OpenGL-Shaders
│   ├── windows/                       # Apollo-Erbe
│   └── macos/                         # Apollo-Erbe
├── packaging/
│   ├── linux/                         # AppImage / Arch / Fedora / Flatpak Manifeste
│   └── (sunshine.rb)                  # Homebrew (irrelevant für uns)
├── scripts/
│   └── linux_build.sh                 # Apollos Build-Script (wird durch Phase-3-Installer abgelöst)
├── tests/                             # gtest, BUILD_TESTS=OFF default
├── third-party/                       # Submodules + glad/nvfbc/sudovda vendoriert
└── tools/                             # Apollo-Hilfstools (großteils Windows)
```

**Stern-Markierungen** (⭐) sind die Sonnenschein-spezifischen Files. Alles andere ist Apollo-geerbt und sollte beim Sunshine-Upstream-Sync (Phase 7+) weitgehend übernehmbar sein.

---

## 8. Submodules

17 direkte + nested. `git submodule update --init --recursive` ist Pflicht nach `git clone` und nach jedem `git pull` mit Submodul-Änderungen.

| Pfad | Repo | Branch | Pin |
|---|---|---|---|
| `packaging/linux/flatpak/deps/flatpak-builder-tools` | flatpak/flatpak-builder-tools | master | floating |
| `packaging/linux/flatpak/deps/shared-modules` | flathub/shared-modules | master | floating |
| `third-party/build-deps` | LizardByte/build-deps | dist | floating, **~1.1 GB** (Pre-built FFmpeg + Boost + x264 + x265 + SvtAv1Enc + cbs + hdr10plus für 9 Plattformen — wir nutzen nur `Linux-x86_64`) |
| `third-party/doxyconfig` | LizardByte/doxyconfig | master | floating |
| `third-party/googletest` | google/googletest | main | floating |
| `third-party/inputtino` | games-on-whales/inputtino | stable | floating |
| `third-party/libdisplaydevice` | LizardByte/libdisplaydevice | master | floating, **Linux noch als Stub markiert** |
| `third-party/nanors` | sleepybishop/nanors | master | floating |
| `third-party/nv-codec-headers` | FFmpeg/nv-codec-headers | sdk/12.0 | floating |
| `third-party/nvapi-open-source-sdk` | LizardByte/nvapi-open-source-sdk | sdk | floating |
| `third-party/Simple-Web-Server` | ClassicOldSong/Simple-Web-Server | master | floating, Apollo-Fork |
| `third-party/TPCircularBuffer` | michaeltyson/TPCircularBuffer | master | floating, macOS-only |
| `third-party/tray` | LizardByte/tray | master | **gepinnt auf `7936cb35`** (letzter Pre-Qt-Commit) — siehe Bekannte Probleme |
| `third-party/ViGEmClient` | LizardByte/Virtual-Gamepad-Emulation-Client | master | floating, Windows-only |
| `third-party/wayland-protocols` | wayland/wayland-protocols (gitlab.fdo) | main | floating |
| `third-party/wlr-protocols` | wlroots/wlr-protocols (gitlab.fdo) | master | floating |
| `third-party/moonlight-common-c` | ClassicOldSong/moonlight-common-c | master | floating, Apollo-Fork. Hat nested submodule `enet` |
| `third-party/moonlight-common-c/enet` | cgutman/enet | (nested) | floating |

**Vendoriert (nicht-Submodule, direkt im Repo)**:
- `third-party/glad/` — OpenGL-Loader
- `third-party/nvfbc/` — NVIDIA NvFBC Headers
- `third-party/sudovda/` — Apollos Windows-VDisplay-Treiber-Header (irrelevant für uns aber drin)

---

## 9. Bekannte Probleme & Workarounds

### 9.1 Ubuntu 24.04 libva 2.20 fehlt `vaMapBuffer2`

**Symptom**: Linker-Fehler `undefined reference to 'vaMapBuffer2'` aus `hwcontext_vaapi.c` (build-deps' FFmpeg).

**Ursache**: Ubuntu 24.04 Stock libva ist 2.20 (API 1.20). build-deps' FFmpeg wurde gegen libva ≥2.22 (API 1.21+) gebaut.

**Workaround (WSL)**: libva 2.22 from source bauen (Anleitung in `docs/building.md`):
```bash
DEBIAN_FRONTEND=noninteractive apt-get install -y meson python3-mako libxcb-dri3-dev libxcb-present-dev libxcb-sync-dev libxcb-xfixes0-dev libx11-xcb-dev
git clone --depth 1 -b 2.22.0 https://github.com/intel/libva.git /tmp/libva
cd /tmp/libva
meson setup build --prefix=/usr --libdir=lib/x86_64-linux-gnu --buildtype=release -Dwith_x11=yes -Dwith_wayland=yes -Dwith_glx=yes
ninja -C build install
```

**Auf realen Distros nicht nötig**: CachyOS hat libva 2.23, Fedora 41+ hat libva 2.22+, Arch hat libva 2.23. Nur WSL2 Ubuntu 24.04 betroffen.

### 9.2 Node 18 zu alt für Vite 6

**Symptom**: `crypto.hash is not a function` beim `npm run build`.

**Ursache**: Apollo's `package.json` will Node ≥20.19 (Vite 6 + @vitejs/plugin-vue 6).

**Workaround**: Node 20 via NodeSource installieren:
```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
apt-get install -y nodejs
```

### 9.3 Tray-Submodule nach Qt-Migration nicht mehr GTK-kompatibel

**Symptom**: `tray_linux.cpp:16:10: fatal error: QAction: No such file or directory`.

**Ursache**: LizardByte/tray master ist mit Commit `d05166f` auf Qt umgezogen (`tray_linux.cpp`). Apollo's CMake erwartet aber `tray_linux.c` (GTK + AppIndicator).

**Workaround**: tray-Submodule auf den letzten Pre-Qt-Commit `7936cb35` gepinnt (siehe `.gitmodules` bzw. das Submodule-Pointer in `third-party/tray`). Phase-1.3-Commit `a95f2ee` hat das fixiert.

**Zukunft**: irgendwann müssen wir zur Qt-tray umsteigen oder `libnotify` direkt nutzen (was wir eh schon tun). System-Tray ist nicht kritisch für Sonnenschein.

### 9.4 CachyOS Boost 1.91 partial install

**Symptom**: `find_package(Boost ... COMPONENTS system REQUIRED)` schlägt fehl, obwohl `BoostConfig.cmake` da ist.

**Ursache**: CachyOS' boost-Paket installiert das umbrella `BoostConfig.cmake` aber nicht alle per-component `boost_<comp>-<ver>/`-Verzeichnisse. `boost_system-1.91.0/` fehlt.

**Workaround**: `cmake/dependencies/Boost_Sunshine.cmake` macht jetzt einen Pre-Flight-Check (`file(GLOB)` in `/usr/lib/cmake/boost_<comp>-*`) und fällt komplett auf FetchContent zurück, wenn auch nur eine Component fehlt. Verhindert die ALIAS-Target-Kollision, die mein erster Fix-Versuch hatte. Commit: `b6f263e`.

### 9.5 Arch/CachyOS CUDA-Pfad

**Symptom**: `Looking for a CUDA compiler - NOTFOUND` obwohl CUDA installiert ist.

**Ursache**: Arch packt nvcc nach `/opt/cuda/bin/nvcc`, nicht in PATH und nicht in CMakes Default-Suchpfad.

**Workaround**: `cmake/compile_definitions/linux.cmake` hat jetzt vor `check_language(CUDA)` einen `find_program(_sns_nvcc nvcc PATHS /opt/cuda/bin /usr/local/cuda/bin NO_DEFAULT_PATH)` und primt damit `CMAKE_CUDA_COMPILER`. Commit: `b6f263e`.

### 9.9 CUDA 13.2 inkompatibel mit GCC 16+

**Symptom**: `check_language(CUDA)` / `enable_language(CUDA)` crasht cmake mit 100+ Fehlern in `/usr/include/c++/16.1.1/type_traits`: `identifier "char8_t" is undefined`, `identifier "requires" is undefined`, etc.

**Ursache**: CUDA 13.2's `cudafe++` (der CUDA-Frontend-Compiler) kann die C++20-Features in GCC 16's libstdc++ Headers nicht parsen. CUDA 13.x unterstützt maximal GCC 15.

**Workaround**: `cmake/compile_definitions/linux.cmake` prüft jetzt `CMAKE_CXX_COMPILER_VERSION >= 16.0` vor `check_language(CUDA)`. Wenn GCC zu neu → CUDA wird automatisch übersprungen mit klarer Warnung. NVENC-Encoding (das eigentliche Feature) ist nicht betroffen — es nutzt die CUDA Runtime-API, nicht Device-Kompilierung. Commit: `5e1b1cf`.

**Zukunft**: Sobald NVIDIA CUDA 14+ mit GCC-16-Support released, die Versionsprüfung anpassen. Alternativ: `-DCMAKE_CUDA_HOST_COMPILER=/usr/bin/gcc-15` wenn user GCC 15 parallel installiert hat.

### 9.6 WSL2 `/tmp` wird beim Restart geleert

**Symptom**: Build-Verzeichnis ist nach WSL-Neustart weg, cmake muss komplett neu konfigurieren.

**Workaround**: Build-Dir auf `/root/snsbuild` (oder `/home/<user>/snsbuild`) legen, nicht `/tmp/snsbuild`.

### 9.7 kscreen-doctor SIGABRT in TTY/SSH

**Symptom**: `qt.qpa.xcb: could not connect to display` + SIGABRT, wenn `kscreen-doctor` aus SSH oder reinem TTY ausgeführt wird.

**Ursache**: kscreen-doctor ist Qt-basiert, will Wayland-Socket oder X11-Display.

**Workaround**: aus einer Konsole-App **innerhalb der laufenden Plasma-Wayland-Sitzung** ausführen (DBus-Session-Bus + WAYLAND_DISPLAY werden geerbt). Nicht aus SSH oder reinem TTY.

**Konsequenz für Sonnenschein**: Phase-3-Installer muss systemd `--user`-Modus als Default empfehlen (NICHT system-mode), damit Sonnenschein die DBus-Session erbt.

### 9.8 CMake `project(Apollo)` Branding nicht umbenannt

**Symptom**: Logs sagen "Apollo", Binary heißt `sunshine`, nicht `sonnenschein`. Verwirrend.

**Status**: ✅ **ERLEDIGT (Session 2026-05-28)**. Voller user-facing Rebrand inkl. `sonnenschein`-Binary + `sunshine`-Kompat-Symlink — Details siehe „Session 2026-05-28" unter dem TL;DR. On-Disk-Pfade (`~/.config/sunshine`, Logs) bewusst unverändert für Backward-Compat.

### 9.10 Binary braucht `cap_sys_admin` für KMS-Capture

**Symptom**: `Failed to gain CAP_SYS_ADMIN`, `Couldn't get handle for DRM Framebuffer`, `Unable to initialize capture method`, alle Encoder scheitern → `Fatal: Unable to find display or encoder during startup.`

**Ursache**: KMS-Capture (DRM-Framebuffer-Zugriff) benötigt die Linux-Capability `CAP_SYS_ADMIN`. Ohne sie kann Sonnenschein den Bildschirminhalt nicht abgreifen → kein Bild zum Encoden → alle Encoder scheitern.

**Workaround (manuell, NUR für KMS-Capture)**:
```fish
sudo setcap cap_sys_admin+p (readlink -f ~/sonnenschein/build/sunshine)
```
Hinweis: `setcap` kann nicht auf Symlinks arbeiten, daher `readlink -f` um den echten Pfad aufzulösen.

**⚠️ ACHTUNG: setcap + PipeWire-Capture sind INKOMPATIBEL!**
`xdg-desktop-portal` verweigert den Zugriff wenn das Binary Capabilities hat (`Unable to open /proc/PID/root`). Für PipeWire-Capture:
```fish
sudo setcap -r (readlink -f ~/sonnenschein/build/sunshine)  # Capabilities ENTFERNEN
```

**Zukunft**: Phase-3-Installer wird das automatisch via udev-Rules + systemd-Service-Config (`AmbientCapabilities=CAP_SYS_ADMIN`) setzen. PipeWire-Capture braucht kein cap_sys_admin.

### 9.11 WSL root + Windows-Checkout: Git `dubious ownership`

**Symptom**: Beim WSL-Build aus `/root/snsbuild` gegen den Source unter `/mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein` läuft der CMake-Build durch, aber Git-Metadaten-Abfragen können warnen oder fehlschlagen (`fatal: detected dubious ownership in repository ...`, teils als nicht-fatales `ERROR: Got git error while fetching tags: 128` im CMake-Output).

**Ursache**: WSL läuft als `root`, der Checkout liegt aber auf dem Windows-Mount und gehört aus Git-Sicht einem anderen Owner.

**Workaround (einmalig in WSL als root)**:
```bash
git config --global --add safe.directory /mnt/c/Users/cooki/Documents/ClaudeCode/sonnenschein
```

**Status**: Nicht build-blockierend. WSL-Build am 2026-05-10 war trotz Warnung erfolgreich (`sunshine-0.0.0` + Symlink `sunshine` in `/root/snsbuild`).

### 9.12 PipeWire Portal: Crash nach Auswahl des virtuellen Bildschirms

**Symptom**: Auf CachyOS/Plasma öffnet sich erstmals der KDE-Screen-Record-Portal-Dialog. Nach Auswahl des virtuellen Bildschirms loggt Sonnenschein `Portal: stream node_id=127` und bricht direkt danach mit GLib ab:
```text
GLib-ERROR **: g_variant_new: expected array GVariantBuilder but the built value has type '(null)'
terminated by signal SIGABRT
```

**Ursache**: `OpenPipeWireRemote` baut das leere `a{sv}`-Options-Argument falsch. Der Code übergibt `g_variant_builder_end(...)` an eine `g_variant_new("(oa{sv})", ...)`-Formatstelle, die einen `GVariantBuilder*` erwartet.

**Status**: Reproduziert durch Maintainer-Log am 2026-05-10. Gefixt in `6d6433f`: `OpenPipeWireRemote` übergibt jetzt wie `CreateSession`/`SelectSources`/`Start` einen echten `GVariantBuilder*` an `g_variant_new("(oa{sv})", ...)`. CachyOS-Test mit `5fe1ea6` bestätigt: `Portal: PipeWire fd=68 node_id=140`, PipeWire `streaming`, kein GLib-`SIGABRT`.

### 9.13 PipeWire negotiated 1920x1080 statt SteamDeck 1280x800@90

**Symptom**: Erster echter End-to-End-Stream funktioniert, aber Auflösung/Aspect Ratio/Bildwiederholrate passen nicht zum SteamDeck. Log:
```text
Display mode for client [sd] requested to [1280x800x90]
Sonnenschein vdisplay (kwin): created 'Sonnenschein-00E8F1E1' 1280x800@90, Hz HDR10
Portal: PipeWire fd=68 node_id=140
PipeWire: format negotiated 1920x1080 fmt=8
PipeWire display initialized: 1920x1080
```

**Ursache (validiert)**: Anfangs war `pw_capture_t::init()` noch auf `1920x1080` voreingestellt; das wurde in `76d03a4` korrigiert. Die danach verbliebene Ursache ist die KDE-Portal-Quelle selbst: Im Dialog ausgewähltes "Virtuelles Display" kommt als XDG-`VIRTUAL`-Source mit `logical size=1920x1080`, nicht als der von Sonnenschein erzeugte KWin-Output `Sonnenschein-...`.

**Status**: `76d03a4` wirkt teilweise: `pwgrab.cpp` loggt `PipeWire: requesting format 1280x800@90`. CachyOS-Test mit `12a17da` zeigt aber weiterhin `PipeWire: format negotiated 1920x1080 fmt=8` und die neue Warnung `PipeWire: negotiated size differs from client request 1280x800 -> 1920x1080`. Damit ignoriert KDE/Portal den Formatwunsch oder die im Dialog gewählte Quelle ist nicht der KWin-Output `Sonnenschein-00E8F1E1`.

**Recherche 2026-05-10**:
- Offizielle XDG-Portal-Doku (`flatpak.github.io/xdg-desktop-portal/.../ScreenCast.html`): `ScreenCast.SelectSources` kennt `types` mit `1=MONITOR`, `2=WINDOW`, `4=VIRTUAL`; `cursor_mode` ist standardmäßig `Hidden`, `2=Embedded` bettet den Cursor in die Videoframes ein.
- KDE Bug 485850 (`bugs.kde.org/show_bug.cgi?id=485850`): `xdg-desktop-portal-kde` Virtual Screens sind/haben historisch `1920x1080` hardcoded (`startStreamingVirtual(..., {1920, 1080}, ...)`).
- KDE Bug 512620 (`bugs.kde.org/show_bug.cgi?id=512620`): KWin-Screencast-Virtual-Monitor kann nur mit `1920x1080` negotiated werden; andere Auflösungen liefern keine Frames oder frieren ein. Das passt exakt zum Sonnenschein-Log, wenn im Portal die Option "Virtuelles Display" gewählt wird.

**Status**: Diagnose-/Cursor-Patch in `447dc8b`. Portal-Stream-Properties (`source_type`, `size`, `position`, `id`, `mapping_id`) werden geloggt und `cursor_mode=Embedded` wird angefordert, falls KDE diesen Modus anbietet. WSL2-Build gegen `/root/snsbuild` ist grün (`ninja: no work to do` nach erfolgreichem Link, Binary `sunshine-0.0.0` aktualisiert um 16:58). Damit sehen wir im nächsten CachyOS-Test eindeutig, ob der Dialog die XDG-`VIRTUAL`-Quelle statt des existierenden KWin-Monitors auswählt, und der Mauszeiger sollte sichtbar werden, wenn KDE Embedded Cursor unterstützt.

**Validierung `bb3e758` (CachyOS, 2026-05-10 17:06)**:
```text
Portal: available source types=7 cursor modes=7
Portal: requesting embedded cursor mode
Portal: stream source_type=VIRTUAL
Portal: stream logical size=1920x1080
PipeWire: requesting format 1280x800@90
PipeWire: format negotiated 1920x1080 fmt=8
PipeWire: negotiated size differs from client request 1280x800 -> 1920x1080
```
Damit ist bestätigt: der falsche 1920x1080-Pfad kommt von der ausgewählten KDE-XDG-`VIRTUAL`-Quelle, nicht von unserem PipeWire-Formatrequest.

**Fix-Kandidat `4c63d36` (2026-05-10)**:
- `src/platform/linux/pwgrab.cpp` probiert bei benanntem Virtual Display zuerst KWin Direct ScreenCast (`zkde_screencast_unstable_v1`) statt `xdg-desktop-portal`.
- Der Code enumeriert `wl_output`-Namen, sucht exakt `Sonnenschein-...`, fordert Embedded Cursor an und verbindet den daraus gelieferten PipeWire-Node über den lokalen PipeWire-Core.
- Wenn der Ziel-Output nicht gefunden wird oder KWin den Zugriff verweigert, fällt Sonnenschein **nicht** mehr auf die KDE-XDG-`VIRTUAL`-Quelle zurück, weil dieser Pfad validiert 1920x1080 erzwingt.

**Weiterer Fix (2026-05-10 18:40)**:
- CachyOS-Test mit `4c63d36` scheiterte, da `Sonnenschein-00E8F1E1` nicht als Output-Name gefunden wurde. KWin verwendet intern vermutlich `Virtual-1` als `wl_output::name`. 
- `pwgrab.cpp` prüft nun zusätzlich `wl_output::description` und nutzt einen Fallback auf `Virtual-*`, falls kein exakter Name gefunden wird.
- Behebung eines C++-Formatierungsfehlers, bei dem `kwin_wayland.cpp` ungültige Strings ins Log schrieb (`@third-party\build-deps...`).

**Weiterer Fix (2026-05-10 19:15)**:
- Ein weiterer CachyOS-Test zeigte, dass der Output noch immer nicht in der `wl_output`-Liste auftauchte. Grund: KWin erstellt das virtuelle Display asynchron, und unsere initialen `wl_display_roundtrip`-Aufrufe in `init()` fanden statt, *bevor* KWin das neue Display broadcasten konnte.
- `pwgrab.cpp` wurde angepasst: Wenn `start()` den Output im ersten Durchlauf nicht findet, werden nun gezielt weitere `wl_display_roundtrip`-Aufrufe durchgeführt, um die Hotplug-Events aus der Wayland-Queue abzurufen, bevor der zweite Such-Durchlauf startet.

**Native Virtual Output Stream Migration (2026-05-10 19:40)**:
- Auch der Roundtrip-Hotfix schlug fehl (`timeout waiting for stream`).
- Analyse des KWin-Wayland-Protokolls (`zkde_screencast_unstable_v1`) zeigte, dass KWin speziell für diesen Anwendungsfall die Methode `stream_virtual_output` (bzw. `_with_description`) anbietet.
- Anstatt zu versuchen, einen von `kscreen-doctor` erstellten Output aus der allgemeinen Wayland-Registry abzufangen, ruft `pwgrab.cpp` nun direkt `zkde_screencast_unstable_v1_stream_virtual_output` auf, falls das Display nicht gefunden wurde. 
- Das umgeht alle Sichtbarkeits- und Asynchronitätsprobleme, da KWin explizit angewiesen wird, für diesen angeforderten Stream ein virtuelles Display in der passenden Auflösung zu liefern.

**Erfolgreicher E2E Test (2026-05-10 19:40)**:
- Der CachyOS Test mit `stream_virtual_output` war ein **Erfolg**! Das Log zeigt: `KWin direct capture: streaming output 'Sonnenschein-00E8F1E1' 1280x800 node_id=73`.
- PipeWire negotiated erfolgreich `1280x800 fmt=8`.
- Die folgenden Probleme verbleiben:
  - **Bildwiederholrate (60Hz statt 90Hz)**: Ein Formatierungsfehler beim Loggen (`@third-party\build-deps...`) deutet darauf hin, dass die Refresh-Rate in `pwgrab.cpp` nicht korrekt übergeben wird. **(Gefixt am 2026-05-10: C++ Typ-Cast gefixt und dynamisches `kscreen-doctor mode set` nach Stream-Start eingebaut. Das Log-Korruptions-Problem mit snprintf wurde entfernt und durch robustere String-Casts ersetzt.)**
  - **Physische Monitore deaktivieren**: Der User fordert, dass bei aktiven Virtual Displays alle physischen Monitore ausgeschaltet werden (Headless-Modus Priorität 1). **(Gefixt am 2026-05-10: `kwin_wayland.cpp` nutzt nun `kscreen-doctor` um physische Monitore abzuschalten und später wiederherzustellen)**.
  - **HDR Option**: HDR wird auf dem Stream noch nicht angeboten.

**E2E Refresh Rate & Headless Fix (2026-05-10 20:45)**:
- Das hartnäckige Problem der korrupten Refresh-Rate-Strings (`@third-party\build-deps...`) wurde identifiziert: In der Windows/WSL Build-Umgebung mit den speziellen `build-deps` Submodules führt das Zeichen `@` in C++ String-Literalen unter bestimmten Bedingungen zu einer Fehlinterpretation durch den Compiler oder das Build-System, wodurch der String durch einen internen Pfad (`sys390.h` aus Boost Predef) ersetzt wird.
- Lösung: Das `@` Zeichen wird nun nicht mehr als Literal im Source-Code verwendet, sondern zur Laufzeit als Zeichen (`0x40`) eingefügt.
- Zudem wurde eine Verzögerung von 500ms vor dem `kscreen-doctor mode set` eingebaut, um KWin Zeit zu geben, das neue virtuelle Display in seinem Konfigurationssystem zu registrieren, bevor die Refresh-Rate geändert wird.
- Headless-Mode (Physische Monitore aus) ist voll funktionsfähig.

### 9.14 PipeWire-Virtual-Display: Touch/Maus fehlt

**Symptom**: Im funktionierenden Virtual-Display-Stream auf SteamDeck sind keine Touch-Eingaben möglich und der Mauszeiger wird nicht angezeigt.

**Ursache (offen)**:
- Cursor: `pwgrab.cpp` setzt derzeit keinen Cursor-Metadatenpfad und ruft `push_captured_image_cb(..., true)` nur mit Frameinhalt auf. Portal/ScreenCast kann Cursor je nach `cursor_mode` im Stream einbetten oder als Metadaten liefern; wir setzen aktuell keine Cursor-Option.
- Touch/Input: Sonnenschein erzeugt zwar ein virtuelles Gamepad (`Gamepad 0 will be Xbox One controller`), aber Touch-/Maus-Koordinaten müssen auf `display->env_width/env_height` und den Virtual-Display-Output abgebildet werden. Bei PipeWire `1920x1080` vs. Client `1280x800` ist das Mapping wahrscheinlich falsch oder inaktiv.

**Status**: Teilweise erledigt. CachyOS-Test mit `bb3e758` bestätigt: Ton und Eingaben funktionieren. Cursor wurde über `cursor_mode=Embedded` angefordert; falls der Cursor weiter fehlt, bleibt das ein separater Cursor-Compositing-Bug. Touch/Input ist nicht mehr Hauptblocker.

### 9.16 SteamDeck Refresh bleibt nach KWin Direct bei 60 Hz

**Symptom**: Steam/Moonlight auf dem SteamDeck zeigt eine niedrigere bzw. falsche Bildwiederholrate im Stream an, obwohl das Deck-Display mehr kann und der Client `1280x800x90` anfragt.

**Ursache (aktuelle Arbeitshypothese)**: Der alte `source_type=VIRTUAL`/`1920x1080`-Pfad ist durch KWin Direct ScreenCast gelöst. `stream_virtual_output` erzeugt den virtuellen Output aber initial mit 60 Hz. Der nachträgliche `kscreen-doctor output.Sonnenschein-...mode.1280x800@90`-Befehl trifft wahrscheinlich nicht zuverlässig den tatsächlichen KScreen-Namen, weil KWin/KScreen den Output intern häufig als `Virtual-*` registriert.

**Status**: `6504268` reicht nicht. Maintainer-Test am 2026-05-13: Stream bleibt bei 60 Hz. Fix-Kandidat `723537a` ersetzt den reinen `kscreen-doctor`-Ansatz durch KWin Output-Management-v2:
- `cmake/compile_definitions/linux.cmake` generiert zusätzlich `kde-output-device-v2` und `kde-output-management-v2`.
- `pwgrab.cpp` bindet KWins Output-Device-Registry, löst den echten Virtual Output auf, legt bei Bedarf einen Custom Mode `WxH@Hz` an, wendet den Mode atomar an und aktiviert HDR/WCG, wenn der Output die Capability meldet.
- Nach der Anwendung wird der aktive Mode über die Wayland-Events verifiziert; `kscreen-doctor` bleibt nur Fallback.
- PipeWire loggt jetzt die negotiated Framerate. Wenn sie unter dem Client-FPS liegt, taktet der Capture-Loop den Encoder weiter mit der Client-Framerate und wiederholt den letzten Frame, statt auf 60 neue Frames pro Sekunde zu blockieren.
- HDR: `display.is_hdr()` und HDR10-Metadaten sind für den PipeWire/KWin-Direct-Pfad aktiv, sobald der Client `dynamicRange > 0` anfragt. 10-bit/scRGB PipeWire-Input wird noch nicht negotiated, weil der aktuelle Software-Konverter Eingangsframes fest als `AV_PIX_FMT_BGR0` behandelt; das ist der nächste gezielte HDR-Qualitäts-Fix, falls der Client zwar HDR signalisiert, aber Farben nicht korrekt sind.

### 9.17 Regression: KWin Direct fehlt, Stream startet nicht mehr

**Symptom**: CachyOS-Test mit HEAD `ec832c8` am 2026-05-13: Client fordert `1280x800x90`, Virtual Display wird erzeugt, danach bricht Capture ab. Log-Signatur:
- `KWin output management: bound kde_output_management_v2 version 19`
- `KWin direct capture: zkde_screencast_unstable_v1 not available; falling back to portal`
- `KWin direct capture: failed for output 'Sonnenschein-...'`
- `Initial Ping Timeout`

**Ursache**: Der neue Output-Management-Patch behandelte fehlendes `zkde_screencast_unstable_v1` als fatal und blockierte bewusst den Portal-Fallback. Das ist für Plug-and-play falsch: KWin Direct ist bevorzugt, aber nicht garantiert in jedem Registry-/Permission-Zustand verfügbar. Zusätzlich war die dynamisch erzeugte KDE-Permission-Datei nur auf `zkde_screencast_unstable_v1` begrenzt, obwohl der neue Pfad auch `kde_output_management_v2` und `kde_output_device_registry_v2` bindet.

**Fix `edc144e`**:
- `pwgrab.cpp` fällt bei fehlendem oder fehlschlagendem KWin Direct Capture wieder auf xdg-desktop-portal Monitor-Capture zurück, statt `-1` zurückzugeben und damit die Session/den Virtual Output zu zerstören.
- Die Permission-Datei `sonnenschein-kwin-screencast.desktop` schreibt jetzt `X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1,kde_output_management_v2,kde_output_device_registry_v2`.
- Erwartung: Stream startet wieder. Wenn KWin Direct danach weiterhin fehlt, ist das im Log sichtbar und Portal ist ein funktionaler Fallback; Refresh/HDR bleiben dann als KWin-Direct-spezifischer Folgefix offen.

**Rollback-Entscheidung (2026-05-13)**: Der Maintainer hat entschieden, den letzten funktionierenden Stand wiederherzustellen. `501431a` setzt die betroffenen Code-Dateien auf `67a93e3` zurück:
- `src/platform/linux/pwgrab.cpp`
- `cmake/compile_definitions/linux.cmake`

Damit sind der Output-Management-v2-Patch `723537a`, der STATUS-HEAD `ec832c8` und der Fallback-Hotfix `edc144e` inhaltlich aus dem Laufzeitcode entfernt. Diese Regressionskette darf nicht erneut als großer Kombi-Patch für Refresh/HDR eingeführt werden. Nächster 90-Hz-Versuch nur in kleinen, einzeln testbaren Schritten.

**Korrektur (2026-05-13)**: Der Maintainer hat direkt nach `501431a` klargestellt, dass `67a93e3`/`6504268` bereits die defekte Version nach dem letzten guten Stand war. Der tatsächliche Zielstand für den Laufzeitcode ist `bf7d939`/`2d5b81a`, also vor dem KScreen-Resolver. `74c63cf` setzt `pwgrab.cpp` entsprechend zurück.

**Weitere Korrektur (2026-05-13 23:10)**: Der neue Log wurde mit Binary `0.0.0.4d47b5e` erzeugt, also nicht mit dem danach gepushten `74c63cf`/`c451725`. Trotzdem ist der eigentliche Defekt noch im aktuellen Laufzeitcode: der KWin-Direct-Pfad gibt bei fehlendem `zkde_screencast_unstable_v1` `-1` zurück und verhindert damit den Portal-Fallback. `41fa9ba` entfernt diesen fatalen Abbruch, ohne erneut Output-Management, HDR oder Frame-Pacing anzufassen.

### 9.18 60-Hz-Bug — Diagnose + erster (zurückgerollter) Lösungsversuch (Stand 2026-05-14 spät)

> **HINWEIS (Stand 2026-05-14 Nacht)**: Der hier beschriebene v2-Implementierungspfad (`2996b4e` + `806a7ca`) ist auf CachyOS Plasma 6.6.4 fehlgeschlagen und in `d7afb8b` + `ea201f5` **zurückgerollt**. Vollständige Post-Mortem-Analyse + v3-Strategie in **§9.20**. Der untenstehende Text bleibt als historische Diagnose erhalten.

**Symptom**: SteamDeck-OLED (oder beliebiger 90/120 Hz Client) bleibt im Stream bei 60 Hz, obwohl der Stream sonst sauber läuft (Bild + Ton + Eingaben + Headless funktionieren).

#### Root Cause (online verifiziert)

KWin Wayland ignoriert **alle** Modi, die nicht in der nativen Mode-Liste eines Outputs sind. Zitat aus der KDE-Mod-Diskussion (KDE Discuss thread 13642): *"Unfortunately, Kwin (Wayland) does not support running the display in any geometry other than what is supported by a native mode."*

Konkret im Sonnenschein-Pfad:
1. `zkde_screencast_unstable_v1::stream_virtual_output(name, w, h, scale, pointer)` — **kein Refresh-Rate-Argument** (siehe `third-party/plasma-wayland-protocols/src/protocols/zkde-screencast-unstable-v1.xml`).
2. KWin erstellt den Output mit fixem 60-Hz-Default; die Mode-Liste des Outputs enthält **nur** 60 Hz.
3. Jeder spätere `kscreen-doctor output.NAME.mode.1280x800@90` schlägt fehl (silent), weil `1280x800@90` nicht in der Mode-Liste ist.

#### Was `29cd4b6` (mein Versuch vom Vormittag) versucht hat — und warum es nicht greift

`KwinWaylandBackend::create()` ruft `kscreen-doctor add-virtual-output NAME W H` + `kscreen-doctor output.NAME.mode.WxH@HZ` + `kscreen-doctor output.NAME.hdr.true` **vor** dem Stream-Start. Die Befehle exit-en alle mit 0. Aber:
- `kscreen-doctor add-virtual-output` erzeugt einen **rein KScreen-internen Cache-Eintrag**, **keinen echten DRM-Connector** und **keinen wl_output**.
- Der Output erscheint NICHT im Wayland Registry (`pwgrab.cpp::find_target()` sieht nur `HDMI-A-1` und `HDMI-A-2`).
- Folge: `pwgrab.cpp` fällt auf `stream_virtual_output` zurück, das einen *anderen* Output erstellt — der wieder 60-Hz-fixed ist.

CachyOS-Test-Log Auszug (2026-05-14 09:39:14):
```
Sonnenschein vdisplay (kwin): pre-created virtual output 'Sonnenschein-00E8F1E1' 1280x800   ← exit 0, aber wirkungslos
KWin direct capture: found output 'HDMI-A-1' ... 3840x2160
KWin direct capture: found output 'HDMI-A-2' ... 1920x1080
KWin direct capture: target not found yet, doing a display roundtrip...
KWin direct capture: output 'Sonnenschein-00E8F1E1' not found in Wayland registry.
                    Requesting KWin to create a stream_virtual_output directly.   ← ergibt 60-Hz-Default
```

Konsequenz: `29cd4b6` ist auf `41fa9ba` zurückgerollt. Der Aufruf war wirkungslos und produzierte verwirrende Logs, ohne Mehrwert.

#### Echte Lösung — `kde_output_management_v2::set_custom_modes`

KWin MR [!8766](https://invent.kde.org/plasma/kwin/-/merge_requests/8766) (Februar 2026, gelandet in Plasma 6.6 — Maintainer hat 6.6.4, also **ist die API verfügbar**) hat `Capability::CustomModes` für virtual outputs eingeführt. Damit kann ein Wayland-Client per `kde_output_management_v2` synthetische Modi mit beliebigen Refresh-Raten zur Mode-Liste eines Outputs hinzufügen. KWin akzeptiert sie als Native-Modi.

Modifizierte Files in MR !8766:
- `src/backends/drm/drm_virtual_output.cpp`
- `src/backends/drm/drm_virtual_output.h`
- `src/backends/virtual/virtual_output.cpp`

Sequenz für den Client (aus `kde-output-management-v2.xml`):

```
kde_output_management_v2.create_mode_list()             → kde_mode_list_v2
kde_mode_list_v2.set_resolution(W, H)
kde_mode_list_v2.set_refresh_rate(mhz)                  // milliHz, also 90000 für 90 Hz
kde_mode_list_v2.set_reduced_blanking(1)
kde_mode_list_v2.add_mode()
// optional: weitere Modi anhängen
kde_output_management_v2.create_configuration()         → kde_output_configuration_v2
kde_output_configuration_v2.set_custom_modes(device, mode_list)
kde_output_configuration_v2.apply()
// dann: kde_output_configuration_v2.set_mode(device, new_mode) + apply() um zu aktivieren
```

#### Was Codex in `723537a` versucht hat — und warum es kollabierte

Codex hat in `723537a` genau diesen Pfad implementiert (+850 Zeilen in `pwgrab.cpp`). Sein Code-Architektur war **korrekt** — Wayland-Binding für `kde_output_management_v2` + `kde_output_device_registry_v2`, `add_custom_kde_mode()`, `apply_kde_configuration()`, alle Helper-Methoden 1:1 nutzbar.

**Der eigentliche Bug war die Setup-Reihenfolge in `start()`:** Codex rief `apply_output_management_settings()` **dreimal** auf — einmal mit 2500 ms Timeout **VOR** `stream_output()`/`stream_virtual_output()`, einmal mit 2500 ms Timeout direkt nach Stream-Open, und einmal mit 1000 ms Timeout nach `wait_for_stream()`. `wait_for_kde_output()` wartet intern via `dispatch_until` bis der neu erstellte virtual output via `kde_output_device_registry_v2` angekündigt wird. Auf dem CachyOS-Pfad (kein matching wl_output → `stream_virtual_output`-Branch) blockierten diese drei Aufrufe die Stream-Etablierung um **bis zu 5000 ms** — länger als der SteamDeck Initial-Ping-Timeout. Resultat: `Initial Ping Timeout` auf dem Client, `wait_for_stream()` lief in seinen 5 s Timeout, der ganze KWin-Pfad brach ab und der Stream sah aus als wäre `zkde_screencast_unstable_v1` "nicht mehr gebunden" (war es technisch schon, kam nur nicht rechtzeitig zum Zug). Die zwei späteren `apply_*`-Calls liefen dann zwar noch, aber zu spät — der Client hatte schon disconnected.

Maintainer hat `723537a` deshalb in `501431a` + `74c63cf` zurückgerollt. Die korrekte Stelle für `apply_output_management_settings()` ist **NACH** erfolgreichem `wait_for_stream()`, mit nur **einem** Aufruf, an der Stelle wo bisher `apply_refresh_rate()` (das fruchtlose `kscreen-doctor mode-set`) stand. Genau so ist es in `806a7ca` umgesetzt.

#### Schritt-für-Schritt-Anleitung für nächste Session (Phase 4.5 / 60-Hz-Fix v2)

Der konkrete Implementierungsweg, der weder Codex' Falle noch meine `add-virtual-output`-Falle wiederholt:

1. **Wayland-Protocol-Bindings erzeugen lassen**: Stelle sicher dass `cmake/compile_definitions/linux.cmake` `kde-output-management-v2.xml` und `kde-output-device-v2.xml` aus `third-party/plasma-wayland-protocols/src/protocols/` via `wayland-scanner` zu C-Bindings macht. Codex' `723537a` hatte das schon (siehe `git show 723537a -- cmake/compile_definitions/linux.cmake`). Cherry-pick exakt diese CMake-Zeilen.

2. **In `pwgrab.cpp` parallel zu `zkde_screencast` ein zweites Wayland-Binding-Set einbauen**:
   - `kde_output_management_v2` global
   - `kde_output_device_registry_v2` global (liefert die `kde_output_device_v2` Liste)
   - Listener für `kde_output_device_v2` der Name, Modi und Capabilities trackt
   - **Beide Bindings müssen koexistieren**, nicht eines das andere ersetzen — das war Codex' Fehler.

3. **Nach `stream_virtual_output` Aufruf** (pwgrab.cpp Z. 219 ff):
   - `wl_display_roundtrip` damit der neue Output via `kde_output_device_registry_v2` ankommt
   - In der `kde_output_device_v2` Liste den Output mit Name `Sonnenschein-XXXX` finden
   - Prüfen: hat er `KDE_OUTPUT_DEVICE_V2_CAPABILITY_CUSTOM_MODES`? Falls nein, log warning und mit 60 Hz weitermachen.
   - `kde_output_management_v2_create_mode_list()` + `set_resolution/refresh_rate/reduced_blanking/add_mode`
   - `kde_output_management_v2_create_configuration()` + `set_custom_modes()` + `apply()` + warten auf `applied`-Callback
   - Dann zweite Configuration: `set_mode(neuer_mode)` + `apply()` um die neue Mode aktiv zu machen
   - Roundtrip + KWin schaltet den Stream auf die neue Refresh-Rate um

4. **Code-Vorlage**: `git show 723537a -- src/platform/linux/pwgrab.cpp` zeigt die volle Implementierung. Cherry-pick chirurgisch: nur die Klassen `kde_output_t`, `kde_mode_t`, `apply_kde_configuration`, `add_custom_kde_mode`, `wait_for_kde_output` und die Registry-Binding-Logic. Alle Änderungen am `zkde_screencast`-Pfad ignorieren.

5. **Test auf CachyOS**: erwartete neue Log-Zeilen:
   ```
   KWin output management: bound kde_output_management_v2 version 19
   KWin output management: bound kde_output_device_registry_v2 version N
   KWin output management: output 'Sonnenschein-XXXX' caps=0x... custom_modes=yes
   KWin output management: adding custom mode 1280x800@90 to 'Sonnenschein-XXXX'
   KWin output management: applied custom mode list
   KWin output management: activated mode 1280x800@90 on 'Sonnenschein-XXXX'
   ```
   Und im Stream: 90 Hz statt 60 Hz auf dem SteamDeck.

6. **Falls KWin trotzdem 60 Hz liefert (Plasma-Version hat MR !8766 nicht)**: Maintainer-Log poste mir, dann Fallback-Plan: EDID-Firmware-Injection als Boot-Setup (HarryAnkers-Methode), siehe §9.19 unten.

#### §9.19 Fallback-Plan: EDID-Firmware-Injection (Boot-time Setup)

Falls `kde_output_management_v2::set_custom_modes` auf Plasma 6.6.4 nicht funktioniert (z.B. weil MR !8766 nicht in 6.6.4 sondern erst 6.7 ist), Fallback-Methode aus dem HarryAnkers Gist:

1. Custom EDID-Binary generieren mit gewünschten Modi (1280x800@90, 1920x1080@120, 4K@60 etc.) inkl. HDMI VSDBs für NVIDIA-Unlock.
2. EDID-Binary nach `/usr/lib/firmware/edid/sonnenschein.bin` kopieren, in initramfs einbinden (`mkinitcpio` / `dracut`).
3. Kernel-Boot-Parameter setzen: `drm.edid_firmware=DP-2:edid/sonnenschein.bin video=DP-2:e`.
4. Nach Reboot: DRM erstellt einen vollwertigen Output mit den definierten Modi. KWin sieht ihn als echten Wayland-Output mit korrekter Mode-Liste.
5. `stream_output(wl_output*)` (statt `stream_virtual_output`) capturet ihn — Refresh-Rate kommt aus der EDID-Mode-Liste.

Nachteile: 
- Erfordert Boot-Konfiguration (Reboot beim Setup, Modi sind statisch).
- HDR funktioniert auf virtuellen Connectors nicht (NVIDIA-Treiber-Limitation: SCDC-Negotiation nur auf echten Links).
- Pro Setup nur eine fixe Konnektor-Konfiguration; Multi-Client-Multi-Resolution skaliert nicht.

Vorteile: rock-solid, distroübergreifend, funktioniert ohne KDE-spezifische API.

#### Externe Quellen für die Diagnose

- KDE Discuss: [KDE Wayland custom resolution AND refresh rate](https://discuss.kde.org/t/kde-wayland-custom-resolution-and-refresh-rate/13642) — *"Kwin (Wayland) does not support running the display in any geometry other than what is supported by a native mode."*
- KWin MR [!8766](https://invent.kde.org/plasma/kwin/-/merge_requests/8766) — `backends: add support for custom modes in virtual outputs`.
- KDE Protokoll-Spezifikation: [kde_output_management_v2 + kde_mode_list_v2](https://wayland.app/protocols/kde-output-management-v2).
- [HarryAnkers' EDID-Firmware-Injection-Anleitung für Sunshine/Moonlight auf NVIDIA Linux](https://gist.github.com/HarryAnkers/8dbf551d66f00e8156ef4dd2b2b090a0).
- `third-party/plasma-wayland-protocols/src/protocols/kde-output-management-v2.xml` (im Repo).

### 9.15 Portal-Dialog erscheint bei jedem Stream

**Symptom**: Bei jedem Test muss der Maintainer im KDE-Screen-Record-Dialog manuell eine Quelle auswählen.

**Ursache**: `pwgrab.cpp` setzt zwar `persist_mode=2`, speichert den vom Portal nach `Start` gelieferten `restore_token` aber nur im RAM der aktuellen `portal_session_t`. Beim nächsten Sonnenschein-Start ist der Token weg, deshalb fragt KDE erneut.

**Status**: ✅ **GELÖST (Session 2026-07-11, Commit `9be3ba4`)**. Restore-Token wird atomar nach `~/.local/state/sonnenschein/portal-restore-token` persistiert, beim Start geladen und bei stale-Token-Fehler einmal ohne Token retried (Datei wird dann gelöscht). Dialog erscheint nur noch bei der allerersten Portal-Nutzung. CachyOS-Verifikation offen.

### 9.20 60-Hz-Fix v2 (`806a7ca`) — Regression-Post-Mortem (Stand 2026-05-14 Nacht)

> **✅ ENDGÜLTIG GELÖST (2026-07-11)**: Maintainer bestätigt nach v3 + Frame-Pacing-Fix: „der 90hz fix ist komplett funktional laut moonlight". Der Rest dieser Sektion ist historische Diagnose.

**Symptom auf CachyOS Plasma 6.6.4 + RTX 3070**: Drei gleichzeitige Issues nach `806a7ca`-Test:
1. KDE Portal-Auswahldialog erscheint („welcher Bildschirm soll geteilt werden") — `kwin_session_t::start()` gibt `false` zurück, Sonnenschein fällt auf Portal-Pfad.
2. SteamDeck-Client zeigt Stream-Fehler / kein Bild — auch Portal-Pfad failt oder läuft im inkonsistenten State.
3. Physische Monitore bleiben nach Disconnect oder Process-Ende aus → Maintainer muss Force-Shutdown machen → **keine Logs verfügbar**.

#### Was bei der Diagnose ausgeschlossen wurde (gegen `/root/snsbuild/generated-src/`)

- **Listener-Struct-Field-Mismatch (NULL-pointer-crash durch fehlende Felder)**: alle vier Listener-Structs verifiziert: `kde_output_device_v2_listener` **40/40** ✓, `kde_output_device_mode_v2_listener` **5/5** ✓, `kde_output_configuration_v2_listener` **3/3** ✓, `kde_output_device_registry_v2_listener` **2/2** ✓. **Kein Mismatch.**
- **Vector-Reallocation-Hazard im `on_kde_output_mode`**: Safe wegen `unique_ptr`-Mechanik — der Heap-`kde_mode_t` bleibt stabil bei Vector-Realloc, nur der `unique_ptr`-Handle wandert. Listener-Pointer (`raw`) zeigt auf das heap-allokierte Objekt, nicht auf den Vector-Slot.

#### Wahrscheinliche Versagensmodi (jeder kann das Symptom-Tripel erklären)

- **A) Mode-List / Configuration Destroy-Reihenfolge**: In `apply_kde_configuration` wird `kde_output_configuration_v2_destroy(configuration)` aufgerufen, BEVOR `kde_mode_list_v2_destroy(mode_list)` (das passiert erst danach im Caller). Laut Wayland-XML sind beide Siblings (kein Parent/Child), aber KWin könnte sich auf eine bestimmte Reihenfolge verlassen für Server-State-Konsistenz. **Fix-Strategy v3-3a**: mode_list destroy ZUERST, dann configuration.

- **B) KWin schließt den Stream bei Output-Mode-Change**: `zkde-screencast-unstable-v1.xml` definiert **kein** Verhalten bei Output-Reconfiguration. Wenn KWin den screencast-Stream beim `kde_output_configuration_v2_mode + apply` schließt, feuert `stream.closed` → `on_stream_closed` setzt `failed=true`. Mein `start()` prüft `failed` NICHT nach `apply_output_management_settings()` — der capture-loop läuft auf einem toten Stream weiter. **Fix-Strategy v3-3b**: nach `apply_output_management_settings` `if (failed) return false;` → sauberer Portal-Fallback.

- **C) Init-Roundtrip-Race**: Zwei `wl_display_roundtrip` in `init()` reichen evtl. nicht für die `kde_output_device_v2`-Properties — `start()` arbeitet auf unvollständigem State. **Fix-Strategy v3-3e**: dritter Roundtrip.

#### Konfirmierte Schwächen (orthogonal zum 60-Hz-Bug)

- **`KwinWaylandBackend` hat keinen Destruktor** (`src/platform/linux/virtual_display/backends/kwin_wayland.cpp` Z 178-237). `destroy_all()` (Z 211-223) wird nur explicit aus `proc::proc_t::terminate()` aufgerufen.
- **Keine SIGSEGV/SIGABRT-Handler** in `src/main.cpp` Z 39-50, 300-329. Nur SIGINT + SIGTERM rufen `proc::proc.terminate()`. Bei Crash → kein Cleanup → Monitore bleiben aus. **Diese Schwäche existiert auch ohne meinen 60-Hz-Fix, wurde aber durch v2 sichtbar.**

#### Diagnostic-Pfad: warum keine Logs

`BOOST_LOG` schreibt asynchron in einen log-sink. Bei Force-Shutdown (`pkill -9` oder Reboot) wird der log-buffer NICHT geflusht → letzte Sekunden verloren. `journalctl --user -u sonnenschein.service` zeigt nur was schon zu syslog kam.

**Lösung**: Phase-2-Cleanup-Hardening (SIGSEGV/SIGABRT-handler ruft `logging::log_flush()` + `proc.terminate()` + `_Exit()`). Mit dem Hardening hinterlässt jeder Misserfolg Logs.

#### Plan-Datei

Vollständiger Recovery-Plan (Phase 1 Revert, Phase 2 Cleanup-Hardening, Phase 3 v3-Fix, Phase 4 Test-Sequenz, Verification): `~/.claude/plans/jetzt-haben-wir-ein-nested-candle.md`.

---

### 9.21 Code-Review der Laufzeit-Fixes (Session 2026-05-28) — Punch-List

Statische Review der nicht-verifizierten Laufzeit-Fixes (60-Hz v3, Crash-Recovery, Backend-Lifecycle). **Keine** Hardware verfügbar → reine Code-Analyse. Zwei triviale Härtungen wurden eingebaut, der Rest ist als Test/Entscheidung markiert.

**(a) Eingebaut (eindeutig korrekt, nicht am Capture-Pfad):**
- ✅ **Null-Guard in `select_backend`** (`virtual_display/factory.cpp`): Manual-Override- und Auto-Pfad prüfen jetzt `if (!candidate) continue;` bevor `->name()`/`->available()` dereferenziert wird. Latent (Factories liefern aktuell immer non-null), aber sauber.
- ✅ **Crash-Handler-Kommentar korrigiert** (`main.cpp:344`): Kommentar behauptete fälschlich `destroy_all()` + „safe even if heap is corrupt". Tatsächlich läuft `terminate()` → Backend-`destroy()` (Mutex + fork/exec + alloc) → **nicht** async-signal-safe. Kommentar stellt das jetzt richtig und nennt `recover_on_boot()` als verbindliches Sicherheitsnetz.

**(b) Offen — CachyOS-Test / Maintainer-Entscheidung (NICHT geändert):**
- 🔴 **HIGH — SIGSEGV/SIGABRT-Handler nicht async-signal-safe** (`main.cpp:351-363`): Der Forwarder ruft `std::map::at` + `std::function` aus dem Signal-Kontext; die Lambdas rufen `proc::proc.terminate()` → `kwin_wayland::destroy()` (nimmt `std::mutex`, fork/`fdopen`/`malloc`/`waitpid`) + `recovery::untrack_disabled_output()` (Mutex + `ofstream`/`rename`). **Wenn der Crash auftrat, während dieser Mutex gehalten wurde → Deadlock im Handler → Prozess hängt statt zu sterben → Monitore bleiben aus UND Boot-Recovery triggert nie.** Empfehlung des Reviews: SIGSEGV/SIGABRT auf bloßes `_Exit(128+sig)` (+ ggf. `write(2)` fixer String) reduzieren und sich voll auf `recover_on_boot()` verlassen. **Verhaltensänderung am bewusst gebauten Crash-Recovery (`b9f431b`) → Maintainer-Entscheidung + CachyOS-Verifikation, ob der Handler real hängen kann.** `TODO(cachyos-test)` ist im Code hinterlegt.
- 🟡 **MED (verify) — Mid-Stream-Reconfig `failed`-Race** (`pwgrab.cpp` ~308-339): Nach erfolgreichem `wait_for_stream()` kann ein `kscreen`/`apply_output_management`-Mode-Switch KWin den Stream schließen lassen (`on_stream_closed` → `failed=true`); der Guard bei ~334 fällt dann auf Portal zurück. Korrekt **nur wenn** das `closed`-Event vom `wl_display_roundtrip` in `apply_kde_configuration` (~779) dispatcht wird. Bei verzögertem Close wird `failed` stale gelesen → Weiterarbeit mit totem `node_id`. Auf RTX-3070-Target prüfen; ggf. ein `wl_display_dispatch_pending` vor dem Check.
- 🟡 **MED (verify) — `add_custom_kde_mode` 800ms/1500ms-Budgets** (`pwgrab.cpp` ~818/874): Apply mit 800ms, dann bis 1500ms auf Mode-Erscheinen warten. Auf busy Compositor kann „applied" vor dem Mode-Event zurückkommen → stiller Fallback auf kscreen-doctor. Budgets auf Target verifizieren.
- 🟢 **LOW — `kde_modes`-Map wird bei Output/Mode-Removal nicht gepruned** (`pwgrab.cpp` ~1166/1236): `on_kde_*_removed` setzt nur `removed=true`, erased aber nicht aus `kde_modes`. Innerhalb einer kurzen Capture-Session benign (Destruktor räumt auf); bei Hot-Remove eines virtuellen Outputs mid-session bleibt ein dangling Key. Fix wäre einzeiliges `erase` — **bewusst nicht gepatcht** (Capture-Pfad, §9.20-Lehre); dokumentiert für späteres gezieltes Fix.

**Bestätigt unkritisch:** `recovery.cpp` selbst ist sauber (atomar temp+rename, mutex-guarded, idempotent, Lockfile auch bei Teil-Enable geleert) und wird **nicht** aus einem Signal-Handler gerufen. `subprocess.cpp` child-side `strerror`/`std::string` nach fork ist technisch nicht async-signal-safe, aber nur im Child vor `_exit` → low risk, belassen.

---

### 9.22 Installer-setcap brach PipeWire-Capture (GELÖST 2026-07-11)

**Symptom**: Nach Installer-Lauf startet kein Virtual-Display-Stream mehr; Portal-Fehler / alle Encoder scheitern.

**Ursache**: `installer/lib/permissions.sh` setzte bedingungslos `cap_sys_admin+p` aufs Binary. Laut §9.10 verweigert xdg-desktop-portal aber Binaries mit File-Capabilities (`Unable to open /proc/PID/root`) — und PipeWire/Portal ist der einzige Capture-Weg für Virtual Displays (§9.13-Architektur-Tabelle). Der Installer hat also das Kern-Feature deaktiviert.

**Fix (Session 2026-07-11)**: Kein setcap per Default. Opt-in `install.sh --kms` (mit dickem Warning) nur für User, die ausschließlich physische Monitore via KMS mirrorn. `install.sh`, `update.sh` und `doctor.sh --repair` ENTFERNEN stale Capabilities aktiv (`setcap -r`). `doctor.sh` meldet vorhandene Caps als [FAIL] mit Erklärung.

### 9.23 Steam Deck: gedrehtes Bild / Portrait-Auflösung (Fix eingebaut, Test offen)

**Symptom**: Bild im Stream auf dem Steam Deck erscheint 90° gedreht bzw. Virtual Display wird hochkant (800x1280) angelegt.

**Ursache**: Das Steam-Deck-OLED-Panel ist nativ ein **Portrait-Panel (800x1280)**, das SteamOS um 90° gedreht darstellt. Je nach Moonlight-Client/Modus wird die native Portrait-Auflösung angefordert. Sonnenschein legte das Virtual Display dann 1:1 hochkant an.

**Fix (Session 2026-07-11)**: Landscape-Normalisierung an beiden Eingangspunkten: `nvhttp.cpp::make_launch_session` (nach Mode-Parsing) und `rtsp.cpp` (`config.monitor` Viewport-Parsing). Wenn `height > width` → swap + Info-Log. Kein Config-Toggle — Game-Streaming ist immer Landscape.

### 9.24 package-lock.json fehlte im Repo (GELÖST 2026-07-11)

**Symptom**: `npm ci` schlägt fehl („can only install with an existing package-lock.json"), keine reproduzierbaren WebUI-Builds, Flatpak-Manifest kopiert eine nicht existente Datei.

**Ursache**: `package-lock.json` stand in `.gitignore` (Apollo-Erbe).

**Fix**: Zeile aus `.gitignore` entfernt, Lockfile erzeugt und committed. CI/Installer können jetzt `npm ci` nutzen (der Build-Weg über CMake ruft weiterhin `npm install`, funktioniert unverändert).

---

## 10. Letzte Commits chronologisch

(neueste zuerst, Format: `hash` — Beschreibung — Tag)

```
249afec — fix(deck): restore host sync and add streaming indicators — 2026-07-20 (v0.2.2-test candidate)
9d9bc07 — docs(status): round 7 — install script rate-limit fix, verified — 2026-07-20
cd58f54 — fix(deck): handle GitHub API rate limits in install script — 2026-07-20
1cf323d — docs(status): v0.2.1-test live — round 6 complete and verified — 2026-07-20
<this commit> — feat(library): Steam artwork endpoint + docs — 2026-07-12
6b82593 — docs(status): round 10 — Phase 6 done, Steam library API, scope decisions — 2026-07-12
a5d8686 — feat: update-state + branch selector + Steam library API — 2026-07-12
5223079 — feat(update): auto-rollback, manual revert, auto-update timer (Phase 6) — 2026-07-12
28d8f98 — ci: harden openSUSE Tumbleweed job (green, now required) — 2026-07-12
7bc4500 — docs(status): round 9 complete — apps+config migrated, self-update endpoint, openSUSE CI — 2026-07-12
ca21bf2 — feat(update): web-triggered self-update endpoint + dashboard button — 2026-07-12
2f8530a — feat(webui): PrimeVue configuration shell, live-tested — 2026-07-12
95ecd1d — feat(webui): PrimeVue applications page, live-tested against real backend — 2026-07-12
9670739 — docs(status): round 8 complete — E2E passed, password + troubleshooting pages — 2026-07-12
95e5742 — feat(webui): PrimeVue troubleshooting page, shared Topbar component — 2026-07-12
282415f — feat: PrimeVue password page + skip distro nodejs/npm when present; Ubuntu E2E passed — 2026-07-12
a5a1170 — docs(status): round 7b — login page, crash reporter v1, libva backport — 2026-07-11
8f82ef1 — feat(webui): PrimeVue login page, live-tested against real backend — 2026-07-11
9d9269b — feat: crash-reporter v1 button + automatic libva backport for Debian/Ubuntu — 2026-07-11
92bbb9c — docs(status): round 7 — live-tested dashboard, WoL, CI hardening — 2026-07-11
e2dbed8 — feat(installer): Wake-on-LAN setup + doctor check; harden CI matrix — 2026-07-11
82b4ac0 — feat(webui): PrimeVue dashboard with live status cards (Phase 5) — 2026-07-11
2bb77b5 — docs(status): record Phase 5 round 1 — setup wizard, PIN-first pairing — 2026-07-11
78e69b4 — feat(webui): PrimeVue setup wizard, PIN-first pairing, locale-API resilience (Phase 5) — 2026-07-11
86bf252 — docs(status): HDR test result — KWin virtual outputs lack HDR capability upstream (Plasma 6.7) — 2026-07-11
150305c — docs(status): round 4 — 90 Hz confirmed solved, HDR stage 1 recorded — 2026-07-11
ffe213e — feat(capture): enable HDR on KWin virtual outputs when the client requests it (Phase 4, stage 1) — 2026-07-11
cca29b9 — docs(roadmap): add client track (native Steam Deck integration) + boot-to-ready host — 2026-07-11
78030cc — docs(status): record round 3 — fps pacing fix, HDR + controller analysis — 2026-07-11
027975e — fix(capture): repeat last frame at client cadence instead of stalling — 2026-07-11
(ältere Runde-2/1-Commits siehe git log)
<frühere Session> — docs: record production-readiness session (installer overhaul, Steam Deck fixes) — 2026-07-11
e0ba34a — build(webui): commit package-lock.json for reproducible installs — 2026-07-11
9be3ba4 — fix(capture): persist portal restore token across restarts — 2026-07-11
e079f66 — fix(stream): normalize portrait client resolutions to landscape — 2026-07-11
a613967 — fix(installer): /opt prefix, manifest-based uninstall, doctor.sh, no default setcap — 2026-07-11
7fc076b — fix(packaging): rename PKGBUILD install artifact in arch dockerfile — 2026-05-28
964b0b0 — fix(packaging): repair stale sunshine refs after rebrand — 2026-05-28
0a6c227 — docs: record overhaul progress — Phase 1.6 rebrand, installer + WebUI foundations — 2026-05-28
19bf915 — review(virtual-display): null-guard backend factory, correct crash-handler note — 2026-05-28
4408e1a — feat(webui): PrimeVue 4 foundation with isolated diagnostics page (Phase 5) — 2026-05-28
efd295a — feat(installer): distro-aware from-source installer scaffold (Phase 3) — 2026-05-28
822682a — rebrand(phase1.6): Apollo/Sunshine → Sonnenschein across user-facing surfaces — 2026-05-28
2452682 — docs(status): record Phase 2+3 of v3 recovery, CachyOS test ready — 2026-05-14
b0f4fd1 — fix(capture): 60-Hz fix v3 — KDE output management with five hardenings — 2026-05-14 (CachyOS-Test pending)
b9f431b — feat(cleanup): crash-safe physical-monitor restoration — 2026-05-14 (SIGSEGV/SIGABRT-handler + RAII + Lockfile-Recovery)
a6ca074 — docs(status): record v2 rollback and Phase 1 of v3 recovery plan — 2026-05-14
d7afb8b — revert build(cmake): generate KDE output management v2 wayland protocol bindings — 2026-05-14
ea201f5 — revert fix(capture): add KDE output management v2 path for client-requested refresh rate — 2026-05-14
c1c7bb4 — docs(status): record 60-Hz fix v2 commits and refine Codex bug diagnosis — 2026-05-14 (BESCHREIBT v2 das jetzt zurückgerollt ist — siehe §9.20)
806a7ca — fix(capture): add KDE output management v2 path for client-requested refresh rate — 2026-05-14 (ZURÜCKGEROLLT in ea201f5)
2996b4e — build(cmake): generate KDE output management v2 wayland protocol bindings — 2026-05-14 (ZURÜCKGEROLLT in d7afb8b)
1248943 — docs(status): record d77a2be + 150dfd0 in commit chronology — 2026-05-14
d77a2be — revert(vdisplay): roll back 29cd4b6 — kscreen-doctor add-virtual-output proven wirkungslos on CachyOS — 2026-05-14
150dfd0 — docs(status): update STATUS.md for 60-Hz fix commit 29cd4b6 — 2026-05-14
29cd4b6 — fix(vdisplay): pre-create KWin virtual output via kscreen-doctor for correct refresh rate — 2026-05-14 (NICHT WIRKSAM — KDE-Cache-Eintrag, kein wl_output)
41fa9ba — fix(capture): use portal fallback when KWin screencast is unavailable — 2026-05-13
74c63cf — revert(capture): restore pre KScreen resolver capture path — 2026-05-13
501431a — revert(capture): restore stable KWin direct capture path — 2026-05-13
edc144e — fix(capture): fall back when KWin direct capture is unavailable — 2026-05-13
723537a — fix(capture): configure KWin virtual outputs via output management — 2026-05-13
6504268 — fix(capture): resolve KScreen output before setting refresh rate — 2026-05-13
bf7d939 — fix(capture): bypass '@' character corruption in compiler and add delay for KWin mode registration — 2026-05-10
2d5b81a — docs: finalized E2E Refresh Rate and Headless Mode documentation — 2026-05-10
6c798e4 — fix(capture): remove string_view literals from BOOST_LOG formatting that caused string corruption in fps metadata — 2026-05-10
9c63e32 — docs: update STATUS.md regarding framerate string fixes — 2026-05-10
be20703 — fix(capture): replace error-prone float formatting with simple string conversions to fix garbage log output and 60Hz mode-set lock — 2026-05-10
6bbb820 — fix(capture): dynamically set virtual output refresh rate using kscreen-doctor after stream starts — 2026-05-10
e453afa — fix(capture): format framerate correctly and disable physical screens in KWin virtual output backend — 2026-05-10
23d0778 — docs: document stream_virtual_output migration — 2026-05-10
d84072e — fix(capture): migrate KWin virtual display to stream_virtual_output — 2026-05-10
96203b1 — docs: update STATUS.md with Wayland roundtrip hotfix — 2026-05-10
f40b4f5 — fix(capture): sync Wayland display roundtrip in KWin direct capture to catch delayed virtual outputs — 2026-05-10
102b36b — fix(capture): capture wl_output description and add Virtual-* fallback for KWin — 2026-05-10
2df15c4 — docs: update STATUS.md with KWin direct capture fix — 2026-05-10
4c63d36 — fix(capture): use KWin direct screencast for virtual outputs — 2026-05-10
bb3e758 — docs: update STATUS.md with portal source diagnostics — 2026-05-10
447dc8b — fix(capture): log portal source and request cursor — 2026-05-10
12a17da — docs: update STATUS.md with PipeWire format fix — 2026-05-10
76d03a4 — fix(capture): request client format from PipeWire — 2026-05-10
5fe1ea6 — docs: update STATUS.md with PipeWire fd fix — 2026-05-10
6d6433f — fix(capture): fix PipeWire remote fd options — 2026-05-10
3c730a4 — docs: update STATUS.md with D-Bus token/path mismatch fix — 2026-05-10
60dac9b — fix(capture): fix D-Bus Portal token/path mismatch causing signal timeouts — 2026-05-10
84347f8 — docs: update STATUS.md with stream probe fix — 2026-05-10
af603b6 — fix(capture): bypass Portal D-Bus dialogs during stream pre-flight encoder probe — 2026-05-10
417013a — docs: update STATUS.md with D-Bus timeout boot fix — 2026-05-10
91967a6 — fix(capture): bypass Portal D-Bus dialogs during boot encoder probe — 2026-05-10
ad33986 — fix(capture): resolve PipeWire timeout and boot failures — 2026-05-10
81e00c3 — docs: Phase 4 status update — PipeWire implemented, portal setcap conflict documented — 2026-05-10
4fa5376 — fix(capture): bypass boot-time sources bitset for runtime PipeWire override — 2026-05-10
50d6fa4 — feat(capture): Phase 4 — PipeWire portal capture backend for virtual displays — 2026-05-10
f475ba6 — docs: Phase 2 complete — KMS cannot capture virtual displays, PipeWire required — 2026-05-10
71cdac3 — fix(capture): route display_name to KMS index, not vdisplay name — 2026-05-10
9baebbf — fix(capture): route KMS capture to primary monitor for Linux virtual display — 2026-05-10
8126f26 — fix(cmake): don't FATAL_ERROR on CUDA when GCC skip was intentional — 2026-05-10
4fff349 — docs: add CLAUDE.md, SESSION_PROMPT.md, update STATUS.md — 2026-05-10
5e1b1cf — fix(cmake): skip CUDA when host GCC > 15 (cudafe++ incompatible) — 2026-05-10
b6f263e — fix(cmake): pre-flight Boost components on disk + auto-find Arch nvcc — 2026-05-10
895295c — fix(cmake): detect partial Boost installs and fall back to FetchContent — 2026-05-10
29298db — fix(cmake): accept any system Boost >= 1.89 instead of EXACT pin — 2026-05-10
72cf5c1 — Phase 2C: Wire virtual-display abstraction into process.cpp on Linux — 2026-05-10
509e87a — Phase 2B: KWin-Wayland virtual-display backend implementation — 2026-05-10
7cdd3f0 — Phase 2A: Virtual-display abstraction foundation (Linux) — 2026-05-09
539d3a5 — Phase 1.7: Rewrite docs/building.md for Sonnenschein — 2026-05-09
a95f2ee — Phase 1.3: Init submodules + pin tray pre-Qt — 2026-05-09
235920b — Initial import: Apollo codebase as Sonnenschein basis — 2026-05-09
```

`main` Branch zeigt nur auf `235920b` (initial import). `dev` ist die aktive Entwicklungs-Linie und liegt ca. 30+ Commits vor `main`.

**Auf `dev` nächster Test-Commit = `41fa9ba`** (Stand 2026-05-13, nach Push). Nächster Schritt ist ausschließlich CachyOS-Validierung, dass der Stream bei fehlendem `zkde_screencast_unstable_v1` nicht mehr abbricht, sondern den Portal-Pfad nutzt. 90-Hz/HDR-Arbeit pausiert bis diese Basis wieder bestätigt ist.

---

## 11. Datei-Inventar (was wir geschrieben haben)

Liste der Dateien, die durch Sonnenschein neu sind oder substantiell geändert wurden. Apollos Original-Dateien sind nicht aufgelistet.

### Repo-Root + Doku
- `CLAUDE.md` (NEU) — Pflichtlektüre für Claude-Code
- `README.md` (REWRITE) — DE/EN Sonnenschein-Style
- `NOTICE` (REWRITE) — Sunshine + Apollo + docker-steam-headless Attribution
- `package.json` (PATCH) — name → "sonnenschein"
- `.gitignore` (EXTEND) — `_reference/`, `*.log`, etc.
- `docs/STATUS.md` (NEU) — DIESE DATEI
- `docs/ROADMAP.md` (NEU) — 8-Phasen-Vision
- `docs/SESSION_PROMPT.md` (NEU) — Session-Kickoff-Vorlage
- `docs/building.md` (REWRITE) — Linux-only Build-Anleitung mit Distro-Lists + Troubleshooting

### Community / GitHub
- `.github/CONTRIBUTING.md` (REWRITE)
- `.github/CODE_OF_CONDUCT.md` (NEU)
- `.github/SECURITY.md` (NEU)
- `.github/PULL_REQUEST_TEMPLATE.md` (NEU)
- `.github/ISSUE_TEMPLATE/{bug,feature,crash,config}.yml` (NEU)
- `.github/copilot-instructions.md` (REWRITE) — Linux-first
- `.github/workflows/lint.yml` (NEU)
- `.github/workflows/build-linux.yml` (NEU/PATCH) — Linux-Matrix, `continue-on-error`, seit KWin-Direktcapture mit rekursiven Submodules.

### CMake-Patches
- `cmake/dependencies/Boost_Sunshine.cmake` (PATCH) — Pre-flight + FetchContent-fallback
- `cmake/compile_definitions/linux.cmake` (PATCH) — virtual_display sources eingehängt, CUDA-Pfad-Auto-Detection (/opt/cuda), tray_linux.c (statt .cpp wegen Pin), PipeWire + KWin-Direct-ScreenCast + KWin-Output-Management-v2-Protokollgeneration.
- `cmake/prep/options.cmake` (PATCH) — `SUNSHINE_ENABLE_KWIN` für optionalen KWin-Direct-ScreenCast-Pfad.

### C++ — virtual_display Modul (NEU, 14 Files)
- `src/platform/linux/virtual_display/types.h` — Enums + Structs
- `src/platform/linux/virtual_display/interface.h` — IBackend
- `src/platform/linux/virtual_display/detection.h+cpp` — Environment-Detection
- `src/platform/linux/virtual_display/factory.h+cpp` — Backend-Auswahl
- `src/platform/linux/virtual_display/subprocess.h+cpp` — popen/fork+exec Helper
- `src/platform/linux/virtual_display/backends/all.h` — make_*-Deklarationen
- `src/platform/linux/virtual_display/backends/kwin_wayland.cpp` — ✅ implementiert. `29cd4b6` bringt das `kscreen-doctor add-virtual-output` + `mode-set` Pre-Create-Pattern zurück, das `e453afa` entfernt hatte; dazu Track-Feld `ActiveDisplay::added_via_kscreen` damit `destroy()` korrekt entscheidet ob `remove-virtual-output` aufgerufen wird.
- `src/platform/linux/virtual_display/backends/{mutter_headless,wlroots_headless,xorg_nvidia,xorg_dummy,amdgpu_param,evdi}.cpp` — Stubs
- `src/platform/linux/virtual_display/README.md` — Architektur

### C++ — process.cpp Wiring (Phase 2C)
- `src/process.h` (PATCH) — `#elif __linux__` Header-Include + zwei private Member
- `src/process.cpp` (PATCH) — Linux-Branch in `execute()` + `terminate()`

### C++ — PipeWire Capture (Phase 4)
- `src/platform/linux/pwgrab.cpp` (NEU/PATCH) — xdg-desktop-portal ScreenCast + PipeWire-Stream; `447dc8b` loggt Portal-Source-Properties und fordert Embedded Cursor an; `4c63d36` nutzt KWin Direct ScreenCast für benannte `Sonnenschein-...`-Outputs und blockiert den KDE-XDG-`VIRTUAL`-Fallback; `d84072e` migriert den KWin-Pfad auf `stream_virtual_output`; `bf7d939` versucht den erzeugten KScreen-Output nach Stream-Start auf die Client-Refresh-Rate zu setzen; `6504268` pollt `kscreen-doctor -o`, setzt den Mode auf dem tatsächlich registrierten Output und verifiziert das Ergebnis; `501431a` setzte zunächst zurück auf `67a93e3`; `74c63cf` setzt weiter zurück auf `bf7d939`, weil `6504268` laut Maintainer schon defekt war; `41fa9ba` entfernt den fatalen Abbruch, wenn KWin Direct nicht verfügbar ist.

### Installer (Session 2026-07-11 Komplett-Umbau)
- `installer/install.sh` (REWRITE) — /opt-Prefix, persistenter Source-Checkout, Manifest-Kopie, Paket-Snapshot, Root-Guard, non-TTY-Autoconfirm, `--kms`-Opt-in
- `installer/uninstall.sh` (REWRITE) — manifest-basiert, Paket-Entfernung, Linger-Revert, main()-gekapselt (löscht das eigene Verzeichnis sicher)
- `installer/doctor.sh` (NEU) — 9 Check-Gruppen + `--repair`
- `installer/update.sh` (PATCH) — /opt-Prefix, SRC_DIR-Auflösung aus install-state.env, stale-Caps-Entfernung, Installer-Kopie-Sync
- `installer/post-install.sh` (PATCH) — /opt-Prefix, KMS-Cap-Parameter
- `installer/lib/permissions.sh` (REWRITE) — setcap nur bei `--kms`, aktive Cap-Entfernung sonst
- `installer/lib/packages.sh` (PATCH) — `packages_snapshot_before` / `packages_record_new`
- `installer/lib/service.sh` (PATCH) — LINGER_ENABLED-Tracking
- `installer/lib/ui.sh` (PATCH) — `ui_final_summary`

### C++ — Steam-Deck-Fixes (Session 2026-07-11)
- `src/nvhttp.cpp` + `src/rtsp.cpp` (PATCH) — Portrait→Landscape-Normalisierung (§9.23)
- `src/platform/linux/pwgrab.cpp` (PATCH) — Portal-Restore-Token-Persistenz (§9.15): `restore_token_path/load/save/drop`, Retry ohne Token bei SelectSources-Fehler

### Build-Reproduzierbarkeit
- `package-lock.json` (NEU committed, war gitignored) + `.gitignore`-Zeile entfernt (§9.24)

### Submodule-Pin
- `third-party/tray/` — gepinnt auf `7936cb35` (vor `.gitmodules`-Datei; gitlink im Tree)
- `third-party/plasma-wayland-protocols/` — neu gepinnt auf `13b3c2f`, liefert `zkde-screencast-unstable-v1.xml` für KWin Direct ScreenCast.

---

## 12. Was als nächstes — konkrete Schritte

> **Neu geschrieben 2026-07-16.** Die historischen §12-Einträge (60-Hz-v3-
> Testplan, alter CMake-Rebrand-Plan, Installer-Reihenfolge) waren erledigt
> oder überholt (60-Hz/90-Hz vom Maintainer bestätigt, Phase 3+6 fertig,
> CachyOS-Gesamttest „funktioniert perfekt" 2026-07-11) und wurden entfernt.
> Tagesaktuelle Autorität bleibt der „HIER WEITERMACHEN"-Block ganz oben
> plus die Runden-Nachträge.

In Reihenfolge der Priorität:

### A) ✅ ERLEDIGT 2026-07-16: Client-CI grün + Release v0.0.3-test + Fork archiviert

Details im HIER-WEITERMACHEN-Block. Ebenfalls erledigt: Deck-Tests
Spiele-Grid/Boxart ✅ + RD-1 ✅ (Maintainer bestätigt).

### B) Client-Modernisierung M1–M6 (Maintainer-Auftrag 2026-07-16, in Arbeit)

Plan in ROADMAP „Client-Modernisierung". Reihenfolge: **M1** Easy/Advanced +
Auto-Settings-Engine → **M2** RD-Abfrage-Dialog → **M3** konstante volle FPS
(Host-Pacing verifizieren + Client fordert native Refresh) → **M4** Rebrand +
moderne UI (inkl. dmg-Name) → **M5** Windows-Installer x64 + 1-Klick-Updater →
**M6** Decky-Plugin (nutzt Client-App; Recherche-Ergebnis in Runde 13/14).

### B2) Verschoben aus altem B): offene Client-Track-Punkte

- cert-auth-Zugriff auf `/api/library` für gepairte Clients entscheiden
  (offene Architektur-Frage; für M6/einheitliche Liste relevant).
- Nativer Steam-Deck-Controller: Machbarkeitsskizze liegt vor (Runde 13);
  nächster Schritt wäre der Standalone-uhid-PoC (`28de:1205`).

### C) Phase-1.6-Rest (kosmetisch, LOW)

Real längst erledigt (entgegen älterer §6/§12-Einträge): `project(Sonnenschein)`
(CMakeLists.txt:7), eigene FQDN-Service/Desktop-Files
(`io.github.elias02345.Sonnenschein.*`, `sonnenschein.service.in`). Noch offen,
rein intern: `cmake/prep/build_version.cmake:57` loggt „Sunshine Branch:", und
das CMake-Target heißt `add_executable(sunshine)` (cmake/targets/common.cmake:4)
— bei Umbenennung Symlink-/Installer-Kompat beachten.

### D) Phase 5 WebUI-Rest

17 Bootstrap-Config-Tabs (`configs/tabs/`) → PrimeVue mit dem etablierten
Live-Test-Setup; Geräteverwaltung. Kein Funktionsverlust im Ist-Zustand.

### E) Phase 4 HDR — upstream-blockiert

KWins virtuelle Outputs melden kein HDR-Capability (Plasma 6.7, caps=0x2000,
Runde 5). Unser Code ist capability-guarded und selbstaktivierend — bei jedem
Plasma-Update einen Stream mit HDR-Request testen (Log „HDR+WCG enabled").

### F) Sunshine-Upstream-Sync

Spätestens Phase 7. Script `scripts/sync-from-sunshine.sh` automatisieren,
das Sunshine für sicherheitsrelevante Fixes cherry-pickt.

### Referenz: CachyOS-Gesamttest-Prozedur (Session 2026-07-11, bestanden)

1. **Installer**: `curl -fsSL https://raw.githubusercontent.com/Elias02345/sonnenschein/main/installer/install.sh | bash`
   — erwartet: Build läuft durch, alles landet in `/opt/sonnenschein`, Abschluss-Summary mit WebUI-URL.
2. **Doctor**: `bash /opt/sonnenschein/installer/doctor.sh` — erwartet: alle Checks [OK], insbesondere „No file capabilities" (§9.22).
3. **Stream** (Steam Deck OLED): Pairing → Stream. Erwartet:
   - Auflösung 1280x800 **Landscape** (auch wenn der Client Portrait anfordert — Log-Zeile „normalizing to landscape", §9.23)
   - Beim ALLERERSTEN Portal-Fallback ggf. einmal Dialog; ab dann nie wieder (Restore-Token, §9.15)
   - 90 Hz statt 60 Hz (60-Hz-Fix v3 aus 2026-05-14, §9.20)
   - Physische Monitore aus während Stream, wieder an nach Disconnect
4. **Uninstall-Probe** (optional): `bash /opt/sonnenschein/installer/uninstall.sh` — erwartet: System wie vorher.
5. Bei Fehlern: `journalctl --user -eu sonnenschein` + `/tmp/sonnenschein-install.log` posten.

---

## 13. Update-Konventionen für diese Datei

**Regel 1**: Diese Datei wird nach jedem abgeschlossenen Schritt aktualisiert. Nicht erst am Ende einer Session — sofort.

**Regel 2**: Aktualisierungen passieren in folgenden Abschnitten:
- **TL;DR** — kurzer Satz zum aktuellen Zustand, immer top-of-mind.
- **Phasen-Status (§6)** — Aufgaben-Tabelle mit ✅/🟡/🔴/⏸ aktualisieren, Commit-Hashes nachtragen.
- **Bekannte Probleme (§9)** — neue Bugs + Workarounds dokumentieren, mit Reproduktions-Befehl + Fix-Commit.
- **Letzte Commits (§10)** — neue Commits oben anhängen.
- **Datei-Inventar (§11)** — neu angelegte oder substantiell geänderte Files dazuschreiben.
- **Was als nächstes (§12)** — Reihenfolge anpassen, abgeschlossene Punkte streichen, neue dazuschreiben.

**Regel 3**: Stand-Datum oben (Zeile 6) bei jeder Aktualisierung neu setzen.

**Regel 4**: Wenn der Maintainer eine neue Entscheidung fällt (z.B. "wir nutzen jetzt doch Bootstrap statt PrimeVue"): unter §4 (Tech-Stack-Entscheidungen) als zusätzliche Zeile festhalten, mit Datum + Begründung.

**Regel 5**: Wenn ein Commit, der hier erwähnt ist, gerebased oder amended wird: hier den Hash aktualisieren. Dieser Markdown ist die Autorität für "welcher Commit hat was getan", nicht ein vergessenes Sticky-Note.

**Regel 6**: Wenn unklar ob etwas in STATUS.md gehört: lieber rein als raus. Diese Datei ist der einzige Schutz gegen Multi-Session-Kontext-Verlust.

---

## 14. Deep-Dive: PipeWire & Portal Integration (Phase 4)

Die Implementierung in `pwgrab.cpp` nutzt das `org.freedesktop.portal.ScreenCast` Interface. Hier sind die technischen Details der kritischen Komponenten:

### D-Bus Portal Handshake
1. **CreateSession**: Erstellt eine Session. Wir übergeben einen `session_handle_token`.
2. **SelectSources**: Wählt die Quelle aus. Wir setzen `types=1` (Monitor) und `persist_mode=2` (Persistent). Hier erscheint beim ersten Mal der KDE-Dialog.
3. **Start**: Aktiviert den Stream. Gibt die PipeWire `node_id` zurück.

### Das Token-System (Fix 60dac9b)
Jeder Portal-Request ist asynchron. Man abonniert ein `Response`-Signal auf einem spezifischen Pfad:
`/org/freedesktop/portal/desktop/request/SENDER/TOKEN`

- **SENDER**: Der eindeutige D-Bus Name (z.B. `1_42`).
- **TOKEN**: Ein von uns gewählter String (z.B. `sonnenschein_1`).

**Fehler**: Vorher wurde für den Pfad und den `handle_token`-Parameter jeweils ein *neuer* Token generiert. Dadurch hörte Sonnenschein auf Pfad `.../sonnenschein_2`, während das Portal die Antwort an `.../sonnenschein_1` schickte. Dies führte zu den 10s Timeouts.

### GLib Main Context Pumping
GIO (GDBus) benötigt einen laufenden Main-Loop, um Signale (wie die Portal-Antwort) zuzustellen. Da Sonnenschein keinen globalen GLib-Loop nutzt, pumpen wir den Default-Context manuell in `wait_response()`:
```cpp
while (!response_received) {
    g_main_context_iteration(nullptr, FALSE);
    std::this_thread::sleep_for(10ms);
}
```

### PipeWire & DMA-BUF
Der Stream wird mit `PW_KEY_STREAM_CAPTURE_PROXY` und DMA-BUF Support initialisiert.
- **Zero-Copy**: PipeWire liefert Dateideskriptoren für GPU-Buffer.
- **EGL-Import**: Diese FDs werden via `eglCreateImageKHR` in GL-Texturen importiert, die dann direkt vom Encoder (NVENC/VAAPI) gelesen werden können.

### Encoder Probing (Dummy-Mode)
Sonnenschein testet Encoder durch Instanziierung eines Displays. Um zu verhindern, dass bei jedem Test ein Portal-Dialog aufpoppt, erkennt `pw_display_t::init()` über den globalen `video::is_encoder_probing_active` Flag, ob es sich um einen Test handelt. Falls ja, wird die D-Bus-Kommunikation übersprungen und ein valides Dummy-Display simuliert.
Falls ja, wird die D-Bus-Kommunikation übersprungen und ein valides Dummy-Display simuliert.
