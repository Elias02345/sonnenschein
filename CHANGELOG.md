# Changelog

All notable user-facing changes to Sonnenschein are documented here.

## 0.2.8-test — 2026-07-21

### Fixed

- Restored the **Stream with Sonnenschein** control directly on matching
  native Steam game pages using Steam's current controller-focusable button
  structure without re-parenting the Play/Install panel.
- Replaced the mutable shared streaming shortcut with one stable, hidden
  runtime entry per host game. Streams no longer create repeated placeholder
  apps, and every title keeps a consistent Steam running identity, overlay,
  focus target, and Steam Input profile.
- Serialized shortcut-state mutations so automatic library sync and a stream
  launch cannot overwrite each other or create duplicate runtime entries.
- Normal stream shutdown now also closes the game on the streaming PC and
  tears down the session and virtual display. Unexpected network loss or a
  client crash intentionally keeps the host game alive for safe reconnects.

### Changed

- Native Steam app IDs are indexed directly when matching the host catalogue,
  avoiding ambiguous or localized title-only matches on the game page.
- Steam Deck release notes now include a curated change summary, platform
  download table, verified one-shot installer, and setup-guide link.

### Controller support

- Steam tracks a stable per-game streaming process for controller focus and
  per-title Steam Input configuration. The client continues to forward the
  Steam Deck controller through the native SDL/Limelight gamepad endpoint,
  including buttons, axes, rumble, motion, battery, and supported extended
  controller events.

## 0.2.5-test — 2026-07-21

### Fixed

- Fixed a Steam Deck regression where slow or unreachable host requests could
  block the Decky backend, making the paired host, library sync, game launch,
  and native game-page stream button appear unavailable at the same time.
- The native Steam game-page stream button now appears when the asynchronous
  host catalogue arrives, even if the page was opened during plugin startup.
- Native Steam games available from the paired host now carry a small sun
  badge on their Game Mode library capsule.
- Host availability polling can no longer overlap or starve other plugin
  calls. Busy and offline hosts both use a red indicator; an available and
  idle host (or the selected game already running) uses green. Availability
  never prevents a manual launch attempt.
- Client and Decky package versions now follow the release version instead of
  remaining at `0.1.0`.

### Changed

- The Steam Deck installer now installs or updates both the Decky plugin and
  the latest x86_64 client AppImage in `~/Applications`, including executable
  permissions, from the same GitHub release.

### Project maintenance

- Added a permanent, auditable release procedure covering immutable tags,
  version updates, CI gates, published-asset verification, and `latest`
  release handling.
