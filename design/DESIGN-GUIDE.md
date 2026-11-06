# GRAB device-art design guide

**For: a graphics designer producing polished, photo-realistic (but 2D) renders of the ROLI
devices GRAB displays.**

GRAB currently draws each device with hand-coded vector art (`src/gui/KeyboardView.h`). This guide
and the SVG templates in [`design/templates/`](templates/) let you replace that art with something
properly rendered — moulded keywaves, real material shading, convincing LEDs — **without breaking the
things GRAB paints live on top** (finger-touch rings, per-key lighting, button LED colours, the
selection highlight).

Read the two hard rules first; they are what make your art usable rather than just pretty.

---

## Rule 1 — keep the footprint proportions exact

GRAB lays every device out on a shared **logical-unit grid** and scales the whole arrangement to fit.
Each device occupies a fixed footprint in those units. **Your artboard must have the same aspect
ratio as the footprint**, or the device will look stretched and — worse — the live overlays will
land in the wrong place.

Convention for the templates: **1 logical unit = 100 px**, and **1 logical unit = 47 mm** of real
hardware (the BLOCKS DNA grid module is 94 mm = 2 units — see below). So the pixel grid maps to a real
physical scale; use it to sanity-check any traced proportions.

### BLOCKS DNA family — true relative scale (locked by the magnet grid)

These snap together on ROLI's 94 mm DNA module grid, so their sizes are **fixed relative to each other**
and must stay that way (a Seaboard Block is exactly 3 Lightpads wide and 1.5 deep). Confirmed against
ROLI's published dimensions and the top-view photos in `product-photos/`:

| Device (GRAB `Family`) | Real top-view | DNA modules | Footprint (units) | Artboard | Notes |
|---|---|---|---|---|---|
| **Lightpad Block M** (`lightpad`) | 94 × 94 mm | 1 × 1 | 2 × 2 | 200 × 200 | square; 15 × 15 pressure LED grid |
| **Control Block** (`control`) | 95 × 48 mm | 1 × ½ | 2 × 1 | 200 × 100 | 10 buttons; Live / Loop / Touch / Developer variants |
| Seaboard **Block** (`block`) | 282 × 141 mm | 3 × 1½ | 6 × 3 | 600 × 300 | 2 octaves / 24 keys, moulded keywave |

### Standalone instruments — own aspect only (not on the DNA grid)

RISE, GRAND and the ROLI Piano / LUMI don't snap to BLOCKS, so only **each device's own aspect ratio**
matters (they're shown on their own). Aspects below are **measured from the top-view photos** and are
authoritative over the old procedural estimates. Height is anchored at 3 units (300 px) by drawing
convention; width = 3 × aspect. These depths are **not** cross-device scale.

| Device (GRAB `Family`) | Photo aspect (W:H) | Footprint (units) | Artboard | Notes |
|---|---|---|---|---|
| **LUMI Keys / ROLI Piano M** (`lumi`) | 2.01 | 6 × 3 | 600 × 300 | 24 keys, flat RGB-lit keybed |
| Seaboard **RISE 25** (`rise`) | 2.41 | ~7.2 × 3 | 723 × 300 | panel + 25-key keywave. Template: `seaboard-rise.svg` |
| Seaboard **GRAND** (`grand`) | 2.78 | ~8.35 × 3 | 835 × 300 | this key count; 25 / 49 / 61 variants, softest keywave |
| **ROLI Piano 49** (`lumi`) | 3.55 | ~10.6 × 3 | 1065 × 300 | 49 keys — pitched **tighter** than the 3-units/octave keywave rule (set tiling to match) |
| Seaboard **RISE 2 / RISE 49** (`rise`) | 3.97 | ~11.9 × 3 | 1190 × 300 | panel + 49-key keywave (C2–C6). Re-lay the panel per RISE 2; tile the keys |

The keybeds are **procedurally tiled by key**: GRAB draws `keyCount` keys from a low note, white keys
evenly spaced at `3 units / 7 white keys`, black keys on the boundary between whites. So you don't
need a separate artboard per key count — design **one octave (or the 2-octave module)** cleanly and
we tile it. The 600×300 and 200×200 templates are the canonical modules; wider boards (RISE 49, Piano
49) are the same module repeated — **except the ROLI Piano 49**, whose measured aspect (3.55) is tighter
than tiling the 24-key LUMI module at 3 units/octave would give (~4.1): its keys are narrower, so the
per-octave width needs lowering to ~2.65 units/octave to match the photo when that art is wired in.

**Rotation:** snapped BLOCKS can be rotated 90° in GRAB (it rotates your art about its centre). Design
everything to read correctly at 0°; the square devices (Lightpad, and roughly Control) rotate cleanly.

---

## Rule 2 — leave the live layers to GRAB

Some things are **not static art** — GRAB paints them every frame from live device data. Your file
must keep these as **separate, named, and (where noted) empty** layers, so GRAB can draw into/over
them. Bake them into the chassis art and they'll be wrong the moment a device is connected.

| Layer id | What GRAB does with it | You provide |
|---|---|---|
| `live-overlay` | Draws finger **Strike disc + Press ring** (and chord ghosts) here, positioned 0..1 across the active area. **Topmost. Keep empty.** | Nothing — reserve the layer |
| `keys` (LUMI/Piano) | Recolours **each key** live from the device's colour scheme (root/scale/rainbow/piano) | One shape per key, id'd `key-w-0…` (whites) and `key-b-0…` (blacks) left→right, so each can be filled live |
| `led-strip` (Control) | Lights the 13-dot strip in the subtype colour | The 13 dot shapes, id'd `led-0`…`led-12` |
| `button-leds` (Control) | Each button has a live **on/off colour** from its config | A fillable ring/halo per button, `btnled-0`…`btnled-9` |
| `grid-leds` (Lightpad) | May light grid cells live | The 15×15 dots, grouped; id the group `grid-leds` |
| `selection` | Draws the accent **border** (bright when selected, dim otherwise) | A border path id'd `selection-frame` that GRAB can recolour, OR leave to GRAB and just keep a 3px margin clear |

Everything else — the moulded body, key material, printed icons, screws, bezels, baked shadows and
specular highlights — is **yours to make as photo-real as you like.** That's the whole point.

### A note on corner radii

The rounded corners in the templates (and in GRAB today) are **just drawing parameters, not hardware
dimensions** — GRAB's current art over-rounds both the device bodies and the keys. Treat every `rx`
value as a starting placeholder and **match the real devices**, which are only gently rounded:
Seaboard/LUMI bodies have a fairly tight body radius, and the keys much less rounding than the
templates show. Change them freely. The values that *do* matter for correctness are the **positions
and sizes** (key spacing, black-key placement, grid pitch, button layout) and the **layer ids** —
not the corner radii.

---

## Active-area geometry (so touches land on the right key)

GRAB maps a finger's normalised position (nx, ny ∈ 0..1) across the **active area**, which is the body
inset by a small margin. Keep your playing surface aligned to these proportions and every touch ring
lands where the finger really is.

**Seaboard keywave** (`drawKeybed`):
- Body corner radius ≈ 12 px (at 100px/unit). Accent border frames the body with a ~3 px margin.
- Keys inset from the body: ~3 px sides, ~4 px top/bottom.
- **Key corner radius** sets the "feel": Block 2.5, RISE 4.0, GRAND 5.5 (softer = larger). Match this.
- **Glide lip**: the keywave's smooth top/bottom rails take **11 %** of key height each — the
  continuous ribbon you glide along. This is the signature Seaboard look; make it moulded, not flat.
- **Black keys**: height = 42 % (Block) / 50 % (RISE, GRAND) of the key area; width = 58 % of a white
  key; sit on the boundary *after* their white key, raised above the white surface.
- Touch y=0 is the top of the keys, y=1 the bottom; x=0..1 spans the full white-key run.

**LUMI / Piano flat keybed** (`drawFlatKeybed`):
- Flat piano-style keys (not moulded). White keys full height; black keys 60 % height, 60 % width,
  centred on the boundary. Each key is individually RGB-lit — see the `keys` live layer above.

**Lightpad** (`drawPad`):
- Square body, corner radius ≈ 10 px. 15 × 15 grid of round LED cells, dot = 62 % of cell pitch,
  centred in the body inset (~5 px). Touches map 0..1 across the grid area.

**Control block** (`drawControl`):
- Body corner radius ≈ 9 px. Layout top→bottom: a **13-dot LED strip**, then **5 round buttons** (top
  row), then **3 round buttons + a wide −/+ pill** (bottom row, pill fills the right two columns).
- Button discs are ~42 % of row height; the printed **icon** on each depends on the variant (below).

---

## Device variants & details

- **Seaboard Block** — compact, hard-edged keywave, 24 keys, USB-C + BLE. Darkest/firmest moulding.
- **Seaboard RISE 25 / RISE 2** — full instrument with a **left control panel**. Use the dedicated
  `seaboard-rise.svg` template. The panel has, as named layers:
  - **`xy-pad`** — the XY touch pad (static surface + bezel). GRAB draws a **live dot** (`xy-dot`)
    showing the current X/Y touch.
  - **`fader-tracks`** — the touch faders (static tracks + labels). GRAB draws a **live thumb**
    (`fader-thumb-0..2`) at each fader's current value. (RISE 25 = three touch faders, labelled
    Glide / Slide / Press by default.)
  - **`buttons`** + **`button-leds`** — the mode button and octave −/+ buttons, each with an LED.
  - The live elements live in a **`panel-live`** group — design placeholder thumbs/dot so you can
    see them; GRAB repositions them from the device.

  **RISE 2 differs:** it drops the three faders for **Pitch & Mod strips on the far-left edge** plus
  the XY pad and buttons. Re-lay the panel to match it (keep the same layer ids / live-element ids),
  and use current ROLI product photos as the realism reference. RISE 49 / GRAND = wider keywave, same
  panel treatment.
- **Seaboard GRAND** — large, premium wooden-bodied keywave; softest key radius. Match the specific
  variant's key count that GRAB reports.
- **LUMI Keys / ROLI Piano M** — 24 edge-lit keys with vivid per-key RGB; the glow under each key is
  the hero. ROLI Piano is the LUMI rebrand; Piano 49 is the wide version.
- **Lightpad Block M** — silicone pressure surface over a 15×15 RGB matrix; the lit cells and the soft
  diffusion through silicone are the look to nail.
- **Control blocks** — same 10-button hardware; only the **printed icons** and accent differ:
  - LED/accent colour: **Touch** purple `#B39CFF`, **Loop** teal `#35D0C6`, **Live** orange `#FF9A3C`,
    **Developer** neutral white `#E6E6EE`.
  - Button legends (left→right, top row then bottom): **Live** = Mode, Volume, Scale, Chord, Arp /
    Sustain, Octave, Love, −, + · **Loop** = Mode, Volume, Click, Snap, Undo / Play-Pause, Record,
    Learn, −, + · **Touch** = Strike, Glide, Slide, Press, Lift / Strike-Lock, Glide-Lock, Piano-Mode,
    −, + · **Developer** = blank caps printed 0–7, −, +. (Full table in [CONTROL-BLOCKS.md](../CONTROL-BLOCKS.md).)

---

## LED & lit-element checklist (design every one — lit *and* unlit)

These devices are defined by their lights. For **every** lit element below, design **two states**: the
**unlit/dim** look and the **lit glow** (GRAB sets the actual colour live, so make the lit state a
colourable glow/halo, not a fixed hue). Keep each on its named, fillable layer.

| Device | Lit elements | Layer / ids | Driven by |
|---|---|---|---|
| **Control block** | **10 button LEDs** (one per button) | `button-leds`: `btnled-0..7`, `btnled-8` (−), `btnled-9` (+) | each button's on/off colour config |
| **Control block** | 13-dot status strip | `led-strip`: `led-0..12` | variant colour (mid dots lit) |
| **LUMI / Piano** | every key edge-lit RGB | `keys`: `key-w-0..`, `key-b-0..` | colour scheme (root/scale/rainbow/piano) |
| **Lightpad** | 15 × 15 LED matrix | `grid-leds`: `led-r-c` | per-cell colour (and finger touch) |
| **RISE panel** | mode + octave button LEDs | `button-leds`: `btnled-mode`, `btnled-oct-up/down` | device state |
| **RISE panel** | fader thumbs + XY dot (lit indicators) | `panel-live`: `fader-thumb-0..2`, `xy-dot` | live touch position |
| **All keybeds** | finger Strike disc + Press ring | `live-overlay` (GRAB draws) | live 5D touch |

So: the control block alone needs **all ten** per-button LEDs rendered (round caps 0–7 plus the two
halves of the −/+ pill), not just a strip. Treat the LED as the halo/ring/underglow around each cap;
GRAB floods it with the configured colour.

## Palette (GRAB's dark theme — match these so art and UI agree)

GRAB is a dark app using ROLI Dashboard's colour language. Your renders sit on a near-black navy
canvas. Use these as your accent/LED/shadow references (hex = ARGB with alpha dropped):

| Token | Hex | Use |
|---|---|---|
| Window background | `#1A1C27` | the canvas behind the device |
| Panel background | `#202034` | cards/surfaces |
| Accent (cyan) | `#00B0BB` | selection border, highlights |
| Strike (blue) | `#0BE4F4` | finger strike disc (GRAB draws) |
| Glide (green) | `#73FE91` | 5D axis |
| Slide (magenta) | `#FF77FD` | 5D axis |
| Press (teal) | `#52D7BA` | finger pressure ring (GRAB draws) |
| Lift (gold) | `#FBC435` | 5D axis |
| Keywave white edge / crest | `#111318` / `#3A3F4A` | moulded white-key shading |
| Keywave black edge / crest | `#060708` / `#22252C` | moulded black-key shading |

A **light-theme variant** is welcome but optional — GRAB ships dark. If you make one, keep the same
layer structure so we can swap by theme.

---

## Deliverables & format

**What to deliver (device checklist):**

1. **Seaboard Block** keywave (24-key module) — `seaboard-keywave.svg`
2. **Seaboard RISE 25** — full instrument, left panel + keywave — `seaboard-rise.svg`
3. **Seaboard RISE 2** — panel re-laid (Pitch/Mod edge strips), same keywave — `seaboard-rise.svg` variant
4. **Seaboard GRAND** — wooden keywave, softest radius — `seaboard-keywave.svg` (grand radius)
5. **LUMI / ROLI Piano M** keybed (24-key module) — `lumi-piano-keybed.svg`
6. **Lightpad Block M** — `lightpad-block.svg`
7. **Control Block** body — `control-block.svg` — **plus 4 icon sets** on the `icons` layer (Live / Loop /
   Touch / Developer legends + accent colours; see Device variants)

RISE 49 and Piano 49 are **not** separate arts — GRAB tiles the keywave / keybed module wider (Piano 49 at
the tighter ~2.65 units/octave noted above). So it's **~6 bodies + the 4 control-icon variants**.

> **Aspect:** every template's `viewBox` now carries the correct, photo-measured aspect (e.g.
> `seaboard-rise.svg` = 723×300 = 2.41, panel re-laid to the real RISE 25 order: faders, XY pad, octave
> −/+). Treat the artboard aspect as fixed; the corner radii and internal detail are yours to refine from
> the photo. (Reference photos are supplied **separately** — they're not in the repo.)

Per device, ideally:

1. **Layered SVG** built on the matching template in [`design/templates/`](templates/) — same
   `viewBox`, same layer ids. This is the preferred master (crisp at any size, themeable).
2. Or **PNG at @1x / @2x / @3x** at the artboard sizes above, **plus** the live layers delivered
   separately (transparent PNGs or kept as SVG) so GRAB can still colour them.

Requirements:
- Keep the **named layers / group ids** from the template. In Illustrator/Figma/Inkscape, named
  groups export as layers — don't flatten.
- Keep the **`live-overlay` group empty and topmost**.
- Keep the **3 px safe margin** around the body clear for the selection border.
- Transparent background (GRAB paints the canvas).
- Design at 0° rotation; make square devices centre-rotation-safe.
- Target on-screen size is small (devices render ~200–1200 px wide), so keep detail legible when
  scaled down — bold moulding and clear speculars read better than fine texture.

Hand back the SVGs/PNGs and we wire them into `KeyboardView` in place of the drawn art, keeping the
same live-overlay draw calls. Questions on geometry → the exact source of truth is
`src/gui/KeyboardView.h` (every constant above is pulled from it).
