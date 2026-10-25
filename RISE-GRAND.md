# Seaboard RISE / GRAND — protocol notes for contributors

**Status: Seaboard RISE 2 is supported** (config read and write, verified on a RISE 2
with firmware 2.0.2 on macOS). The original RISE 25/49 and the GRAND are not supported
yet. The RISE 2 protocol below is confirmed on that device, the rest of this document
collects what is known so far for the other models.

## How RISE/GRAND differ from BLOCKS
- `roli_blocks_basics` (the library this tool is built on) has **no RISE/GRAND
  device type** — ROLI Dashboard drives them through a *separate* code path, not
  the BLOCKS packed protocol (`F0 00 21 10 77 …`).
- Community reverse-engineering of the **older RISE** (2016, RISE 25) shows a
  simpler, un-packed SysEx dialect under namespace `0x78`:
  `F0 00 21 10 78 3D <param> <value> F7`
  (see Sources). It is **not** bit-packed and appears to have no checksum.
- The **RISE 2** uses the same `0x78` dialect, extended. Its full encoding is
  in the next section. The **GRAND** probably uses it too, unconfirmed.

## Seaboard RISE 2 protocol (confirmed on device)
Device: Seaboard RISE 2, firmware 2.0.2 (main), 2.0.2 (second image), Bluetooth module
0.0.2, USB port name `Seaboard RISE 2`. Method: ROLI Dashboard 4.7.2 was driven one
setting at a time while its MIDI output was recorded, then every setting was written
from `seaprobe rise2`, read back and restored. The `0x78` handling was cross-checked in
the decoded firmware image.

All SysEx is `F0 00 21 10 78 <cmd> ... F7`, plain 7-bit, no checksum.

| cmd | dir | meaning |
|---|---|---|
| `3F` | host to device | identify |
| `3A` | device to host | identify reply, 224 bytes, carries the **live settings** (see State dump) |
| `3C <p>` | host to device | read one parameter. Only `28` (battery) and `29` are answered |
| `3E <p> <v>` | device to host | reply to `3C` |
| `3D <p> <v>` | host to device | set one parameter |

The firmware handles nothing else in the `0x78` namespace. Settings written with `3D`, NRPN
and RPN are kept across a power cycle.

### Set parameter `3D <p> <v>`
| p | setting | values |
|---|---|---|
| `08` | Pedal CC | CC number |
| `0E` | MPE on | 1 MPE, 0 Single or Multi Channel (sent as part of the MIDI mode block below) |
| `17` / `18` / `19` / `1A` / `1B` | Glide / Slide / Press / Strike / Lift sensitivity | 0..127 |
| `1C` / `1D` / `1E` | left / middle / right fader CC | CC number |
| `23` / `24` | XY pad X / Y CC | CC number |
| `26 <lo> <hi>` | Sleep Time | 14-bit, Dashboard steps by 60 (unit not confirmed) |
| `38` | Slide CC | CC number |
| `39` | Slide Mode | 0 Absolute, 1 Relative Unipolar, 2 Relative Bipolar |
| `3A` / `3B` | Fixed Velocity / its value | 0..1 / 1..127 |
| `46` / `47` | Strike curve min / max | 1..127 / 0..127 |
| `48` / `49` | Press curve min / max | 0..127 |
| `4A` / `4B` | Lift curve min / max | 0..127 |
| `4C` | Semitone Legato | 0..1 |
| `4D` | MIDI Port Forwarding | 0..1 |
| `4E` | MIDI Port Throttle | 0 Off, 1 Half, 2 Quarter, 3 Note On/Off |

Dashboard also sends `0A` and `0C` with the pedal CC and `25` with the XY CCs. The
firmware ignores `0A` and `0C`. It also handles params Dashboard never sends (`10`..`12`
fader modes, `14`..`16` fader values and a few unknown ones), GRAB does not use them.

### NRPN and RPN
NRPN on channel 1: `B0 63 00  B0 62 <n>  B0 06 <v>  B0 26 00`.

| NRPN | setting | values |
|---|---|---|
| 0 and 7 | Glide and Slide tracking (Dashboard sets both) | 1 Last Played, 2 Highest, 3 Lowest |
| 4 | Press tracking | 0 Poly Aftertouch, 1 Last Played (Channel Pressure), 2 Highest, 3 Lowest |
| 2 | Octave | octave + 4 |
| 3 | Transpose | semitones + 64 |

MIDI mode is a block: `3D 0E <mpe>`, then RPN 6 (MPE configuration) and RPN 0 (pitch
bend range), each with data entry LSB 0.
- **MPE**: RPN 6 = member count on channels 2 and 1 (lower zone) or 15 and 16 (upper
  zone), then RPN 0 = bend range on the same two channels.
- **Multi Channel**: RPN 6 = 16 and RPN 0 on channel 1.
- **Single Channel**: RPN 6 = 1 and RPN 0 on the chosen channel.

### State dump (identify reply)
The `3A` reply is the device's settings struct copied out raw, so GRAB reads the real
state on connect. Offsets count from `F0`, values are stored as sent.

| offset | content |
|---|---|
| 6..13 | `RISE1.00` (family and format, a RISE 2 reports this too) |
| 22 / 30 / 38 | firmware versions: main, second image, Bluetooth module |
| 46..62 / 63..74 | MAC / serial (GRAB does not send these anywhere) |
| 79 / 80 / 81 | first member, last member and master channel (0-based) |
| 82 | MPE on |
| 83 | pitch bend range |
| 84 | octave + 4 |
| 85 / 149 | glide / slide tracking |
| 86 | press tracking |
| 88 | pedal CC |
| 92 / 96 / 100 | fader CCs |
| 102 / 103 | XY CCs |
| 105 / 106 / 107 / 108 / 148 | Glide / Slide / Press / Strike / Lift sensitivity |
| 111 / 112 | sleep time low / high 7 bits |
| 150 / 151 | slide CC / slide mode |
| 152 / 153 | fixed velocity / value |
| 154..159 | Strike, Press, Lift min / max |
| 160 / 161 / 162 | semitone legato / port forwarding / port throttle |
| 207 | transpose + 64 |
| 209 | battery level 0..7 (same as `3C 28`) |

Single Channel mode shows as first = last = master. Multi Channel only clears 82.

### Battery
Battery level 0..7 comes from the Bluetooth module (an ASCII `BAT <n>` message inside the
device) and is readable with `3C 28` or at offset 209. Dashboard polls `3C 28` once a
minute. No charging flag was found.

### 5D curves in Dashboard
Dashboard draws each dimension from its sensitivity only (there is no curve shape
parameter on the wire): Strike, Press and Lift a curve reaching max at s / 127 of the
width, Slide an S-curve around the centre, Glide the same on its side. All straighten into
the diagonal towards 127. At 0 Strike draws a line at max, Lift a line at min, Press and
Slide nothing. `CurveComponent` reproduces these.

### Trying it
`seaprobe rise2 identify | battery | set <name> <value> | octave <n> | transpose <n> |
glide_tracking <n> | press_tracking <n> | sleep_time <n> | mode ...` prints the exact bytes
and only sends them with `--commit`. It accepts only the settings above, in the ranges
Dashboard uses.

## Dashboard UI layout (observed on a RISE 2)
ROLI Dashboard has a separate **RISE / GRAND** tab. Its editor is the *same shape* as
the BLOCKS editor GRAB already builds — **5D Touch** curves, **MIDI Settings**, **Pitch**
— plus one section BLOCKS devices don't have: **MIDI CC Assignments**. That extra
section maps the RISE/GRAND's additional left-hand hardware controllers to CC numbers:
- three touch **faders** → `TouchFader1/2/3CC` (seen on a RISE 2: 107 / 109 / 111),
- the **XY pad** → `XYPadXaxisCC` / `XYPadYaxisCC` (113 / 114),
- the **pedal input(s)** → `PedalInputCC*` (64 Sustain Pedal).

GRAB's RISE page follows the same layout: the curve panels and rows from the BLOCKS page,
plus a MIDI CC Assignments section. The wire encoding is in the RISE 2 section above.

## Confirmed from live devices (first reports, Oct 2026)
RISE devices are **class-compliant USB-MIDI** (no driver), with both an input and an
**output** port — so active probing (host → device) is possible as well as listening.
GRAB detects them by name, flags them RISE/GRAND, and times out with guidance (they
aren't BLOCKS devices). Confirmed so far:

| Model | OS seen | MIDI port name | USB IDs |
|---|---|---|---|
| **Seaboard RISE 49** (original, 2016) | Windows 11 | `ROLI Seaboard RISE 49` | `vid_2af4 pid_0210` |
| **Seaboard RISE** (likely RISE 2) | macOS | `Seaboard RISE` | (CoreMIDI UID only) |
| **Seaboard RISE 2**, firmware 2.0.2 | macOS | `Seaboard RISE 2` | (CoreMIDI UID only) |

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
observe the reply). On the RISE 2 the identify request returns every setting, see the
State dump above. GRAB can't use `roli_blocks_basics` for RISE at all.

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
For the RISE 2 the on-wire encoding is confirmed above. For the GRAND it still needs a
capture.

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

These are from one user's 2016 investigation of a RISE 25. On the RISE 2 the `3D`
params above match, but pressure mode is **NRPN** 4 (not RPN 4) and channel allocation is
the MPE RPN 6 block described above.

## How to add the GRAND or an older RISE
1. Connect the device and run ROLI Dashboard. Capture its MIDI as you change one
   setting at a time (e.g. move each slider / toggle each option). RISE uses plain
   SysEx, so a MIDI monitor or `seaprobe raw` (if you can see the port) reveals the
   exact `<param> <value>` bytes per setting. (USBPcap works too; our
   `tools/decode-capture.sh` auto-detects USB vs BLE.)
2. Compare with the RISE 2 tables above. `seaprobe rise2 identify` shows whether the
   device answers the identify request and how long the reply is.
3. Extend `src/common/Rise2Protocol.h` and `src/gui/Rise2Component.cpp`. The RISE page
   only shows settings for firmware 2.x until another layout is confirmed.
4. Please re-verify every value on your firmware and note the model + firmware
   version in the PR.

## Sources
- Reverse-engineering the Roli Seaboard (2016): https://chrissieviolin.wordpress.com/2016/05/19/reverse-engineering-the-roli-seaboard/
- ROLI Dashboard `.mode` preset files (local install), decoded here.
