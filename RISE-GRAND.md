# Seaboard RISE / GRAND — protocol notes for contributors

**Status: not yet supported by this tool.** This document collects what we know so
far, so someone who owns a RISE or GRAND can add support. Our Block tooling
(`libs/sea_sysex`, port selection in `src/common/LoggingMidi.h`, the capture
scripts in `tools/`) is reusable; what's missing is the RISE/GRAND wire encoding.

## How RISE/GRAND differ from BLOCKS
- `roli_blocks_basics` (the library this tool is built on) has **no RISE/GRAND
  device type** — ROLI Dashboard drives them through a *separate* code path, not
  the BLOCKS packed protocol (`F0 00 21 10 77 …`).
- Community reverse-engineering of the **older RISE** (2016, RISE 25) shows a
  simpler, un-packed SysEx dialect under namespace `0x78`:
  `F0 00 21 10 78 3D <param> <value> F7`
  (see Sources). It is **not** bit-packed and appears to have no checksum.
- The **RISE 2** (2024) and **GRAND** likely use an evolved version of that
  dialect. The exact RISE 2 encoding is compiled into ROLI Dashboard's native
  code (packets are built dynamically — not extractable as static strings), so
  **confirming it needs one capture** of Dashboard changing a setting. Because
  it's plain SysEx (not bit-packed), capturing/decoding is *easier* than for a
  Block.

## Dashboard UI layout (observed on a RISE 2)
ROLI Dashboard has a separate **RISE / GRAND** tab. Its editor is the *same shape* as
the BLOCKS editor GRAB already builds — **5D Touch** curves, **MIDI Settings**, **Pitch**
— plus one section BLOCKS devices don't have: **MIDI CC Assignments**. That extra
section maps the RISE/GRAND's additional left-hand hardware controllers to CC numbers:
- three touch **faders** → `TouchFader1/2/3CC` (seen on a RISE 2: 107 / 109 / 111),
- the **XY pad** → `XYPadXaxisCC` / `XYPadYaxisCC` (113 / 114),
- the **pedal input(s)** → `PedalInputCC*` (64 Sustain Pedal).

So adding RISE/GRAND to GRAB is largely **the existing generated layout + a new "MIDI
CC Assignments" section** for those controllers. The screenshot values match the
`.mode`-derived table below, so those defaults are confirmed against a live device's
UI — what still needs a capture is the on-wire `<param>` encoding.

## Confirmed from live devices (first reports, Oct 2026)
RISE devices are **class-compliant USB-MIDI** (no driver), with both an input and an
**output** port — so active probing (host → device) is possible as well as listening.
GRAB detects them by name, flags them RISE/GRAND, and times out with guidance (they
aren't BLOCKS devices). Confirmed so far:

| Model | OS seen | MIDI port name | USB IDs |
|---|---|---|---|
| **Seaboard RISE 49** (original, 2016) | Windows 11 | `ROLI Seaboard RISE 49` | `vid_2af4 pid_0210` |
| **Seaboard RISE** (likely RISE 2) | macOS | `Seaboard RISE` | (CoreMIDI UID only) |

Vendor `2af4` is shared across ROLI devices (the Seaboard **Block** is `vid_2af4 pid_0700`);
the PID identifies the model. The **original RISE 49** is the best starting point — it uses
the older, documented `0x78` dialect below (vs the RISE 2's compiled-in encoding).

**To capture one:** in GRAB, **Capture…** → pick `Seaboard RISE` → **Start** → play and
operate the device (and change settings on it) → **Save to file** → attach the `.txt` to
an issue. That recording is the raw material for adding support; the `<param>` encoding
in the tables below still needs confirming against such a capture.

## Confirmed from captures (RISE 25 **and** RISE 2, playing): performance = plain MPE
Captures of both a **RISE 25** and a **RISE 2** being played contained **only standard
channel-voice MIDI — no SysEx at all**:
- Note On = strike, per-channel **pitch bend** = glide, **CC 74** = slide (Y),
  **channel pressure** = press (Z), spread across MPE member channels.

This means a RISE already works as an MPE controller in any host **without GRAB** — there
is nothing to reverse-engineer on the performance side. GRAB's value for RISE is purely
**configuration** (the host-initiated side below).

So (unlike a BLOCK, which also streams the `0x77` packed touch protocol) the RISE
emits **plain MPE** for performance, and **no config SysEx while playing**. Config is
therefore **host-initiated only** (the `0x78` dialect) — the device won't volunteer it,
so reading/confirming settings requires **active probing** (send a documented request,
observe the reply). GRAB can't use `roli_blocks_basics` for RISE at all.

GRAB has an **Active probe** tool for exactly this (the **Probe…** button): pick the
device's output + input, choose a curated message (the MIDI Identity Request, or one of
the documented `0x78` writes below with a value), review the exact bytes, tick **Arm**,
and **Send** — the reply is logged and can be submitted/saved. Firmware/reset/packed
messages are hard-blocked. Confirming each `0x78` `<param>` this way is how RISE support
gets built.

**Start with the Identity Request** (the default, read-only probe). Its reply — or
silence — is the decision point: if the RISE answers SysEx there's a query channel to
explore; if it's silent, config is likely via **RPN** (standard CCs, see the old-RISE
notes below) or only visible by capturing ROLI Dashboard, not by probing.

## Config parameters (from ROLI Dashboard's preset files)
These are taken from ROLI Dashboard's `.mode` preset files
(`…/ROLI Dashboard/Littlefoot/{Rise,Grand}.littlefootModes/*.mode`). They are the
host-side variable names + per-preset defaults — the semantic set to implement.
The on-wire `<param>` byte for each still needs confirming by capture.

### Seaboard RISE (RISE 2)
| Dashboard variable | Example value | Equivalent BLOCKS config id |
|---|---|---|
| `MIDI Mode` (Single Channel / Multi Channel / MPE) | MPE | midiUseMPE (2) |
| `MPEZone` (Lower / Upper) | Lower | MPEZone (40) |
| `MPEChannelRange` | 15 | midiChannelRange (9) |
| `MIDIStartChannel` / `MIDIEndChannel` | 2 / 16 | 0 / 1 |
| `GlideAndSlideTrackingMode` | Last Played | xTrackingMode/yTrackingMode (30/31) |
| `pressMode` (Channel Pressure / Poly Aftertouch) | Channel Pressure | *(RISE-specific; cf. zTracking 32)* |
| `PressureTrackingMode` | Last Played | zTrackingMode (32) |
| `Octave` / `Transpose` / `PitchBendRange` | 0 / 0 / 48 | 4 / 5 / 3 |
| `StrikeSensitivity` + `…Min`/`…Max` | 101 / 0 / 127 | velocitySensitivity (10) + velocityMin/Max (50/51) |
| `GlideSensitivity` | 127 | glideSensitivity (11) |
| `SlideSensitivity` | 127 | slideSensitivity (12) |
| `PressureSensitivity` + `…Min`/`…Max` | 127 / 0 / 127 | pressureSensitivity (13) + pressMin/Max (52/53) |
| `LiftSensitivity` + `…Min`/`…Max` | 127 / 0 / 127 | liftSensitivity (14) + liftMin/Max (54/55) |
| `SlideCC` / `SlideMode` | 74 / Absolute | slideCC (6) / slideMode (7) |
| `FixedVelocity` | false | fixedVelocity (15) |
| `SemitoneLegato` | true | *(RISE-specific)* |
| `TouchFader1CC` / `TouchFader2CC` / `TouchFader3CC` | 107 / 109 / 111 | *(RISE-specific — the 3 sliders)* |
| `XYPadXaxisCC` / `XYPadYaxisCC` | 113 / 114 | *(RISE-specific — the XY pad)* |
| `PedalInputCC1` | 64 Sustain Pedal | pedalCC (47) |
| `SleepTime` (seen in UI "General") | 0 | *(RISE-specific)* |

### Seaboard GRAND
Smaller exposed set (it's a different form factor with three pedals):
| Dashboard variable | Example value | Notes |
|---|---|---|
| `UseMPE` | true/false | MIDI mode |
| `MIDIStartChannel` / `MIDIEndChannel` | 2 / 16 | |
| `GlideTrackingMode` / `PressureTrackingMode` | Last Played | |
| `pressMode` | Channel Pressure | Poly Aftertouch / Channel Pressure |
| `Octave` / `Transpose` / `PitchBendRange` | 0 / 0 / 48 | |
| `GlideSensitivity` | 115 | (only Glide sensitivity is exposed) |
| `PedalInputCC1/2/3` + `PedalSwitch1/2/3` | 60 / 67 / 66, Switch (auto-detect) | three pedal inputs |

## Known old-RISE SysEx (RISE 25, 2016 — verify before trusting)
Namespace `0x78`, form `F0 00 21 10 78 3D <param> <value> F7`:

| Function | param | value |
|---|---|---|
| MPE mode | `0x0E` | 1=on, 0=off |
| Glide curve | `0x17` | 00=off … 7F=linear |
| Slide curve | `0x18` | 〃 |
| Pressure curve | `0x19` | 〃 |
| Strike curve | `0x1A` | 〃 |
| Lift curve | `0x1B` | 〃 |

Plus RPN (standard CC messages, not SysEx):
- Pressure mode: RPN address 4, MSB 0=PolyPressure, 1=Channel Pressure (last),
  2=(highest), 3=(lowest), 4=(disabled).
- Channel allocation: RPN address 6, MSB = (number of channels − 1), on the
  lowest channel in the range.

These are from one user's 2016 investigation of a RISE 25 and may not match RISE 2
or current firmware.

## How to add RISE/GRAND support
1. Connect the device and run ROLI Dashboard. Capture its MIDI as you change one
   setting at a time (e.g. move each slider / toggle each option). RISE uses plain
   SysEx, so a MIDI monitor or `seaprobe raw` (if you can see the port) reveals the
   exact `<param> <value>` bytes per setting. (USBPcap works too; our
   `tools/decode-capture.sh` auto-detects USB vs BLE.)
2. Map each Dashboard variable above to its confirmed SysEx/RPN message.
3. Add a RISE/GRAND module that enumerates these as config items (reuse the
   generated-UI approach), and wire it into transport selection.
4. Please re-verify every value on your firmware and note the model + firmware
   version in the PR.

## Sources
- Reverse-engineering the Roli Seaboard (2016): https://chrissieviolin.wordpress.com/2016/05/19/reverse-engineering-the-roli-seaboard/
- ROLI Dashboard `.mode` preset files (local install), decoded here.
