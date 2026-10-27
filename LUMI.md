# Piano M / LUMI Keys — notes

> **Status: confirmed with GRAB on a Piano M** (reports as `LUMI Keys`, "ROLI Piano M
> (6x3)", firmware **1.3.9**, USB, macOS): config read, Scale, Key, Color Scheme and both key
> colours all work. The protocol decoding comes from MIDI Monitor captures of ROLI Dashboard
> and ROLI Studio Player driving a LUMI Keys over USB-C; see **Hardware results** at the end.

The **Piano M** is the rebranded **LUMI Keys** (same hardware). It speaks the **same BLOCKS
protocol** (`F0 00 21 10 77 …`) as the Seaboard Block and the ROLI Piano 49, so GRAB should
talk to it through the vendored `roli_blocks_basics` library with no new protocol code.
Its MIDI port is named **`LUMI Keys BLOCK`**; GRAB already matches it (`"lumi"` in
`isBlocksDevice()`, `src/common/LoggingMidi.h`).

## Lighting and scales are plain `setConfig` writes

What looked like a LUMI-specific command set turns out to be ordinary BLOCKS config
messages: message type `0x10` (`configMessage`), command `0` (`setConfig`), an 8-bit item id
and a 32-bit value, bit-packed into 7-bit bytes like every other BLOCKS packet. Decoding
every host→device ROLI frame in the captures gives only standard config items:

| Captured frame (inner bytes) | Decodes to | Same as Piano 49 |
|---|---|---|
| `10 20 64 3F 00 00 7E 03` | item **34** Global Key Color = `0xFF0000FF` (ARGB blue) | yes |
| `10 30 64 3F 00 7F 7F 03` | item **35** Root Key Color = `0xFFFF00FF` (magenta) | yes |
| `10 30 23 00 00 00 00 00` | item **27** Key = 1 (C♯) | yes |
| `10 60 42 04 00 00 00 00` | item **22** Scale = 18 (Chromatic) | yes |

Scale values use the same 0..18 order GRAB already names (see the table in
[PIANO.md](PIANO.md)); all 19 were captured from Dashboard's scale menu. Root note (Key)
is 0..11, C-based.

Across all captures, Dashboard and Studio only wrote items **0, 1, 2, 4, 9, 20, 22, 23, 27,
34, 35, 40, 102, 103**, plus a `requestFactorySync`. GRAB's config overrides are keyed by
item id, not device type, so the LUMI should get the named **Scale** dropdown, both key
colours and the **Color Scheme** dropdown with no LUMI-specific code.

### Observed behaviour

- Key colours (34/35) are only visible when the **active** mode's Color Scheme is **User**;
  other schemes ignore them (same as the Piano). Writing them while another scheme is
  active still stores them — they appear as soon as a User-scheme mode is selected.
- Writes take effect immediately, with no keepalive.
- Settings persist across a power cycle, but the keyboard may start in a different mode
  (Active Mode, id 20, is not restored), so a User-scheme layout can look lost until you
  switch back to that mode with the mode button. See **Hardware results**.
- Incoming MIDI **Note On** lights that exact key **white** (built-in firmware feedback,
  e.g. from a DAW) — no SysEx involved.
- The LUMI dims itself after a few seconds (battery saving).

## Per-key colour needs a LittleFoot program (out of scope)

ROLI Studio's per-key lighting is not a config setting. Studio first uploads a program
(27 `sharedDataChange` packets, `02 01`…`02 1B`), then drives it with
**`programEventMessage`** (host type `0x03`) packets carrying three 32-bit ints:

```
[ operation, MIDI note, 0x00RRGGBB ]
```

From the C-minor arpeggiator capture: notes `0x30`/`0x33`/`0x37` (C3, E♭3, G3) with colours
`0xFFFFFF` (white root), `0xBD10E0` (purple chord keys) and `0xFFB41E` (orange moving
highlight); operations seen were 0–5 and 7 (2 = set base colour, 3 = restore, 4/5 =
highlight on/off — roles inferred). GRAB never uploads programs, so this is documented only.
[lumipaint](https://github.com/SimonBourdareau/lumipaint) (GPLv3, separate project) is a
working per-note-colour program for LUMI Keys / Piano M; it notes that LittleFoot programs
need firmware **1.3.0 or later**, and that Dashboard's factory reset restores ROLI's program.

## Hardware results

Piano M, firmware 1.3.9, USB on macOS, ROLI software closed:

- **Connects** as `LUMI Keys` ("ROLI Piano M (6x3)") with battery level; GRAB shows
  **29 items in 9 sections**, no unknown ids. Reported: 2, 3, 4, 5, 9, 10, 13, 14, 15, 16,
  20, 22, 27, 30, 32, 34, 35, 36, 40, 41, and per mode Color Scheme / Enable Pitch Bend /
  Enable Pressure (100/102/103, 107/109/110, 114/116/117, 121/123/124). Unlike the Piano 49
  it does **not** report 11 (Glide), 47/48 (pedal) or
  56/57 (port forwarding/throttling); Octave range is −4..5.
- **Color Scheme**, **Scale** and **Key** write from GRAB and the keys follow (e.g. D minor
  lit with D as root).
- **Global / Root Key Color** — first written with `seaprobe set 34 …` / `set 35 …`, then
  with GRAB's colour picker (swatch → popover; the keys follow live while dragging, presets
  work). Values are stored and read back; with a User-scheme mode active the scale keys showed blue and the root
  pinkish-magenta.
- **Persistence:** after being switched off for an hour, the key colours, Scale, Key and the
  per-mode Color Schemes all survived. Only the **active mode** didn't: the Piano M started
  in a different mode, and pressing its mode button back to the User-scheme mode brought
  back the blue/magenta D minor layout. So GRAB's writes persist like on the Piano 49; the
  earlier "power cycle resets to full white" observation was the startup mode, not lost
  settings.

## Hardware test checklist

Run with ROLI Dashboard/Connect closed (they hold the port). For each step, note what GRAB
shows and what the keys do.

1. **Connect.** GRAB detects `LUMI Keys BLOCK` and shows a config page. Save the item list
   (Advanced ▸ capture) — which ids does the LUMI report? Is 22 / 27 / 34 / 35 / 100 there?
2. **Color Scheme → User** on the active mode (id 100 + 7 × mode). Keys change to the User
   look.
3. **Global / Root Key Color** (34 / 35): click the swatch in the **Colors** card and pick a
   colour (or a preset); the keys should follow live while you drag.
4. **Scale** (22) and **Key** (27): the lit keys follow the scale relative to the root;
   **Chromatic** lights every key.
5. **Persistence:** unplug/replug, then a full power cycle. Do the colours, scale and key
   survive each? (Captures suggest a power cycle resets them.)
6. **Chained pair** (if available): shows as one keyboard; writes reach both blocks.

The same writes from the command line, one item at a time:

```sh
./build/seaprobe_artefacts/Release/seaprobe blocks              # read config
./build/seaprobe_artefacts/Release/seaprobe set 22 18 --commit  # Scale = Chromatic
```

_Captures and decoding from the WasabiLumi research (LUMI Keys over USB-C, 2026-08-26)._
