# SESSION_PROMPT.md — Vorlage für neue Claude-Code-Sessions

**Wofür**: Du arbeitest an Sonnenschein in mehreren Sessions, weil Rate-Limits dich zwingen Pausen einzulegen. Damit jede neue Session sofort den vollen Kontext hat, paste diesen Text als deine erste Nachricht in den neuen Chat.

**Wo**: Im Projekt-Ordner `~/sonnenschein` (Linux/CachyOS) oder `C:\Users\cooki\Documents\ClaudeCode\sonnenschein` (Windows).

**Wann anpassen**: Du musst nichts manuell anpassen — der Prompt verweist Claude direkt auf `docs/STATUS.md`, wo der aktuelle Stand steht. Claude liest das, bevor es loslegt.

---

## Der Prompt — alles ab hier in die neue Session reinkopieren

```
Hi. Wir arbeiten am Sonnenschein-Projekt weiter (Linux-natives, headless 
Moonlight-Streaming-Backend, Apollo-Fork mit Linux-Virtual-Display-Abstraktion).

BEVOR DU IRGENDETWAS TUST:

1. Lies CLAUDE.md (Repo-Root) komplett.
2. Lies docs/STATUS.md komplett. Das ist die einzige Quelle der Wahrheit über 
   den aktuellen Stand — Phasen, Commits, Hardware, Entscheidungen, bekannte 
   Probleme und konkrete nächste Schritte. Da steht wirklich alles drin.
3. Prüfe `git log --oneline -10` und `git status` auf dem dev-Branch — sind 
   Commits seit dem in STATUS.md dokumentierten letzten Stand dazugekommen? 
   Wenn ja, STATUS.md zuerst aktualisieren.
4. Falls ich eine konkrete Aufgabe habe, sage ich sie unten. Falls nicht: 
   schau in STATUS.md → Abschnitt 12 ("Was als nächstes") und schlag vor, womit 
   wir weitermachen. KEINE eigenmächtigen Code-Änderungen ohne Rückfrage, wenn 
   die nächste Aufgabe nicht offensichtlich ist.

WÄHREND DU ARBEITEST:

- Sprache mit mir: immer Deutsch.
- Branch: immer `dev`. Niemals direkt auf `main`.
- Nach jedem abgeschlossenen Schritt (Commit, neuer Bug entdeckt, neue 
  Architektur-Entscheidung): docs/STATUS.md aktualisieren — siehe die 
  Update-Konventionen ganz unten in der Datei.
- Beim Pushen: STATUS.md im selben Commit oder direkt danach updaten 
  (Phase-Status, Letzte-Commits-Liste, Datei-Inventar). Niemals 
  STATUS.md hinterher hängen lassen.
- Wenn ich auf eine Frage von dir antworte: meine Antwort SOFORT in 
  STATUS.md persistieren (z.B. unter §4 Tech-Stack-Entscheidungen oder 
  §9 Bekannte Probleme), bevor du Code dafür schreibst. Sonst geht die 
  Info beim nächsten Rate-Limit verloren.

WICHTIG:

- Mein Test-System ist CachyOS, RTX 3070, Plasma 6.6.4 Wayland, 
  NVIDIA Driver 595. Build geht von SSH, aber Sonnenschein-Tests müssen 
  in einer KDE-Konsole *innerhalb* der Plasma-Sitzung laufen 
  (kscreen-doctor braucht WAYLAND_DISPLAY).
- Mein Dev-Setup ist Windows 11 + WSL2 Ubuntu 24.04 (Build-Verifikation), 
  Build-Dir = /root/snsbuild (NICHT /tmp).
- Maintainer-Mail / GitHub-Account etc. stehen in CLAUDE.md.

Aktuelle konkrete Aufgabe:

[HIER DEINE AUFGABE EINTRAGEN — z.B.:]
[ "Schau in den letzten Logs, der Build auf CachyOS hat noch dieses Problem: ... " ]
[ ODER: "Mach weiter mit dem nächsten geplanten Schritt aus STATUS.md §12." ]
[ ODER: "Lies erstmal alles und dann sag mir, wie wir weitermachen." ]
```

---

## So nutzt du diesen Prompt

1. Neue Claude-Code-Session öffnen, im Sonnenschein-Repo (`cd ~/sonnenschein` oder `cd C:\Users\cooki\Documents\ClaudeCode\sonnenschein`).
2. Inhalt des `[code block]` von oben kopieren (von "Hi." bis zum letzten `]`).
3. Im "Aktuelle konkrete Aufgabe"-Block deine Sache reinschreiben. Beispiele:
   - "Hier ist der Build-Output von CachyOS, fix das: \`\`\` ... \`\`\`"
   - "Ich konnte testen, der Stream läuft, aber HDR funktioniert nicht. Logs: ..."
   - "Mach weiter mit Phase 3, der Installer."
   - "Lies erstmal alles und schlag vor, was als nächstes."
4. Senden. Claude liest dann CLAUDE.md → STATUS.md → git log und legt los.

## Falls der Prompt zu lang ist

Wenn du den Prompt zu lang findest, kannst du auch einfach diesen Mini-Prompt nutzen (Claude Code lädt CLAUDE.md sowieso automatisch beim Start):

```
Lies docs/STATUS.md komplett, dann schau auf git log --oneline -10. 
Aktualisiere STATUS.md falls Commits dazugekommen sind, die noch nicht drin 
sind. Dann: [DEINE AUFGABE].
```

## Wenn STATUS.md selbst veraltet aussieht

Falls Claude Code in einer neuen Session bemerkt, dass STATUS.md nicht zum Code-Zustand passt (z.B. wenn ich offline auf dev gepusht habe ohne STATUS zu updaten), gilt: **STATUS.md zuerst aufholen, dann an die eigentliche Aufgabe**. Niemals annehmen, der dokumentierte Stand sei aktuell — immer mit Git-State abgleichen.

## Verlust-Schutz

Falls beide Sessions gleichzeitig STATUS.md ändern und es zu Merge-Konflikten kommt: STATUS.md ist Plain-Markdown, Konflikte sind manuell auflösbar. Im Zweifel: lieber einen Abschnitt zweimal hinzufügen als einen verlieren — bessere doppelte Information als verlorene.
