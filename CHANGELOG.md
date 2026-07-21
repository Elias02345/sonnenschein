# Changelog

All notable user-facing changes to Sonnenschein are documented here.

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
