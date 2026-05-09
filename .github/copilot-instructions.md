# Copilot / AI-Coder Hinweise

Sonnenschein ist Linux-first. Bauen passiert auf Linux (oder WSL2 auf Windows).

## Build

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j$(nproc)`
- Build-Ordner: `build/`. Bitte nicht committen — ist in `.gitignore`.

## Tests

- gtest. Testbinary: `build/tests/test_sonnenschein` (in Phase 1 noch `test_apollo`/`test_sunshine`).
- Auf Linux ausführen, nicht auf Windows.

## Stil

- C/C++: `.clang-format` ist verbindlich. `clang-format-18 -i <file>` vor PR.
- Bash: strict mode (`set -euo pipefail`). `shellcheck` muss grün sein.
- Vue/JS: `prettier` mit der Projekt-Config.

## Was Sonnenschein neu macht

Linux-Port des Apollo-Killer-Features (Auto-Match Virtual Display) plus Wayland-HDR plus distroübergreifender Installer. Code dafür entsteht in `src/platform/linux/virtual_display/`, `installer/`, und einer neuen Vue-3-WebUI in `src_assets/common/assets/web/`.

## Was NICHT zu tun ist

- **Kein Windows-Code**. Wir behalten `src/platform/windows/` als Erbe, aber bauen es nicht. Die Apollo-Maintainer pflegen Windows.
- **Keine spekulativen Refactorings**. Apollo/Sunshine ist groß. Halte die Diff klein, mache eine Sache pro PR.
- **Kein Bypass von Sicherheits-Mechanismen** (CAP_SYS_ADMIN etc.) — wenn etwas Capabilities braucht, ist das beabsichtigt.
