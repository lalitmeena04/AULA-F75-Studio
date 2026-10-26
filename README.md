# AULA F75 Studio

An open-source companion studio app and alternative driver for the 
AULA F75 mechanical keyboard to customize layouts, RGB lighting, and macros: left icon
rail, dark tonal surfaces, a live keyboard canvas.
It speaks the keyboard's own HID protocol, recovered by static analysis of the
vendor driver (`OemDrv.exe`, SINO WEALTH "G5KB" family) with REA/Ghidra. No
vendor code, assets or libraries are included; nothing talks to the network.

## Features

| Page        | What it does                                                                                                                                                                      |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Home        | Device identity, link type, current effect / sleep, quick navigation                                                                                                              |
| Lighting    | 18 hardware effects with brightness / speed / colour; **Custom (per-key)** canvas with brush, fill, erase, multi-select; **Audio-reactive** (WASAPI loopback) and **CPU/GPU heatmap** (PDH + NVML) streamed to the keys |
| Keys        | Remap any key on the Base or Fn layer: capture a shortcut, pick a key, media/app control, mouse button, macro trigger, profile switch; read the layer back from the keyboard       |
| Macros      | Record keystrokes with real timing, edit events/delays, insert mouse clicks, upload up to 32 macros onboard, read back what is stored                                             |
| Power       | Idle sleep timer, receiver status channel (raw), factory reset with confirmation                                                                                                  |
| Profiles    | Local JSON profiles (lighting + per-key colours + key map + macros + sleep) with duplicate / export / import; "Save & apply" bar for unsaved edits                                |
| Debug       | Raw 128-byte config block, every HID frame exchanged, icon set in use                                                                                                             |

Icons come from **Segoe Fluent Icons** (Windows 11) or **Segoe MDL2 Assets**
(Windows 10); when neither is installed the app falls back to **Font Awesome 6
Free** bundled through QtAwesome's font resources. The mapping lives in
`src/IconService.cpp`.

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
| Key-matrix write, macro upload    | Follow the vendor encoding exactly (same buffer format the keyboard returns); **not yet written to a keyboard**                                                              |
| Per-key colour group (persisted) | Encoding from the vendor `ApplySetting` routine (3 × 126-byte planes, 384 bytes); **not yet written to a keyboard**                                                           |
| Live streaming (audio / heatmap) | Verified on hardware: vendor real-time command `0x08` (378 bytes, RGB per LED, transient), 120 frames at 10 fps with the keyboard answering afterwards (`--stream-test`)    |
| Factory reset                     | Verified earlier (command `0x11`)                                                                                                                                          |
| Receiver battery                  | Not recovered                                                                                                                                                              |

## Build

Requires MSVC (Visual Studio 2022+), CMake ≥ 3.24, Ninja and a Qt 6.8+ MSVC
x64 kit. Configure fetches QtAwesome from GitHub (fonts only).

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\msvc2022_64 ^
      -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run with Qt's `bin` on `PATH`, or deploy with `windeployqt --qmldir qml --release build\AulaF75Studio.exe`.

CLI helpers (all read-only unless noted):

- `AulaF75Studio --hid-list` — every HID interface of the keyboard and receiver.
- `AulaF75Studio --probe` — identity + config as JSON.
- `AulaF75Studio --dump` — decoded Base/Fn layers and macro buffer; with
  `F75_REAPPLY=1` it also rewrites the current config unchanged (write-path check).
- `AulaF75Studio --stream-test` — 3 s rainbow sweep through the real-time
  frame command, then a re-probe (transient; changes no stored setting).
- `AulaF75Studio --screenshot-dir DIR` — render every page headlessly.

## Protocol summary

Feature report, 0x208 bytes, report ID `0x06`:
`[id, cmd, sub, 0, pages, page, len_lo, len_hi, payload×512]`; reads send
`cmd|0x80` then `HidD_GetFeature`. Commands: `02` identity, `03` matrix (sub =
layer), `04` config, `05` macros, `06` colour group (sub = group), `08`
real-time frame (transient, used by the vendor's Music mode), `0A` RGB table,
`11` reset.

- **Config** (0x80 B, `5A A5` at 0x7E): `[0x09]` custom mode, `[0x0A]` effect
  hardware ID, `[0x18]` sleep (SleepHW table), slots at `0x3A + 2·i` =
  `brightness, speed<<4 | colour`.
- **Matrix** (4 layers × 128 × u32 LE, trailer `0xA55A0000`): byte 0 = type:
  `00` keyboard `[mods, key2, key1]` (modifier keys are their bit only), `02`
  consumer `[00, hi, lo]`, `03` macro `[mode, repeat, index]`, `05` profile,
  `07`/`08` device functions (KB.ini words byte-swapped), `0D` = Fn.
- **Macro buffer**: N × `{u16 offset, u16 size}` then bodies:
  `[nameLen][UTF-16 name][events: (kind<<4 | delay>>16 | 0x80 release), delay>>8, delay, code]`.
