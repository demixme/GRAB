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

## What GRAB is — and isn't
**GRAB is a protocol-preservation effort first, a configuration tool second.** With
ROLI winding down, the near-term goal is to reverse-engineer and *document* how these
instruments are configured over MIDI, and to gather that knowledge across the whole
device range — while the hardware and know-how are still around — so ROLI devices stay
usable whatever happens to ROLI's software and servers.

**It is:**
- An open, no-login, no-driver tool that reads and writes a device's **core settings**
  over MIDI SysEx — the things on Dashboard's config panel (5D sensitivities, MIDI
  mode/channels, pitch, tracking, play mode), generated live from what the device reports.
- A growing, community-built **map of the protocol** (the opt-in self-learn store, see
  [DEVICE-PROFILES.md](DEVICE-PROFILES.md)) and an offline **decoder** for captured traffic.

**It is *not* (yet):**
- A drop-in **replacement for ROLI Dashboard.** It deliberately does **not** flash
  firmware, upload Littlefoot programs/apps, or customise lighting — and it doesn't
  reproduce Dashboard's full workflow.
- Finished or polished — expect rough edges, and **only the Seaboard Block is confirmed**
  so far (other BLOCKS should work; RISE/GRAND aren't supported yet).

If you just want to tweak your device's settings today, it does that. If you'd like it to
*grow into* a fuller Dashboard alternative, that's exactly why **contributing a profile**
(one click) matters now: the more devices we capture while we still can, the better any
future tool can be.

## Quick start — download and use it (no building, no ROLI software)
You don't need to build anything or install any ROLI software — just download,
connect, and run.

1. **Download** your platform's build from the
   [**latest release**](../../releases/latest):
   - **Windows** → `GRAB-<version>-windows.zip`
   - **macOS** → `GRAB-<version>-macos.zip` (universal — Apple Silicon + Intel)

   Each zip also includes the `seaprobe` / `seadecode` command-line tools.
2. **Unzip, then clear the "downloaded from the internet" flag** so the OS will open
   the unsigned app (the builds have no paid signing certificates):
   - **Windows:** right-click `GRAB.exe` → Properties → **Unblock** (or, in
     PowerShell from the folder, `Unblock-File .\GRAB.exe`), then double-click it.
     If SmartScreen still warns → **More info** → **Run anyway**.
   - **macOS:** in Terminal from the unzipped folder, `xattr -dr com.apple.quarantine GRAB.app`,
     then double-click **GRAB.app** (or right-click → **Open** the first time).
3. **Connect your device** — USB and Bluetooth both work on Windows and macOS, no
   driver needed:
   - **USB:** just plug it in.
   - **Bluetooth** (optional): on macOS, connect it once via **Audio MIDI Setup →
     Window → Show MIDI Studio → Bluetooth**; after that it's a normal MIDI port. A
     device bonds to one computer at a time, so turn Bluetooth **off** on any other
     machine that's holding it.
4. **Launch GRAB.** It auto-detects the device and shows its settings — 5D
   sensitivity curves, MIDI mode/channel, pitch, play mode, and more. Edit freely;
   **nothing is written until you click Save to device.** (If a port won't open,
   another app or leftover ROLI software is holding it — GRAB tells you.)
5. **Contribute (optional).** Click **Contribute…** to send an **anonymized**
   snapshot of what your device reports — your serial is **hashed**, never sent raw
   — so GRAB can learn and label settings for everyone. Review the exact data first,
   or **Save to file** instead.

GRAB only reads and writes the same settings ROLI Dashboard does; it **never** sends
firmware, reset, or program-upload messages (see [Safety and firmware](#safety-and-firmware)).

*Want the bleeding edge?* CI builds every commit: **Actions** tab → latest `build`
run → **Artifacts** (needs a GitHub login, expires after 90 days — prefer a release
otherwise). *Want to build or improve GRAB?* See [Building from source](#building-from-source)
and [CONTRIBUTING.md](CONTRIBUTING.md).

## What it does
- **Reads and writes device configuration** over USB-MIDI or BLE-MIDI — all the
  settings ROLI Dashboard exposes (5D sensitivities, MIDI mode/channels, pitch,
  tracking, pedal, …), generated from what the device reports rather than hard-coded.
- **Writes persist** to flash (plain `setConfig` — the same thing Dashboard does).
- **Live UI** — controls update as the device reports changes (from the device itself
  or another app), and the control set adapts when a setting like MIDI Mode changes
  which parameters are available — no manual refresh needed. Unsaved edits are kept.
- **JSON presets**, multi-device (daisy-chained BLOCKS), transport auto-select (USB/BLE).
- **Decodes the protocol offline** from hex dumps or USBPcap/Wireshark/BLE captures.

## Supported devices
| Device | Status |
|---|---|
| Seaboard **Block** | ✅ supported (config read/write, presets) |
| Other **BLOCKS** (Lightpad/Pro, Live, Loop, Touch, LUMI, Airwave, ROLI Piano 49) | ✅ should work (same protocol; config is generated from the device) — needs testing/reports |
| Multiple BLOCKS (DNA-connected cluster) | ✅ detected; per-block selection |
| Seaboard **RISE** / **GRAND** | ⛔ not yet — different protocol; see [RISE-GRAND.md](RISE-GRAND.md) and [issue #1](../../issues/1) |

## Building from source
Windows, Visual Studio 2022/2026 + MSVC. Built with **JUCE 8** + **CMake** (no
Projucer); the code is portable and the `sea_sysex` decoder has no JUCE dependency.
macOS/Linux build the same way (CMake picks the native generator). CI builds Windows
and universal macOS on every push.

```sh
cmake -S . -B build -G "Visual Studio 18 2026" -A x64   # first run fetches JUCE 8
# GUI:
cmake --build build --config Debug --target GRAB
./build/GRAB_artefacts/Debug/GRAB.exe
# CLI probe / writer:
cmake --build build --config Debug --target seaprobe
./build/seaprobe_artefacts/Debug/seaprobe.exe blocks             # read config
./build/seaprobe_artefacts/Debug/seaprobe.exe set 13 64 --commit # write one value
# offline decoder:
cmake --build build --config Debug --target seadecode
./build/Debug/seadecode.exe F0 00 21 10 77 00 01 01 00 5D F7
```
`--transport usb|ble|auto` and `--serial <S>` select the transport / a specific
block. The GUI writes logs to a per-user dir (`%APPDATA%\GRAB\logs` on Windows,
`~/Library/Application Support/GRAB/logs` on macOS).

## Components
- **`GRAB`** — the desktop GUI: connection status, transport + device selectors, one
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
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to help, especially with new devices.

## Contributing — and how the "self-learn" store works
GRAB gets better the more devices people point it at. Three easy ways to help, all
**optional** and shown to you before anything is sent:

- **Contribute a device profile** — one click (**Contribute…**) in the GUI. It sends an
  **anonymized** snapshot of exactly what your device reports: its config items, ranges,
  option lists and current values. Your **serial is hashed** (never sent raw); you can
  review the exact JSON first, or **Save to file** instead of sending.
- **Report a connection problem** — **Report…** sends an anonymized diagnostic of the
  MIDI ports GRAB saw and how the connect attempt ended (or opens a prefilled GitHub
  issue). This is how unsupported devices — **Seaboard RISE/GRAND** especially — get the
  data needed to add them.
- **Capture raw MIDI** — **Capture…** opens any port and logs everything a device emits
  (read-only — it sends nothing), then **Submit** it to the store, save it, or open an
  issue. The best way to help reverse-engineer a device GRAB can't talk to yet.
- **Contribute code** — the dashboard/UI and new-device support are open; open a pull
  request. RISE/GRAND is a great start ([RISE-GRAND.md](RISE-GRAND.md) / [issue #1](../../issues/1)).

**What it's for.** With ROLI winding down, the aim is to build a **community map of the
protocol** across the whole range while we still can. Submissions go to a small store at
demix.me, and GRAB then **pulls the community-learned names back** to label settings your
device reports but doesn't name — so everyone's copy of the tool gets smarter as more
people contribute. GRAB never sends audio or anything you play, and nothing it collects
identifies you. See [DEVICE-PROFILES.md](DEVICE-PROFILES.md) for the schema and the exact
data, and [CONTRIBUTING.md](CONTRIBUTING.md) to get involved.

## Safety and firmware
GRAB only sends read/config MIDI over the one selected port. It **never** sends
firmware-update, program-upload, factory-reset, or bootloader messages, and if a port
is held by another app it reports that rather than fighting for it.

On firmware specifically: GRAB **reads and reports** the version (shown in the header
and in a contributed profile) but does **not** download, flash, or distribute
firmware. ROLI's firmware is their copyrighted IP behind authenticated endpoints, and
GRAB is login-free by design. To preserve *your own* device's firmware while ROLI's
servers are still up, see [tools/FIRMWARE-CAPTURE.md](tools/FIRMWARE-CAPTURE.md) (via
your own ROLI account) — keep those binaries for yourself, out of the profile store.

## License
MIT — see [LICENSE](LICENSE). Vendored `roli_blocks_basics` is ISC (ROLI Ltd); JUCE
is fetched at build time under its own license.
