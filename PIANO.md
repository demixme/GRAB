# ROLI Piano (49) — notes

The **ROLI Piano 49** speaks the **same BLOCKS protocol** as the Seaboard Block
(`F0 00 21 10 77 …`), so GRAB talks to it with the vendored `roli_blocks_basics`
library — no new protocol code. Serial prefix is `RP4`.

**Confirmed working** over USB on Windows, firmware **1.5.0**: GRAB reads ~36 config
items (33–34 visible depending on MIDI mode), writes a single `setConfig` per changed
item, and the change **persists across a power cycle**. Key colours can be written too.
(First reported by a contributor — thanks.)

## Getting it to connect (Windows)

GRAB needs no ROLI driver, but the Piano's port has to be free and enumerated:

- **Close ROLI's background service first.** If ROLI Connect/Dashboard is installed, the
  **"ROLI Hardware Driver"** Windows service holds the port and GRAB can't open it. Stop
  that service (or uninstall Connect, which installs it).
- **Without any ROLI driver:** Windows' standard USB-audio driver may fail to start on
  the Piano (Device Manager **code 10**). On **Windows 11 25H2+** (where Windows MIDI Services
  is built in), pick the in-box **"USB MIDI 2.0"** driver manually in Device Manager — the port then shows
  up as **"USBMidi2-ACX"** (rename it in your DAW). MPE and per-note pressure work, and a
  DAW and GRAB can use the Piano at the same time.

GRAB recognises the Piano by name (`roli piano`) or port identifier (`pianofortynine`
with ROLI's driver, `vid_2af4&pid_0f00` with the in-box driver) — see
`src/common/LoggingMidi.h`, `isBlocksDevice()`.

## It's a hybrid: it answers both protocols

A **Help add device…** / auto-explore capture of a Piano (contributed via the store)
shows it is a dual-protocol device:

- It **answers the ROLI identify** (`F0 00 21 10 78 3F F7`) with a **RISE-style state
  dump** (`F0 00 21 10 78 3A …`, a ~217-byte `0x78` serialDump) — the same message a
  Seaboard RISE 2 returns. The dump begins with the firmware string **`RISE1.00`**,
  then the BLE/MAC address, then the serial (`RP4…`), then binary state bytes in the
  same layout as the RISE 2 dump (see [RISE-GRAND.md](RISE-GRAND.md)).
- It **also streams the `0x77` BLOCKS packed protocol** and connects over the normal
  BLOCKS topology — which is how GRAB reads/writes its config today.

So both views of the Piano are real: GRAB drives it as a BLOCKS device (`0x77`,
`setConfig`), and the Piano additionally exposes the RISE-family `0x78` state dump. (A
plain **Seaboard Block**, by contrast, answers *neither* raw identity request — it only
talks over the `0x77` topology handshake.)

## Key colours

Items **34** (Global Key Color) and **35** (Root Key Color) are written as **32-bit
ARGB** (e.g. `0xFF3D0707`); the keys change immediately.

## Previewing without the hardware (dev)

GRAB has secret dev shortcuts (no device needed) to render a device page from synthetic data,
handy for working on UI for instruments we don't own:
- **Ctrl+Alt+Shift+P** — simulated **ROLI Piano** config page (the items in the table below,
  incl. the named **Scale** dropdown and **Color Scheme**). Press again to exit.
- **Ctrl+Alt+Shift+L** — simulated **Piano M / LUMI Keys** page (same items minus Glide and the
  pedal, octave -4..5, four modes' Color Scheme — the editable key-colour picker shows here too).
  Press again to exit.
- **Ctrl+Alt+Shift+G** — **GRAND** page preview on the RISE component (4D, 3 pedals, full settings),
  from a synthetic 0x3a reply; see [RISE-GRAND.md](RISE-GRAND.md). Press again to exit.
- **Ctrl+Alt+Shift+R** — RISE page preview (toggles RISE 2 / original RISE).

## Scales & scale lighting

**Scale** (config id **22**, range 0..18) selects a musical scale, but the device reports
**no names** for the values — the library even flags it *"Should be options"*. The names and
note masks below come from ROLI Dashboard's own LittleFoot library (`Scale.littlefoot`), in
device order, and were contributed and tested on a **ROLI Piano 49 (fw 1.5.0)**. GRAB names
these in [src/common/ConfigOverrides.h](src/common/ConfigOverrides.h) (needs `numOptionNames`
≥ 19 — raised to 20 in the vendored library, patch **P3** in [PATCHES.md](PATCHES.md)).

The mask is a 12-bit set of the semitones in the scale: **bit n set ⇒ note n semitones above
the root is in the scale** (e.g. Major `0xab5` = semitones 0,2,4,5,7,9,11).

| # | Scale | Mask | | # | Scale | Mask |
|---|---|---|---|---|---|---|
| 0 | Major | 0xab5 | | 10 | Mixolydian | 0x6b5 |
| 1 | Minor | 0x5ad | | 11 | Locrian | 0x56b |
| 2 | Harmonic Minor | 0x9ad | | 12 | Whole Tone | 0x555 |
| 3 | Pentatonic Neutral | 0x4a5 | | 13 | Arabic (A) | 0xb6d |
| 4 | Pentatonic Major | 0x295 | | 14 | Arabic (B) | 0x575 |
| 5 | Pentatonic Minor | 0x4a9 | | 15 | Japanese | 0x8d1 |
| 6 | Blues | 0x4e9 | | 16 | Ryukyu | 0x8b1 |
| 7 | Dorian | 0x6ad | | 17 | 8-Tone Spanish | 0x57b |
| 8 | Phrygian | 0x5ab | | 18 | Chromatic | 0xfff |
| 9 | Lydian | 0xad5 | | | | |

**Scale lighting is native to the Piano's firmware — no LittleFoot program needed.** Set the
**active mode's Color Scheme** (ids 100/107/114/121) to **User** (value **1**) and the Piano
lights only the keys of the selected **Scale** (22) relative to the root **Key** (27); moving
Scale changes the lit keys live, and **Chromatic** (18) lights every key. In the default
Major / C / User setup this just looks like "white keys lit, black keys off". GRAB already
exposes Color Scheme as a named dropdown (0 Pro, 1 User, 2 Piano, 3 Stage, 4 Rainbow), so
Scale-selection + Color Scheme = User gives scale lighting with plain `setConfig` writes that
persist like any setting — no program upload.

**Not used:** the SDK defines `mode*_showScale` (ids 104/111/118/125, "Show Scales"), but the
Piano does **not** report those items, so GRAB doesn't write them. A per-key example program
(`third_party/roli_blocks_basics/littlefoot/scripts/Example Scripts/ROLIPianoExample.littlefoot`)
exists for temporary custom lighting but is unrelated to native scale display and untested.

_Contributed by Legitimate_Horror_72 (Piano 49, fw 1.5.0)._

## Config items reported (id · name · range / choices)

| id | name | range / choices |
|---|---|---|
| 0 | MIDI Start Channel | 1..16 *(Single/Multi Channel mode only)* |
| 2 | MIDI Mode | 0 Multi Channel, 1 MPE, 2 Single Channel |
| 3 | Pitch Bend Range | 0..96 |
| 4 | Octave | -4..3 |
| 5 | Transpose | -11..11 |
| 9 | No. MIDI Channels | 1..15 *(MPE mode only)* |
| 10 | Strike Sensitivity | 0..127 |
| 11 | Glide Sensitivity | 0..127 |
| 13 | Pressure Sensitivity | 0..127 |
| 14 | Lift Sensitivity | 0..127 |
| 15 | Fixed Velocity | 0..1 |
| 16 | Fixed Velocity Value | 1..127 |
| 20 | Active Mode | 0..3 (Mode 1..4) |
| 22 | Scale | 0..18 — named by GRAB (device reports no names); see **Scales & scale lighting** |
| 27 | Key | 0..11 (C..B) |
| 30 | Pitch Bend Tracking | 1 Multi-Channel, 2 Last Played, 3 Highest, 4 Lowest |
| 32 | Pressure Tracking | 0 Poly Aftertouch, 1 Channel Pressure |
| 34 | Global Key Color | ARGB |
| 35 | Root Key Color | ARGB |
| 36 | Brightness | 0..100 |
| 40 | MPE Zone | 0 Lower, 1 Upper *(MPE mode only)* |
| 41 | Key Pitch Bend Amount | 1..12 |
| 47 | Pedal Control Change | 0..127 |
| 48 | Pedal Type | 0 Sustain, 1 Expression |
| 56 | MIDI Port Forwarding | 0..1 |
| 57 | MIDI Port Throttling | 0 Off, 1 Half, 2 Quarter, 3 Note On/Off |
| 100 / 107 / 114 / 121 | Mode 1–4 Color Scheme | 0 Pro, 1 User, 2 Piano, 3 Stage, 4 Rainbow |
| 102 / 109 / 116 / 123 | Mode 1–4 Enable Pitch Bend | 0..1 |
| 103 / 110 / 117 / 124 | Mode 1–4 Enable Pressure | 0..1 |

**Note:** items `0` vs `9`/`40` swap when **MIDI Mode** changes, so a preset that switches
modes only fully applies after a **save + reload**.

_The Piano M item map above was verified end-to-end against a full ROLI Dashboard capture
(every item exercised one at a time, right after a factory reset) contributed by **lluisgerard**
(Piano M, fw RISE1.00 — [issue #11](../../issues/11)); it confirmed the map is complete and that
Color Scheme value 2 = Piano._
