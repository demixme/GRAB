# Control Blocks — Live / Loop / Touch (settings reference)

ROLI **Control Blocks** — **Live**, **Loop** and **Touch** (plus the **Developer Control** block) —
are `0x77` BLOCKS devices with **10 buttons** and an LED strip. They are **DNA slaves** (`isMaster:false`),
normally snapped onto a Lightpad or Seaboard Block, though they can connect on their own. All of them ship
the **byte-identical `control` firmware image** (ROLI serves one build for the whole family — see
[PROTOCOL.md](PROTOCOL.md) §12), and they run the **same factory Littlefoot program**, whose metadata
targets `Loop;Live;Touch;Developer Control`. So **one decode names the settings for all of them**; the
per-type differences are only the physical button layout and the LED-strip accent GRAB draws.

Unlike the Lightpad's apps (whose settings are program-internal and never reported — see
[LIGHTPAD-APPS.md](LIGHTPAD-APPS.md)), the control blocks **report their button config as ordinary
device config items**, so GRAB reads and **live-edits** them.

**Source:** decoded from `…\ROLI Dashboard\Littlefoot\Control Block Default Program.littlefoot`
(settings metadata + the `packButtonConfig` layout), and confirmed against captured DB profiles for a
**Live** block and a **Loop** block (firmware 1.1.0) plus two button-press captures. A **Touch Block**
capture is still welcome to confirm its per-type button layout and defaults.

---

## Config item IDs

| ID | Name | Type | Notes |
|----|------|------|-------|
| `0` | Global MIDI channel | int | `0`–`16` |
| `64` | Button Mode | options | `0` internal · `1` MIDI CC · `2` SysEx |
| `65` | Reset | int | write `0` to reset all configs |
| `66`–`75` | Button 1–10 data | int32 (packed) | per-button settings, packed (below) |
| `76`–`85` | Button 1–10 colour (on) | colour | ARGB int32 |
| `86`–`95` | Button 1–10 colour (off) | colour | ARGB int32 |

IDs `66`–`95` carry raw int32 values (range `0x80000000`..`0x7FFFFFFF`). GRAB previously listed 64–95 as
`unknownConfigIds`; they are now named in `src/common/ConfigOverrides.h` and edited in `MainComponent`.

### Per-type button names

The ten button rows are the same config ids on every control block, but the physical caps are labelled
differently per type. GRAB names them by their real function (from the ROLI Control Blocks Creator Manual,
§4.2–4.5); `controlButtonName()` in `MainComponent.cpp` maps the block's hardware type → these labels.
idx `0`–`7` are the eight round caps (top row `0`–`4`, lower row `5`–`7`); idx `8`/`9` are the `−`/`+` pill.

| idx | Live | Loop | Touch | Developer |
|-----|------|------|-------|-----------|
| 0 | Mode | Mode | Strike | 0 |
| 1 | Volume | Volume | Glide | 1 |
| 2 | Scale | Click | Slide | 2 |
| 3 | Chord | Snap | Press | 3 |
| 4 | Arp | Undo | Lift | 4 |
| 5 | Sustain | Play/Pause | Strike Lock | 5 |
| 6 | Octave | Record | Glide Lock | 6 |
| 7 | Love | Learn | Piano Mode | 7 |
| 8 | − | − | − | − |
| 9 | + | + | + | + |

An unknown/generic block falls back to the generic "Button 1–10" labels. A user-learned name (renamed in
GRAB) still wins over the function name.

## Packed button data (ids 66–75)

Each button's settings are packed into one int32:

```
packButtonConfig(value, minVal, maxVal, cc, mode) =
    ((value & 0x7F) << 24) | ((maxVal & 0x7F) << 17) | ((minVal & 0x7F) << 10) | ((cc & 0x7F) << 3) | (mode & 0x7)
```

Unpack:

| Field | Bits | Meaning |
|-------|------|---------|
| value (default) | `(v >> 24) & 0x7F` | default / sent value |
| maxVal | `(v >> 17) & 0x7F` | maximum |
| minVal | `(v >> 10) & 0x7F` | minimum |
| cc | `(v >> 3) & 0x7F` | MIDI CC number |
| mode | `v & 0x7` | behaviour (below) |

**Modes:** `0` toggle · `1` pulse · `2` gate · `3` trigger · `4` cycle · `5` inc/dec · `6` tri-state.

**Factory default, button _i_:** `packButtonConfig(100, 0, 127, 102 + i, gate)` → CC **102..111**,
value 100, min 0, max 127, mode gate. On colour **cyan** (`0xFF00FFFF`), off colour **white**
(`0xFFFFFFFF`). Verified: profile `id66 = 1694368562` unpacks to value 100 / max 127 / min 0 / cc 102 /
gate, matching the button-press capture `B0 66 7F` (CC 102, momentary 127 press / 0 release).

---

## In GRAB

- **Graphic** — each type is drawn in its real **10-button layout** (5 round buttons on top; 3 round
  plus a wide **−/+** pair below), with the LED strip tinted per type (Touch purple, Loop teal,
  Live orange).
- **Editor** — per-button rows (ids 66–75) expose a two-line editor matching ROLI Dashboard: **MIDI CC**
  slider + **behaviour** dropdown on top, **Default / Min / Max** steppers beneath. The on/off colours
  (ids 76–95) are colour wells in their own rows. Edits repack `packButtonConfig` and write live.
- **Read + write confirmed on real hardware** (2026-11-01, contributor *moofy*, Live + Loop via a Lightpad
  master): editing a button's **MIDI CC / behaviour / on-off colour** in GRAB takes effect on the device —
  the LED colour changes and the button sends the new CC. So the full round-trip (decode → `packButtonConfig`
  → live write) is proven, not just math-checked.

## ⭐ Help wanted — one gap left: a Touch Block

Reading and writing are both confirmed on **Live** and **Loop** blocks. The only remaining gap:
- **We have no Touch Block data at all.** If you have a Touch Block, run **Advanced ▸ Help add device**
  and press every button so we can confirm its layout and defaults. (The config *names* already cover it;
  this is confirmation.) Connected on its own or chained to a Lightpad/Seaboard, either works.
  See [CONTRIBUTING.md](CONTRIBUTING.md).

See also: [PROTOCOL.md](PROTOCOL.md) (0x77 BLOCKS framing, firmware catalogue),
[CONTRIBUTING.md](CONTRIBUTING.md) and [tools/CAPTURE-DASHBOARD.md](tools/CAPTURE-DASHBOARD.md)
(how to send a capture).
