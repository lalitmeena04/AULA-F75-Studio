# AULA F75 Studio

An open-source companion studio app and alternative driver for the 
AULA F75 mechanical keyboard to customize layouts, RGB lighting, and macros: left icon
rail, dark tonal surfaces, a live keyboard canvas.
It speaks the keyboard's own HID protocol directly over USB feature reports.
No third-party code, assets or libraries are included; nothing talks to the
network.

## Highlights

| Page        | What it does                                                                                                                                                                      |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Home        | Device identity, link type, current effect / sleep, live USB / receiver presence, quick navigation                                                                              |
| Lighting    | 18 hardware effects with brightness / speed / colour and an **animated on-screen preview**; **Custom (per-key)** canvas with brush, fill, erase, multi-select; **Audio-reactive** (WASAPI loopback) and **CPU/GPU heatmap** (PDH + NVML) streamed to the keys |
| Keys        | Remap any key on the Base or Fn layer: capture a shortcut, pick a key, media/app control, mouse button, macro trigger, profile switch; read the layer back from the keyboard       |
| Macros      | Record keystrokes with real timing, edit events/delays, insert mouse clicks, upload up to 32 macros onboard, read back what is stored                                             |
| Power       | Idle sleep timer, receiver status channel (raw), **Game mode** (host-side lock for Win, Alt+Tab, Alt+F4), factory reset with confirmation                                        |
| Profiles    | Local JSON profiles (lighting + per-key colours + key map + macros + sleep) with duplicate / export / import; "Save & apply" bar for unsaved edits                                |
| Debug       | Connected HID interfaces with rescan, raw 128-byte config block, every HID frame exchanged, icon set in use                                                                        |

The keyboard is detected when plugged in and probed automatically (read-only);
unplugging drops the verified state. The effect preview is a host-side
approximation of each built-in animation so colour, speed and brightness can be
tuned before pressing Apply; the keyboard runs its own engine.

Icons come from **Segoe Fluent Icons** (Windows 11) or **Segoe MDL2 Assets**
(Windows 10); when neither is installed the app falls back to **Font Awesome 6
Free** bundled through QtAwesome's font resources. The mapping lives in
`Controllers/IconService.cpp`.

## Connection model

| Link                       | What works                                                                                                            |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| USB cable (VID 258A:010C)  | Everything. The `FF00/0001` interface with 520-byte feature reports is the configuration channel.                      |
| 2.4 GHz receiver (3554:FA09) | Typing and a 20-byte `FF02/0002` status channel only. The receiver has no configuration interface, so writes need the cable. The app monitors the status channel and shows raw reports; the battery field is not decoded yet. |

Every write is a read–modify–write of the keyboard's own data followed by a
readback; writes are refused until the identity (`03 00 00 00 00 cd`) and the
`5A A5` config signature have been verified over USB.

## Verification status

| Operation                         | Status                                                                                                                                                                     |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Identity / config read            | Verified on hardware (`--probe`)                                                                                                                                           |
| Config write (effect, sleep)      | Verified on hardware: unchanged config re-written and read back (`F75_REAPPLY=1 AulaF75Studio --dump`); effect/sleep writes were verified in the earlier Lean Device Studio build |
| Key-matrix encoding               | Read-verified on hardware: all 81 keys of the base layer and every factory Fn-layer entry decode to the expected words (`--dump`)                                          |
| Key-matrix write, macro upload    | Use the same buffer format the keyboard returns on read; **not yet written to a keyboard**                                                                                  |
| Per-key colour group (persisted) | 3 × 126-byte R/G/B planes (384 bytes); **not yet written to a keyboard**                                                                                                     |
| Live streaming (audio / heatmap) | Verified on hardware: real-time command `0x08` (378 bytes, RGB per LED, transient), 120 frames at 10 fps with the keyboard answering afterwards (`--stream-test`)           |
| Factory reset                     | Verified earlier (command `0x11`)                                                                                                                                          |
| Receiver battery                  | Not decoded yet                                                                                                                                                            |

## Install

There is no installer yet. Build from source (below); the deployed app lands in
`bin\` with the Qt runtime beside it and runs standalone. Logs are written to
`%AppData%\LocalDeviceTools\AulaF75Studio\logs\`, profiles to
`%LocalAppData%\LocalDeviceTools\AulaF75Studio\profiles.json`.

## Build from source

Prerequisites: Visual Studio 2022+ with "Desktop development with C++",
CMake >= 3.21, Ninja, and a Qt 6.8+ MSVC x64 kit. Configure fetches QtAwesome
(fonts only), spdlog and GoogleTest from GitHub.

Always build through the root script; it sets up the MSVC environment, picks a
Qt kit, runs the CMake preset and deploys the Qt runtime into `bin\`:

```powershell
.\aulaf75studio.ps1 bootstrap          # check prerequisites, then build
.\aulaf75studio.ps1 build -RunTests    # debug build + test suite
.\aulaf75studio.ps1 build -Config release
.\aulaf75studio.ps1 test -Filter "ProtocolTest.*"
.\aulaf75studio.ps1 run
.\aulaf75studio.ps1 format             # clang-format every C++ source
.\aulaf75studio.ps1 help
```

The equivalent raw CMake flow uses the presets in `CMakePresets.json`
(`msvc-debug`, `msvc-release`); pass `-DCMAKE_PREFIX_PATH=<Qt kit>` if your Qt
is not at `C:/Qt/6.11.2/msvc2022_64`. `VERSION.txt` is the single version
source; dev builds get a `-<commit count>.<sha>` suffix.

CLI helpers (all read-only unless noted):

- `AulaF75Studio --hid-list` — every HID interface of the keyboard and receiver.
- `AulaF75Studio --probe` — identity + config as JSON.
- `AulaF75Studio --dump` — decoded Base/Fn layers and macro buffer; with
  `F75_REAPPLY=1` it also rewrites the current config unchanged (write-path check).
- `AulaF75Studio --stream-test` — 3 s rainbow sweep through the real-time
  frame command, then a re-probe (transient; changes no stored setting).
- `AulaF75Studio --screenshot-dir DIR` — render every page headlessly.

`.\aulaf75studio.ps1 run -AppArgs "--probe"` runs any of them from `bin\`.

## Project layout

| Path                      | Contents                                                                                                   |
| ------------------------- | ---------------------------------------------------------------------------------------------------------- |
| `Model/`                  | Pure logic with no hardware access: `Protocol` codec, `KeyLayout` table, `ProfileStore` JSON documents, `LightingPreview` effect approximation, `GameModeRules` (`aulaf75_core`) |
| `Controllers/`            | Windows-facing classes exposed to QML: `DeviceController` (HID worker), `HostEffectsController` (WASAPI / PDH / NVML), `LightingPreviewController`, `GameModeController` (low-level keyboard hook), `DeviceWatcher` (hot-plug), `HidTransport`, `IconService`, `CommandLine` |
| `Views/`                  | QML module `AulaF75Studio`: pages, components, `Theme` singleton                                            |
| `MainControllerClass.*`   | Owns every subsystem, registers them as QML context properties (`device`, `hostEffects`, `lightingPreview`, `gameMode`, `deviceWatcher`, `profileStore`, `appInfo`), loads `Main.qml` |
| `Logging.*`               | spdlog setup and the `TRC_*` macros; Qt messages are routed into the same file                              |
| `tests/`                  | GoogleTest suite (`aulaf75_gtest`) mirroring `Model/` and `Views/`, plus the `qml_smoke` CTest             |
| `aulaf75studio.ps1`       | Build / test / run / format / clean entry point                                                             |
| `.github/workflows/`      | CI: debug + release presets on `windows-latest`                                                             |

## Protocol summary

Feature report, 0x208 bytes, report ID `0x06`:
`[id, cmd, sub, 0, pages, page, len_lo, len_hi, payload×512]`; reads send
`cmd|0x80` then `HidD_GetFeature`. Commands: `02` identity, `03` matrix (sub =
layer), `04` config, `05` macros, `06` colour group (sub = group), `08`
real-time frame (transient, used for live streaming), `0A` RGB table,
`11` reset.

- **Config** (0x80 B, `5A A5` at 0x7E): `[0x09]` custom mode, `[0x0A]` effect
  hardware ID, `[0x18]` sleep (SleepHW table), slots at `0x3A + 2·i` =
  `brightness, speed<<4 | colour`.
- **Matrix** (4 layers × 128 × u32 LE, trailer `0xA55A0000`): byte 0 = type:
  `00` keyboard `[mods, key2, key1]` (modifier keys are their bit only), `02`
  consumer `[00, hi, lo]`, `03` macro `[mode, repeat, index]`, `05` profile,
  `07`/`08` device functions (32-bit words, byte-swapped), `0D` = Fn.
- **Macro buffer**: N × `{u16 offset, u16 size}` then bodies:
  `[nameLen][UTF-16 name][events: (kind<<4 | delay>>16 | 0x80 release), delay>>8, delay, code]`.
