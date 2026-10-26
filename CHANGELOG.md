# Changelog

All notable changes to AULA F75 Studio are documented here. This project follows
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Animated on-screen preview of every built-in lighting effect on the
  Lighting page, following the working profile's effect, brightness, speed
  and colour (`LightingPreview`, `LightingPreviewController`).
- Game mode on the Power page: a host-side lock for the Windows keys, Alt+Tab
  and Alt+F4 (each optional) while the app runs (`GameModeRules`,
  `GameModeController`).
- Hot-plug detection: the keyboard is probed automatically when the USB cable
  is connected or at start-up, and marked absent when unplugged
  (`DeviceWatcher`). Home shows live USB / receiver presence.
- Connections card on the Debug page listing every keyboard and receiver HID
  interface with role, usage page, report sizes and a Rescan button.

### Changed

- Repository restructured into `Model/`, `Controllers/`, `Views/` and `tests/`
  with a root `MainControllerClass` that owns every subsystem and exposes it to
  QML through context properties.
- `Device` renamed to `DeviceController`, `HostEffects` to
  `HostEffectsController`; headless CLI switches moved to `CommandLine`.
- Tests moved to GoogleTest (`aulaf75_gtest`) with new coverage for
  `KeyLayout`, `ProfileStore` and the QML wiring contract.
- Added spdlog file logging (`%AppData%/AulaF75Studio/logs`), `VERSION.txt` as
  the single version source, CMake presets, `.clang-format`, a GitHub build
  workflow and the `aulaf75studio.ps1` build script.

## [0.1.0] — 2026-10-26

Initial release: Qt 6 QML dashboard for the AULA F75 speaking the keyboard's
own HID protocol.

### Added

- Hardware lighting effects, per-key custom canvas, audio-reactive and
  CPU/GPU heatmap streaming through the real-time frame command.
- Key remapping on Base and Fn layers, onboard macros with readback.
- Idle sleep timer, receiver status monitor, factory reset.
- Local JSON profiles with duplicate / export / import.
- Diagnostics page and read-only CLI switches (`--hid-list`, `--probe`,
  `--dump`, `--stream-test`, `--screenshot-dir`).
