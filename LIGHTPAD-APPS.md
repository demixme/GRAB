# Lightpad apps — settings reference

A Lightpad (and other BLOCKS) runs an **app** — a Littlefoot program such as Note Grid, Fader
Block or Drum Block. The program is uploaded to the device (GRAB never does that — program upload
is out of scope and a brick risk). The factory apps ship with ROLI Dashboard and can be loaded with
open-source tooling — see [PRESERVATION.md](PRESERVATION.md) for how loading works and why the apps
are preservable offline.

**App settings are out of GRAB's reach — and this is architectural, not a missing feature.** This
file documents *what each app offers* and *why* GRAB can't configure it, plus the one app it can.

This started as a hope that app settings were ordinary config items GRAB could read/write. Two
things disproved it:

1. **The device never reports them.** Three Lightpad captures — including one with apps loaded and
   **121 factory config syncs** (issue: Roger's "Lightpad apps test") — only ever reported the
   **standard items 0–40**. No app-specific slots (64–95, 102+) appeared, ever.
2. **ROLI's own code hides them.** Each app's `initialise()` *deactivates* its config from host
   visibility — e.g. Fader Block does `setLocalConfigActiveState(i, false, false)` for items 0–31
   and never activates its fader-CC slots for reporting. So the settings stay **program-internal**
   (program heap), read/written by Dashboard only because it uploaded the program and knows its
   layout. That's program editing — the territory GRAB avoids.

So no capture will surface them: a Lightpad running Fader Block simply doesn't *report* its fader
CCs over MIDI.

**The one app GRAB fully supports is Note Grid** — the default app, which *is* the standard config
(ids 0–40: MIDI, 5D touch, scale, chord, mode slot, colours, tracking). GRAB reads and writes all of
it. Everything below is reference for the other apps: useful for understanding the devices, but not
a GRAB to-do.

**Source:** the tables are extracted from the `<metadata>` blocks of ROLI Dashboard's bundled app
scripts (`…\ROLI Dashboard\Littlefoot\*.littlefoot`) — settings metadata only (names / types /
ranges / defaults), not the Littlefoot program code. The `cfg@N` notes are the program-internal
config indices the metadata declares (not host-visible device config).

---

## Per-app settings (program-internal — not configurable by GRAB)

### Fader Block  (Lightpad)
*Turn your Lightpad into 4 banks of 4 assignable MIDI CC touch faders.*

| Setting | Type | Range | Default | Config |
|---|---|---|---|---|
| MIDI Channel | int | 1–16 | 1 | |
| Fader 1–16 · CC | int | 0–127 | 0 | `cfg@102+` |
| Fader 1–16 · Color | colour | | random | |
| Fader 1–16 · Minimum | int | 0–127 | 0 | |
| Fader 1–16 · Maximum | int | 0–127 | 127 | |

### Drum Block  (Lightpad)
*A drum grid with multiple MIDI modes and layouts.*

| Setting | Type | Range | Default | Options |
|---|---|---|---|---|
| Grid size | int | 1–4 | 4 | |
| Base note | midiNote | C-2–E7 | C1 | |
| Fixed strike | bool | | off | |
| Fixed strike value | int | 1–127 | 127 | |
| Tracking mode | option | | Last note played | Last / Highest / Lowest note on channel |
| Press type | option | | Channel Pressure | Channel Pressure / Poly Aftertouch |
| Slide CC no. | int | 0–127 | 74 | |
| Slide curve | float | −1.0–1.0 | 0.0 | |
| Glide enabled | bool | | on | |
| Glide rate | float | 0.0–1.0 | 0.5 | |
| Send pressure | bool | | on | |
| Send slide | bool | | on | |
| Pad 1–16 · Color | colour | | per-pad palette | |

### Mixer Block  (Lightpad)
*Mix tracks — 4 faders + 4 buttons.*

| Setting | Type | Range | Default | Config |
|---|---|---|---|---|
| MIDI Channel | int | 1–16 | 1 | |
| Fader color | colour | | 0x5082f1 | |
| Button color | colour | | 0xe84ab8 | |
| Strip 1–4 · Behavior | option | | Toggle (Toggle / Gate / Trigger) | |
| Strip 1–4 · Button CC | int | 0–127 | 0 | `cfg@102+` |
| Strip 1–4 · Fader CC | int | 0–127 | 0 | `cfg@112+` |
| Strip 1–4 · Minimum | int | 0–127 | 0 | |
| Strip 1–4 · Maximum | int | 0–127 | 127 | |

### Control Grid  (Lightpad)
*Multiple control pads (up to 5×5).*

| Setting | Type | Range | Default | Options / Config |
|---|---|---|---|---|
| Grid Size | option | | 4x4 | 1x1 / 2x2 / 3x3 / 4x4 / 5x5 |
| MIDI Channel | int | 1–16 | 1 | |
| Trigger Time (msec) | int | 0–2000 | 500 | |
| Use Poly Pressure | bool | | off | |
| Pad 1–25 · Behavior | option | | Note on/off | Note on/off / Controller |
| Pad 1–25 · Action | option | | Gate | Toggle / Gate / Trigger |
| Pad 1–25 · Note | midiNote | | C3 | `cfg@60+` |
| Pad 1–25 · CC | int | 0–127 | 102 | `cfg@102+` |
| Pad 1–25 · Send Pressure | bool | | off | |
| Pad 1–25 · Color | colour | | random | |

### XYZ Pad  (Lightpad)
*An expressive XYZ pad controller.*

| Setting | Type | Range | Default | Options |
|---|---|---|---|---|
| X Axis CC | int | 0–127 | 113 | |
| Y Axis CC | int | 0–127 | 114 | |
| Z Axis CC | int | 0–127 | 115 | |
| MIDI Channel | int | 1–16 | 1 | |
| Touch Color | colour | | 0xFFFF00 | |
| Background Color | colour | | 0x180029 | |
| Send | option | | All | All / X / Y / Z |

### RISE Controller  (Lightpad)
*The Seaboard RISE control panel on a Lightpad.*

| Setting | Type | Range | Default |
|---|---|---|---|
| MIDI Channel | int | 1–16 | 1 |
| X Axis CC | int | 0–127 | 113 |
| Y Axis CC | int | 0–127 | 114 |
| Slider 1 / 2 / 3 CC | int | 0–127 | 107 / 109 / 111 |
| XYZ Touch / Background Color | colour | | 0xFFFF00 / 0x180029 |
| Slider 1 / 2 / 3 Color | colour | | 0x5082f1 |
| Send | option | | All (All / X / Y) |

### MusicGen  (Lightpad)
*Generate chords and rhythms.*

| Setting | Type | Range | Default | Options |
|---|---|---|---|---|
| Speed | float | 0–5 | 1 | |
| Root Note | midiNote | | C1 | |
| Chord | option | | Major | Major / minor |

---

## Apps with no user-configurable settings

These expose nothing to configure via config items (so there's nothing for GRAB to add):

- **Note Grid** — the default app; it *is* the standard config (scale, chord, mode, 5D sensitivity,
  colours, tracking — ids 0–40), which GRAB already supports.
- **GarageBand Control / Logic Control / Ableton Live Control** — DAW surfaces, auto-configured.
- **Melodics** — learning app.
- **BLOCKSvaders / BreakBLOCKS** — games (internal state only).
- **Seaboard / Control Block default programs** — factory defaults.
