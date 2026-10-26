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

## Supported devices
| Device | Status |
|---|---|
| Seaboard **Block** | ✅ supported (config read/write, presets) |
| Seaboard **RISE 2** | ✅ supported (config read/write, firmware 2.x) — own protocol |
| **ROLI Piano** (49) | ✅ confirmed (config read/write, key colours; firmware 1.5.0, Windows) — same BLOCKS protocol · [PIANO.md](PIANO.md) |
| **LUMI Keys** | 🟡 keyboard-family — connects and shows its basic shared settings; full support (scales, lighting) not done yet — **contributions welcome** |
| Pad & control **BLOCKS** (Lightpad/Pad, Live, Loop, Touch) · **Airwave** | ◻️ out of scope *for now* — they connect and GRAB shows their basic shared settings, but ROLI's per-app/mode config (Note Grid, Fader Block, Drum Block, …) isn't managed by GRAB (see **Scope** below). **Contributions to add them are welcome** |
| Multiple BLOCKS (DNA-connected cluster) | ✅ detected; per-block selection |
| Seaboard **RISE 25/49** (original) | 🟡 read-only — settings are shown (same `0x78` dump as the RISE 2, ~90% decoded) but **Save is disabled** until writes are confirmed on that firmware · [RISE-GRAND.md](RISE-GRAND.md) |
| Seaboard **GRAND** | ⛔ not yet — help add it with one click (see below) · [RISE-GRAND.md](RISE-GRAND.md) · [issue #1](../../issues/1) |

Got an **unsupported** device? Run GRAB, click **Help add device…** → **Run & submit** —
one click, read-only, and it's how the next devices get added.

**Scope.** GRAB focuses on ROLI's **keyboard-family instruments** — the Seaboard Block,
Seaboard RISE / RISE 2, and ROLI Piano (with LUMI Keys a natural next addition) — where a
device's configuration is a fixed parameter set GRAB reads and writes in full. The **pad
and control BLOCKS** (Lightpad/Pad, Live, Loop, Touch) run ROLI's Littlefoot **app** system,
where each app (Note Grid, Fader Block, Drum Block, …) has its own layout and most settings
live; GRAB can read their basic shared settings but doesn't manage apps/modes, and it never
uploads programs. That's where the current scope ends — **not where it has to**: if you'd
like your block fully supported, **contributions are very welcome**. The **Help add
device…** and **Advanced…** capture tools gather exactly what's needed to start, and
[CONTRIBUTING.md](CONTRIBUTING.md) has the details.

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
- Finished or polished — expect rough edges. **Confirmed on real hardware: the Seaboard
  Block, the Seaboard RISE 2, and the ROLI Piano 49** so far. Other BLOCKS (Lightpad, Live,
  Loop, Touch, LUMI, Airwave) connect and show the config they report, but **app- and
  mode-specific features aren't covered yet** (e.g. a Lightpad's Fader Block setup) — those
  need more reports. The **original Seaboard RISE** shows its settings **read-only** (writes
  not confirmed on that firmware yet); the **GRAND** isn't supported yet.

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

## Troubleshooting — my device won't connect
GRAB auto-detects on launch; the **status line at the top** says what it found. Work
through whichever case matches, then (if it's still stuck) send a report — that's how
devices get fixed and added.

**Nothing detected ("No … Seaboard found")**
- **USB:** plug the device in directly — no driver needed; try another cable/port. ROLI
  devices are class-compliant, so it should appear right away.
- Use the **Transport** dropdown (top row: Auto / USB / BLE) to force USB or BLE instead
  of Auto, then press **Reload from device**.

**Bluetooth won't connect**
- **macOS:** BLE-MIDI devices do *not* appear in System Settings → Bluetooth. Connect
  once via **Audio MIDI Setup → Window → Show MIDI Studio → the Bluetooth icon → Connect**.
  After that it's a normal MIDI port — set **Transport → BLE** (or Auto) and **Reload from device**.
- **Windows:** pair the device first in **Settings → Bluetooth & devices**.
- A ROLI device **bonds to one computer at a time** — if it won't advertise, turn
  Bluetooth **off** on any other machine (or in ROLI software) holding it, then
  power-cycle the device and try again.

**Stuck on "Connecting…" or "no BLOCKS device replied"**
A port opened but the device never answered — usually one of:
- **Another app holds the port** — ROLI Dashboard/Connect, a DAW, or another MIDI app.
  Quit it and reconnect (GRAB reports a held port rather than fighting for it).
- **Windows: ROLI's background service holds the port.** If ROLI Connect/Dashboard is
  installed, the **"ROLI Hardware Driver"** Windows service can keep the port open. Stop
  that service (or uninstall Connect) and reconnect. GRAB itself needs **no** ROLI driver.
- **It's a Seaboard RISE** — RISE uses a different protocol (GRAB shows a separate RISE
  page). The **RISE 2** is fully supported; the **original RISE 25/49** shows its settings
  **read-only** (Save disabled until writes are confirmed on that firmware). The **GRAND**
  isn't supported yet — GRAB says so in the status line; please help add it.
- Try the other **Transport** option (USB ↔ BLE).

**Connected, but a setting looks missing / unnamed / wrong** — contribute a profile
(below); the shared name map improves it for everyone.

### How to help or report — all opt-in, shown before anything is sent
Your serial, if included, is hashed (or blanked in raw MIDI and logs); each dialog also offers **Save to file** or **Open a
prefilled GitHub issue** instead of submitting.

- ⭐ **Unsupported device? Click "Help add device…"** → **Run & submit**. The easiest way
  to help: GRAB auto-picks the device's port, sends a **read-only** "who are you?"
  request, records the reply + a few seconds of what it emits, and submits it all in one
  go. **It never changes a setting.** This is the one-click path for RISE/GRAND owners.
- **Contribute a profile** — if it *did* connect, **Contribute…** → **Submit** (the config
  your device reports).
- **Advanced… menu** (for the technically-minded): **Raw MIDI capture** (log everything a
  device emits while you play; read-only) and **Active probe** (send one curated,
  documented message and log the reply — firmware/reset messages are blocked). Both can
  Submit, Save, or open a prefilled GitHub issue.

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
  and the gated single-value writer (dry-run by default). `seaprobe rise2 ...` does the
  same for a Seaboard RISE 2.
- **`seadecode`** — portable (no-JUCE) offline decoder for ROLI SysEx, USB-MIDI and
  BLE-MIDI captures.
- **`libs/sea_sysex`** — the portable framing/checksum/bit-unpacking decoder library.
- **`third_party/roli_blocks_basics`** — ROLI's open BLOCKS stack, vendored and
  patched for JUCE 8 (see [PATCHES.md](PATCHES.md)).

## Documentation
- [PROTOCOL.md](PROTOCOL.md) — the observed BLOCKS SysEx protocol (framing, checksum,
  config, firmware), each fact marked confirmed-on-device vs from-source.
- [RISE-GRAND.md](RISE-GRAND.md): the Seaboard RISE 2 protocol and notes for adding the GRAND.
- [PIANO.md](PIANO.md) — ROLI Piano 49 notes: connecting on Windows + the config items it reports.
- [DEVICE-PROFILES.md](DEVICE-PROFILES.md) — the opt-in "self-learn" device-profile
  schema (contribute what your device reports, to a store at demix.me).
- [PATCHES.md](PATCHES.md) — the vendored-library patches.
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to help, especially with new devices.

## Contributing — and how the "self-learn" store works
GRAB gets better the more devices people point it at. Several ways to help, all
**optional** and shown to you before anything is sent:

- ⭐ **Help add an unsupported device — one click.** If GRAB says your device isn't
  supported (RISE/GRAND), click **Help add device…** → **Run & submit**. GRAB auto-picks
  the port, sends one **read-only** identity request, records the reply + a few seconds of
  what the device emits, and submits it — **without changing any setting**. The simplest
  possible contribution.
- **Contribute a device profile** — **Contribute…** sends an **anonymized** snapshot of
  what your (supported) device reports: config items, ranges, options, values. Your
  **serial is hashed** (never sent raw); review the JSON first, or **Save to file**.
- **Dig deeper (advanced)** — the **Advanced…** menu has two optional tools: **Raw MIDI
  capture** logs everything a device emits (read-only), and **Active probe** sends one
  *curated, documented* message and logs the reply, to confirm an unsupported device's
  control protocol. Both offer Submit / Save / open-an-issue; firmware/reset/packed
  messages are hard-blocked and you must Arm before each probe send.
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
