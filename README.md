# GRAB

**GRAB** — a standalone, open tool to configure **ROLI** MPE devices directly over
MIDI SysEx, with **no dependency on ROLI Dashboard, ROLI Connect, the ROLI hardware
driver, or any login/servers.** With ROLI winding down, GRAB keeps these
instruments configurable and self-documents their protocol.

> The name nods to the devices it targets: **G**rand, **R**ise, **A**irwave,
> **B**lock (and the rest of the BLOCKS family).

<p align="center">
  <img src="docs/grab-screenshot.png" width="480"
       alt="GRAB configuring a ROLI Seaboard BLOCK over USB-MIDI: 5D sensitivity curves, MIDI settings, pitch and play-mode controls, all read live from the device.">
</p>

<p align="center"><sub>Controls are generated from what the connected device
reports — here a Seaboard BLOCK on fw 1.4.3 over USB.</sub></p>

Built with **JUCE 8** + **CMake** (no Projucer), developed on Windows 10/11 x64
(MSVC). The code is kept portable; the decoder library has no JUCE dependency.

## What it does
- **Reads and writes device configuration** over USB-MIDI or BLE-MIDI — all the
  settings ROLI Dashboard exposes (5D sensitivities, MIDI mode/channels, pitch,
  tracking, pedal, etc.), generated from what the device reports rather than
  hard-coded.
- **Writes persist** (plain `setConfig`; confirmed to flash on fw 1.4.3 — the same
  thing Dashboard does).
- **JSON presets**, multi-device (daisy-chained BLOCKS), transport auto-select.
- **Decodes the protocol offline** from hex dumps or USBPcap/Wireshark/BLE captures.
- Never sends anything destructive (no firmware/reset/program-upload/bootloader).

## Downloads
**Just want to run it?** Grab a prebuilt Windows or macOS build from the
[**Releases**](../../releases/latest) page — no GitHub login needed, and the links
don't expire. Each release has `GRAB-<version>-windows.zip` and
`GRAB-<version>-macos.zip` (each contains the GRAB app plus the `seaprobe` /
`seadecode` command-line tools).

For the very latest commit instead, CI builds every push: **Actions** tab → latest
`build` run → **Artifacts** (`GRAB-windows` / `GRAB-macos`). Artifacts need a login
and expire after 90 days, so prefer a Release unless you want the bleeding edge.

The binaries are **unsigned** (ROLI-independent, no paid signing certs), so the OS
warns on first launch:
- **Windows:** SmartScreen → "More info" → "Run anyway".
- **macOS:** Gatekeeper blocks unsigned downloads. After unzipping, clear the
  quarantine flag once: `xattr -dr com.apple.quarantine GRAB.app`, then open it
  (or right-click → Open the first time).

## Supported devices
| Device | Status |
|---|---|
| Seaboard **Block** | ✅ supported (config read/write, presets) |
| Other **BLOCKS** (Lightpad/Pro, Live, Loop, Touch, LUMI, Airwave, ROLI Piano 49) | ✅ should work (same protocol; config is generated from the device) — needs testing/reports |
| Multiple BLOCKS (DNA-connected cluster) | ✅ detected; per-block selection |
| Seaboard **RISE** / **GRAND** | ⛔ not yet — different protocol; see [RISE-GRAND.md](RISE-GRAND.md) (contributions welcome) |

## Quick start (Windows, VS 2022/2026 + MSVC)
```sh
cmake -S . -B build -G "Visual Studio 18 2026" -A x64   # first run fetches JUCE 8
# GUI:
cmake --build build --config Debug --target GRAB
./build/GRAB_artefacts/Debug/GRAB.exe
# CLI probe / writer:
cmake --build build --config Debug --target seaprobe
./build/seaprobe_artefacts/Debug/seaprobe.exe blocks            # read config
./build/seaprobe_artefacts/Debug/seaprobe.exe set 13 64 --commit # write one value
# offline decoder:
cmake --build build --config Debug --target seadecode
./build/Debug/seadecode.exe F0 00 21 10 77 00 01 01 00 5D F7
```
`--transport usb|ble|auto` and `--serial <S>` select transport / a specific block.
Logs are written to `logs/`.

## Components
- **`GRAB`** — the desktop GUI: connection status, transport + block selectors, one
  control per device config item grouped into sections, Save to device, JSON presets.
- **`seaprobe`** — console tool: list ports, dump raw SysEx, decode topology/config,
  and the gated single-value writer (dry-run by default).
- **`seadecode`** — portable (no-JUCE) offline decoder for ROLI SysEx, USB-MIDI and
  BLE-MIDI captures.
- **`libs/sea_sysex`** — the portable framing/checksum/bit-unpacking decoder library.
- **`third_party/roli_blocks_basics`** — ROLI's open BLOCKS stack, vendored and
  patched for JUCE 8 (see [PATCHES.md](PATCHES.md)).

## Documentation
- [PROTOCOL.md](PROTOCOL.md) — the observed BLOCKS SysEx protocol (framing, checksum,
  config, firmware), each fact marked confirmed-on-device vs from-source.
- [RISE-GRAND.md](RISE-GRAND.md) — notes for adding Seaboard RISE/GRAND support.
- [DEVICE-PROFILES.md](DEVICE-PROFILES.md) — the opt-in "self-learn" device-profile
  schema (contribute what your device reports, to a store at demix.me).
- [PATCHES.md](PATCHES.md) — the vendored-library patches.
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to help, especially new devices.

## Help wanted / known limitations
- **macOS device detection.** GRAB builds and runs on macOS, but transport
  selection uses Windows-style MIDI identifiers (`usb#`, `blemidi`, …). macOS
  CoreMIDI uses different identifiers, so auto-detect needs mac-tuned heuristics
  (in `src/common/LoggingMidi.h`). Great first contribution for a Mac owner.
- **Other BLOCKS** (Lightpad/Pro, Live, Loop, Touch, LUMI, Airwave, ROLI Piano 49)
  should work via the same protocol but need testing — reports/profiles welcome.
- **Seaboard RISE / GRAND** support — see [RISE-GRAND.md](RISE-GRAND.md).

## Contributing
GRAB grows by people with different ROLI hardware reporting what their device does.
The easiest contribution is a **device profile** (one click in the GUI, or a captured
SysEx log) — see [CONTRIBUTING.md](CONTRIBUTING.md). Adding a whole device family
(RISE/GRAND) is a bigger, welcome contribution with a clear starting point in
[RISE-GRAND.md](RISE-GRAND.md).

## Safety
GRAB only sends read/config MIDI. It **never** sends firmware-update,
program-upload, factory-reset, or bootloader messages. It uses the selected MIDI
port exclusively (USB or BLE) and reports — rather than works around — a port held
by another app.

## License
MIT — see [LICENSE](LICENSE). Vendored `roli_blocks_basics` is ISC (ROLI Ltd); JUCE
is fetched at build time under its own license.
