# AGENTS.md

Guidance for AI coding agents working in this repository.

## Development

- Always keep code generalized.
- Always add or update existing unit tests for any changes made in the code.
- For comments: state facts and use simple language with short sentences. Follow
  softened ASD-STE100 standard and use light caveman mode.
- Add diagnostic logging (`TRC_*` macros from `Logging.h`) to facilitate error logging.
- Never write to the keyboard from a test. Hardware paths stay behind the
  `DeviceController` verify step and the CLI switches.

## Layout

| Path            | Contents                                                              |
| --------------- | --------------------------------------------------------------------- |
| `Model/`        | Pure protocol codec, key layout and profile storage (`aulaf75_core`)  |
| `Controllers/`  | Windows HID / WASAPI / PDH code exposed to QML as context properties  |
| `Views/`        | QML module `AulaF75Studio` (pages, components, `Theme` singleton)     |
| `tests/`        | GoogleTest suite mirroring `Model/`, `Controllers/`, `Views/`         |

`MainControllerClass` owns every subsystem and registers them with
`setContextProperty`. QML pages reference `device`, `hostEffects`,
`lightingPreview`, `gameMode`, `deviceWatcher`, `profileStore` and `appInfo`
directly; do not add `required property` plumbing for them.

## Building

Always build through the root PowerShell script instead of invoking CMake or MSVC
directly:

```powershell
.\aulaf75studio.ps1 build
```

This handles Qt/MSVC environment setup, CMake configure, compile, and Qt runtime
deployment. Useful variants:

- `.\aulaf75studio.ps1 build -Config release` — release build
- `.\aulaf75studio.ps1 build -Clean` — full rebuild
- `.\aulaf75studio.ps1 build -RunTests` — build then run the test suite
- `.\aulaf75studio.ps1 test` — run the unit tests
- `.\aulaf75studio.ps1 run` — launch the app
- `.\aulaf75studio.ps1 help` — list all commands

Format C++ with the repository `.clang-format` before committing.

## Version Control

Never commit or push agent made changes automatically.
