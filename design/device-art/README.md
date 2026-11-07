# GRAB device art: V2 tiling kit

New device art for every device GRAB draws. It follows [DESIGN-GUIDE.md](../DESIGN-GUIDE.md) (footprints,
layer ids, live layers). Instead of one picture per model, it's a **kit of parts that GRAB assembles**,
the same way `KeyboardView` already tiles keys.

**Preview it (interactive, GRAB-sized window, lit demo):**
https://claude.ai/artifact/CdfGvULwazwtYt6T7NActG

Nothing here touches the app yet. This PR only adds files under `design/device-art/`.

## What's here

```
art-v2/
  kit/
    tiles/       one key = one tile: seaboard-white, -gap, -black, grand-black, lumi-white, lumi-black
    surrounds/   each body as a 3-slice (left cap fixed, mid stretched, right cap fixed)
                 for block / rise / grand / lumi, plus grand-button (GRAND's centred master button)
    fixed/       lightpad-block, control-block (body) + icons-live / -loop / -touch / -developer
    manifest.json  how to assemble: surround caps, key insets, tile names, white-key pitch per family
  models/        13 models assembled from the kit, exactly as GRAB would do it (reference + test fixtures)
  models.json    slug, name, width for each model
generator/       the Python that draws the kit (see generator/README.md)
```

All SVGs are **JUCE-safe**: no filters, masks, CSS or text, so `juce::Drawable` renders them as they
look in a browser. Every model passes `generator/validate.py` (the guide's layer-id contract, with
`live-overlay` present, empty and topmost).

## How GRAB would use it

1. **Keybeds** (Seaboard Block, RISE, GRAND, LUMI/Piano): draw the family's surround as a 3-slice to
   the device's width. Then loop over keys the way `drawKeybed` / `drawFlatKeybed` already do, scaling
   each tile to `(pitch, key-region height)` from `manifest.json`. Whites sit at their left edge, blacks
   and gaps on the boundary between two whites. One set of tiles covers every key count, and
   `pitchOverrides` handles ROLI Piano 49's tighter keys.
2. **Lightpad / Control Blocks**: draw `fixed/` as-is. A Control Block is the body plus one
   `icons-<variant>.svg` on top.
3. **Live layers**: as in the guide. LUMI keys, LEDs, button LEDs, fader thumbs and the XY dot are
   separate ids, and GRAB recolours the shapes while keeping each shape's baked opacity.
4. **Touch mapping**: each `<svg>` carries `data-grab-active-area="x y w h"`, the rect where the playing
   surface really is. RISE, GRAND and Piano have real bezels, so touches need mapping into that rect
   instead of today's `r.reduced (3, 4)`.

## Decisions for you

- **Kit vs per-model files.** The kit keeps one source of truth for every key count. If wiring tiles
  in is more work than it's worth, `models/` are complete per-model SVGs you can use directly.
- **Which key counts to ship.** Built now: Block 24, RISE 25/49, GRAND Studio 37 + Stage 61 + 88 First Edition, LUMI 24, Piano 49.
- **GRAND = Studio (37 keys, 608 × 338 mm) and Stage (61 keys, 937 × 338 mm)**, per ROLI's support page
  and the photos, in a matte black body (not wood). GRAB already draws each GRAND at the key count it
  reports (`Rise2Component.cpp` ~648), so the tiles fit any count. The **88-key First Edition** (2013,
  88 made, A0–C8, 1313 × 338 mm) is drawn too, from launch photos: same black body and master button.
  (The Studio assembles at 530 px wide against 540 from ROLI's size: it shares the Stage's key pitch.)
- **RISE 49 width.** It assembles at 1224 px against the photo's 1190. RISE 2 keys are slightly
  narrower, so it's one pitch value in the manifest if you want it exact.
- **RISE 2 panel.** The guide mentions Pitch/Mod edge strips, but the RISE 2 photo shows the same
  3-fader panel as RISE 25, so the art follows the photo. The RISE top `< >` pill isn't in the guide's
  id list, so it's emitted as `btn-nav-prev` / `btn-nav-next`.
- **Control Block icons** are redrawn from the product photos and ROLI's Control Blocks Creator Manual
  (Live: Mode, Volume, Scale, Chord, Arp, Sustain, Octave, Love). `KeyboardView::drawControlIcon`
  still draws the old glyphs, so port them or use `icons-<variant>.svg`.
- No ROLI logo or wordmark is drawn (trademark).

## Update: keyboards redrawn from photo traces

The Seaboard Block, RISE, GRAND and Piano/LUMI art was redrawn from Illustrator traces of the product
photos: shapes measured, then simplified to flat colours. **Control Blocks and Lightpad are unchanged**
(byte-identical), so the Control Block wiring in #16 needs nothing. Every model keeps its footprint.

- **Block:** keywave (ridges, S-grooves, gaps) measured from the Seaboard M trace, neutral greys, rounded corners.
- **RISE:** the trace only sets the casing and fader slots; the keys are the Block's tiles, so the two match.
- **GRAND:** cooler charcoal body, lighter key tray, pale pills in a dark halo, lit toes on the white keys.
  It now has **its own white and gap tiles** (`grand-white`, `grand-gap`). Read the tile names from
  `manifest.json` (`families.grand.tiles`) rather than assuming `seaboard-*`.
- **Piano / LUMI:** open-top black-key frames, wide key gaps, light front lip. New optional
  **`families.lumi.blackOffsets`**: each black key's centre offset from its boundary, by pitch class
  (C#=1 … A#=10), in white-key pitches. Offset the black tile by `offset × pitch`. If you ignore it, keys are centred.
- **Seams:** the RISE, GRAND and LUMI 3-slice parts are drawn with non-overlapping regions (`fill-rule="evenodd"`),
  so the left/mid/right joins don't show a line when anti-aliased.
- The look is switchable per device in `generator/style.json` (`"look": "trace"`), with the measurements next to it.

### Selection border and corners

Corners look off when GRAB's old fixed 12 px `drawRoundedRectangle (r.reduced (1), 12.0f, …)` border is drawn
over the art: at GRAB's size the art's corners are much tighter. Size the border from the art instead.
With `s = drawnHeight / 300` (surround SVGs are 300 tall), use `radius = (manifest.families[fam].radius + 1) * s`,
inset by `2 * s` (that's the body's outer edge). For Lightpad / Control Block (200-wide fixed SVGs), use radius 9, inset 4, `s = drawnWidth / 200`.
Draw one border only, not a second outline on the tile bounds.

### Open question

The RISE photo shows the faders as a lit line with no thumb. The art keeps the `fader-thumb-*` live layer
(GRAB moves it to show the value). Keep it, or drop it and show the value another way?

## Regenerate

```sh
python3 design/device-art/generator/kit.py   # rewrites art-v2/kit + art-v2/models, then validates
```

Visual decisions (tones, light, radii, LED looks, accents) live in `generator/style.json`.
