# Seaboard RISE / GRAND — protocol notes for contributors

**Status: the Seaboard RISE 2 and the original RISE (25/49) are both supported — read and
write.** The RISE 2 was verified on fw 2.0.2 (macOS); the original RISE returns the **same
224-byte state dump** and uses the **same `0x78 3D` / RPN / NRPN write protocol**, confirmed
from a contributed RISE 25 Dashboard capture (see "Write protocol confirmed" below). The one
caveat on the original: the Strike/Press/Lift **min/max block reads back as zeros**, so GRAB
hides those (they're writable, just not readable on that firmware). The **GRAND** is confirmed (Oct 2026) a
`0x78` RISE-family device **configured with the same `3D` set-param protocol as the RISE** (from
a Dashboard↔GRAND capture — see "Seaboard GRAND" below); it is **4D (no slide)** with **3 pedal
CCs**, and its 87-byte reply is shorter than the RISE's 224-byte dump. Writing works via the
existing RISE code; a systematic capture is still needed to complete its param map. The RISE 2 protocol below is confirmed on that device; the rest of this document
collects what is known so far for the other models.

**Presets (v0.1.19):** GRAB's RISE page ships ROLI's four built-in presets — **MPE / Multi
Channel / Single Channel / Piano** — transcribed from Dashboard's `Rise.littlefootModes/*.mode`
files (see "Config parameters — reference" below). Note the RISE **Piano** preset is just
Single Channel with **Glide Sensitivity 0** — the RISE has no "Piano Mode" flag (unlike the
Seaboard Block, whose Piano preset sets that flag and zeroes Slide/Press/Lift instead).

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
| 46..62 / 63..74 | MAC / serial (GRAB blanks these in logs, captures and submissions) |
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

#### Original RISE 25 and 49 answer with the same dump (confirmed, fw 1.1.8)

Contributed captures of an **original Seaboard RISE 49** *and* an **original Seaboard
RISE 25** (both 2016, firmware **1.1.8**) answering `F0 00 21 10 78 3F F7` show they return
the **same 224-byte `3A` dump as the RISE 2** — byte-identical in structure between the 25
and the 49 — and most offsets above decode correctly against it:

- ✅ model `RISE1.00`, firmware `1.1.8` (the main image matches ROLI's `MAIN_1.1.8.syx`),
  MAC, serial, MPE block (79–82), bend range (83), octave (84), glide/press tracking
  (85/86), pedal CC (88), sensitivities (105–108/148), slide CC/mode (150/151), fixed
  velocity (152/153), transpose (207), battery (209).
- ⚠️ **Min/max block (154–159) reads all zeros** — either not stored on this firmware or
  at a different offset; don't trust those six on an original RISE.
- ✅ **Fader CCs (92/96/100) and XY CCs (102/103)** — the original RISE has the **same three
  touch faders + XY pad** as the RISE 2 (hardware-confirmed by an owner), and these offsets
  decode to the **expected CC numbers** (107/109/111, 113/114), so GRAB shows them.
- There is an extra version string (`1.1.0`) at offset 14 that a RISE 2 leaves blank; it
  does **not** shift the settings region (every offset from 22 on still lines up).

Because the port is named `ROLI Seaboard RISE 49`, `rise2::isRisePort()` already matches
it, so GRAB (≥ 0.1.9) routes it to the RISE page rather than the BLOCKS path. GRAB treats
any fw 1.x RISE as editable and **hides only** the Strike/Press/Lift min/max block (it reads
back as zeros on that firmware — writable, just not readable), via `isCurveMinMax()` in
`Rise2Component.cpp`.

**Field-confirmed post-release (Oct 2026, GRAB 0.1.21, macOS).** An independent, in-the-wild
RISE 49 auto-explore capture reproduced the same `3A` reply: model `RISE1.001.1.0`, firmware
**1.1.8** (main + second image), BLE module `0.0.4`, 218-byte body. Two things validated on
real hardware: (1) RISE 49 read works on the latest build on macOS, and (2) the serial/MAC
block came back **fully redacted** (`58 58 …` = `X`), confirming the `0x78` identify-reply
anonymiser shipped in **v0.1.20** scrubs correctly against a live device — no identifiers leaked.

#### Write protocol confirmed (fw 1.x, from a RISE 25 Dashboard capture)

A contributed macOS Dashboard↔RISE-25 capture (see [tools/CAPTURE-DASHBOARD.md](tools/CAPTURE-DASHBOARD.md))
shows the original RISE uses the **same writes as the RISE 2**, verified against a preset
load and (for Octave) by replaying the message live:

- settings: `F0 00 21 10 78 3D <param> <value> F7` — MIDI mode `0E`, sensitivities `17`–`1B`
  (glide/slide/press/strike/lift), endpoints `46`–`4B`, slide CC `38`, slide mode `39`,
  faders `1C`/`1D`/`1E`, pedal `08` — all matching `Rise2Protocol.h`;
- RPN/NRPN on ch 1: Pitch Bend Range = RPN 0, MCM = RPN 6, Octave = NRPN 2 (`4+oct`),
  Transpose = NRPN 3 (`64+t`), Press Mode = NRPN 4 — all matching the RISE 2 too.

The preset load confirmed `3D 1A 65` = Strike sensitivity 101 (so `0x1A` = Strike, as on the
RISE 2). So GRAB's existing RISE-2 write code drives the original RISE directly.

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
| **Seaboard RISE 49** (original, 2016) | Windows 11 **+ macOS** | `ROLI Seaboard RISE 49` / `Seaboard RISE 49` | `vid_2af4 pid_0210` |
| **Seaboard RISE** (likely RISE 2) | macOS | `Seaboard RISE` | (CoreMIDI UID only) |
| **Seaboard RISE 2**, firmware 2.0.2 | macOS | `Seaboard RISE 2` | (CoreMIDI UID only) |
| **Seaboard GRAND** ("GRAND 62") | macOS | `Seaboard GRAND` | (CoreMIDI UID only) |
| **ROLI Airwave Expression** | macOS | `ROLI Airwave Expression` | (CoreMIDI UID only; **input only** seen — see below) |

### Device-name aliases (for detection)
GRAB matches a port by **substring**, case-insensitive, over the name **and** identifier, so
any alias a device reports is caught: `seaboard`, `rise`, `grand`, `piano`, `lumi`, `lightpad`,
`airwave`, `block`, `roli`, plus the USB vendor id `vid_2af4` and the identifier tokens
`seaboardblock` / `pianofortynine`. Known names/aliases so far:
- **Seaboard GRAND** — `Seaboard GRAND`; also marketed as **Seaboard Grand 88** and the
  **Stage / Studio / Limited** editions; key-count suffixes seen like **GRAND 62** / **GRAND 88**.
  (Firmware server: only `Seaboard GRAND` is registered, and it returns **204 — no build**; all
  size/edition names 404. See [PROTOCOL.md](PROTOCOL.md) §12.)
- **Seaboard RISE** — `Seaboard RISE`, `Seaboard RISE 25`, `Seaboard RISE 49`,
  `ROLI Seaboard RISE 49`, `Seaboard RISE 2`. Firmware products: `Seaboard RISE` (49 → 1.1.8),
  `Seaboard RISE 25`, `Seaboard RISE2` (no space, RISE 2).
- **Seaboard Block** — `Seaboard Block`, `Seaboard Block M`, `Seaboard BLOCK (6x3)`.
- **ROLI Piano (49)** — name contains `Piano`/`ROLI`; identifier `pianofortynine`.
- **LUMI Keys / Piano M** — `LUMI Keys`, `LUMI Keys Studio Edition`, `Piano M`.

Note: both `Seaboard RISE…` and `Seaboard GRAND` route to the RISE page (the `0x78` path). As of
v0.1.31 `isBlocksDevice()` excludes "grand" and `rise2::isRisePort()` includes it, so a real GRAND
no longer gets grabbed by the BLOCKS detector (which left it showing no controls while `0x77` timed
out). The GRAND page is provisional: it shows the fields confirmed end-to-end — the three pedal CCs
and glide sensitivity — and reads them from the GRAND offset map below.

## Seaboard GRAND — first capture (Oct 2026)
A contributed auto-explore capture (macOS) settles the big question: **the GRAND is a `0x78`
RISE-family device**, not a BLOCKS (`0x77`) one. It answers the ROLI identify
`F0 00 21 10 78 3F F7` with a `0x78 3A` reply, and emits ordinary MPE (pitch bend, channel
pressure, notes). It did **not** stream any `0x77` BLOCKS traffic (so, unlike the Piano 49, it
is *not* dual-protocol).

The GRAND's `3A` reply is **87 bytes** — not the RISE's full 224-byte dump. Decoded header
(differs from the RISE layout):
- offset 6: model `GRND` + format version `1.01`
- offset 14: a hardware/ID code (`SBG-…`; redacted by the anonymiser)
- then version strings `0.0.7` (bootloader), `0.6.9`, `0.6.9`, `1.9.5`
- then a ~22-byte **settings tail** (see below) + checksum

### GRAND config IS the RISE `0x78 3D` protocol (Dashboard capture, Oct 2026)
A **ROLI Dashboard↔GRAND 62 capture** (contributed by Fishbonecreek / BabeleDunnit, macOS,
Dashboard 4.7.2) settles it: **the GRAND is configured with the same `F0 00 21 10 78 3D
<param> <value> F7` set-param protocol as the RISE** — 308 such writes in the session. It does
**not** use `0x77`: Dashboard sent 690 BLOCKS `requestTopology` probes and the GRAND **never
replied** to any (so it is not BLOCKS, not dual-protocol). So the earlier "identity-only,
unmappable" note was wrong — GRAND config is **writable exactly like a RISE**.

Params seen written (mapped against `Rise2Protocol.h` set-param ids):
- `0x08 / 0x0A / 0x0C` = **three pedal CCs** (e.g. 60 / 67 / 66), each with a `0x09 / 0x0B /
  0x0D` companion byte — a **GRAND-specific** feature (a grand has 3 pedals; the RISE has 1).
- `0x0E` = **MIDI mode** (0 = Single, 1 = MPE).
- `0x17` = **Glide sensitivity**.
- No Strike/Slide params — the GRAND is **4D (no slide axis)**; its Dashboard has no Strike
  Sensitivity or Slide CC. This capture is **partial** (the contributor couldn't exercise
  every control), so the full param set still needs a systematic per-setting capture.

**The 87-byte `3A` reply carries live settings, not just identity.** Its ~22-byte tail is
`3E 01 0F 00 01 | 3C 00 43 00 42 00 | 01 00 01 00 | 30 | 40 40 | 00 0C 00 00` + checksum —
and `3C 00 43 00 42 00` = the three pedal CCs (60/67/66) **echoing the values written**. So the
GRAND is **writable like a RISE and partially readable** via a GRAND-specific (compact, not
224-byte) offset map that still needs fully decoding.

### Full Dashboard capture (Oct 2026, BabeleDunnit) — GRAND `3A` offset map

A **complete** GRAND 62 Dashboard capture (MIDI Monitor `.mmon`, every pedal + mode + glide
exercised) was decoded by diffing its **28 `0x3A` state dumps**. The reply body (after
`F0 00 21 10 78 3A`) is 80 bytes; offsets below are **from `F0`** (what `rise2::State::at()` uses),
and are the `goffset::` constants in `Rise2Protocol.h`. Verified: pedals read 60/67/66, glide
tracked a 0→127 sweep, key count = 62.

| offset (from F0) | field | notes |
|---|---|---|
| 6 | model | `GRND1.01` |
| 38 | firmware | `0.6.9` |
| 62 | key count | `0x3E` = 62 (GRAND **62**) |
| 67 / 69 / 71 | pedal 1 / 2 / 3 CC | low byte of a 2-byte pair; 60 / 67 / 66 |
| 77 | pitch bend range | 48 default (seen 12/48/60/72) |
| 78 | transpose | 64 = centre |
| 80 / 81 | glide / press sensitivity | 0..127 |

Write params (`3D`), confirmed from the same capture: pedals `08/0A/0C` (+ companion toggles
`09/0B/0D`), MIDI mode `0E` (0 Single / 1 MPE), glide sensitivity `17`.

**Still to confirm (left off the page until verified on hardware):** the reply offsets for MIDI
mode / MPE members / zone / octave, and whether bend/transpose are writable via `3D` or RPN on the
GRAND. GRAB now routes a real GRAND to the RISE page showing pedals + glide; extend from there as
BabeleDunnit verifies more fields.

Vendor `2af4` is shared across ROLI devices (the Seaboard **Block** is `vid_2af4 pid_0700`);
the PID identifies the model. The **original RISE 25 and 49** both answer the same `0x78 3F`
identify as the RISE 2 (see "Original RISE 25 and 49 answer with the same dump" above). What
is still open for them is whether their `0x78 3D` write params match the RISE 2's.

**To capture one:** in GRAB, **Advanced… → Raw MIDI capture** → pick `Seaboard RISE` → **Start** → play and
operate the device (and change settings on it) → **Save to file** → attach the `.txt` to
an issue. That recording is the raw material for adding support; the `<param>` encoding
in the tables below still needs confirming against such a capture.

## ROLI Airwave Expression — first sighting (Oct 2026, not yet probed)
A macOS auto-explore capture (GRAB 0.1.21) is the first time an **Airwave Expression** has
connected to GRAB. GRAB saw it as a MIDI **input** (`ROLI Airwave Expression`) but logged
**`Output: (none)`**, so it could not send the `0x78 3F` identify and captured **no protocol
data** (the record holds only the port listing). Nothing about its dialect is known yet.

**This is confirmed device behaviour, not a GRAB bug.** `ExplorePanel::pickRoli` matches any
port whose name contains `roli`/`seaboard`/`rise`/`grand`, and it matched the Airwave *input*
fine — the capture's own `# port OUT …` listing shows **no ROLI output port at all** (only IAC
buses and a Stream Deck). So the Airwave enumerates as **input-only**: it streams MPE out but
exposes no MIDI *destination* to send an identify (or any SysEx) to. Plausible for a
hand-tracking *expression* sensor — it may simply have nothing to configure over MIDI, or its
configuration travels over a non-MIDI transport (e.g. a USB vendor interface or BLE GATT).

**Implication:** the Airwave can't be active-probed the way the Seaboards/RISE are, because
there's no output endpoint. It stays **detect-only** unless (a) an owner reports an Airwave
output port on another OS/firmware, or (b) its config turns out to live on a different
transport we'd need to investigate separately. No code change is warranted on the strength of
this one capture.

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

GRAB has an **Active probe** tool for exactly this (**Advanced… → Active probe**): pick the
device's output + input, choose a curated message (the MIDI Identity Request, or one of
the documented `0x78` writes below with a value), review the exact bytes, tick **Arm**,
and **Send** — the reply is logged and can be submitted/saved. Firmware/reset/packed
messages are hard-blocked. Confirming each `0x78` `<param>` this way is how RISE support
gets built.

**Start with the Identity Request** (the default, read-only probe). Its reply — or
silence — is the decision point: if the RISE answers SysEx there's a query channel to
explore; if it's silent, config is likely via **RPN** (standard CCs, see the old-RISE
notes below) or only visible by capturing ROLI Dashboard, not by probing.

## Config parameters — reference (from ROLI Dashboard's preset files)
> **For the RISE 2, the confirmed on-wire protocol is the "Seaboard RISE 2 protocol"
> section above — use that.** The tables below are the host-side variable names + defaults
> from Dashboard's `.mode` files, kept as the *semantic* set and as a starting point for
> the one device **not yet confirmed**: the **GRAND**. (The original **RISE 25/49** are now
> confirmed to answer `0x78 3F` with the same dump as the RISE 2 — see above; only their
> *write* params remain to be captured.)

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
