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
  the Piano (Device Manager **code 10**). With **Windows MIDI Services** installed, pick
  the in-box **"USB MIDI 2.0"** driver manually in Device Manager — the port then shows
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
| 22 | Scale | 0..18 (no names reported; 0 = Major) |
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
