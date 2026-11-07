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
  models/        12 models assembled from the kit, exactly as GRAB would do it (reference + test fixtures)
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
- **Which key counts to ship.** Built now: Block 24, RISE 25/49, GRAND Studio 37 + Stage 61, LUMI 24, Piano 49.
- **GRAND = Studio (37 keys, 608 × 338 mm) and Stage (61 keys, 937 × 338 mm)**, per ROLI's support page
  and the photos, in a matte black body (not wood). GRAB's port-name inference (`MainComponent.cpp`,
  around line 44) treats any GRAND that isn't "88" as 61 keys from C3, so a Studio would be drawn as 61.
  It needs a Studio → 37 case. Is there really a GRAND 88, or can that branch go? (The Studio
  assembles at 530 px wide against 540 from ROLI's size: it shares the Stage's key pitch.)
- **RISE 49 width.** It assembles at 1224 px against the photo's 1190. RISE 2 keys are slightly
  narrower, so it's one pitch value in the manifest if you want it exact.
- **RISE 2 panel.** The guide mentions Pitch/Mod edge strips, but the RISE 2 photo shows the same
  3-fader panel as RISE 25, so the art follows the photo. The RISE top `< >` pill isn't in the guide's
  id list, so it's emitted as `btn-nav-prev` / `btn-nav-next`.
- **Control Block icons** are redrawn from the product photos and ROLI's Control Blocks Creator Manual
  (Live: Mode, Volume, Scale, Chord, Arp, Sustain, Octave, Love). `KeyboardView::drawControlIcon`
  still draws the old glyphs, so port them or use `icons-<variant>.svg`.
- No ROLI logo or wordmark is drawn (trademark).

## Regenerate

```sh
python3 design/device-art/generator/kit.py   # rewrites art-v2/kit + art-v2/models, then validates
```

Visual decisions (tones, light, radii, LED looks, accents) live in `generator/style.json`.
