# Product photos (reference for tracing device art)

Drop overhead **top-down** ROLI product shots here. They're the tracing reference for redrawing the
templates in [`design/templates/`](../templates/) accurately. Not shipped in the app; reference only.

## What makes a shot usable

- **True top-down / orthographic** — straight down on the device, not a 3/4 angle. (An angled hero
  shot is still useful for material/lighting reference, but I can't trace proportions off it without
  perspective-correcting. If that's all that exists, say so and I'll de-skew it.)
- **Whole device in frame**, square to the edges, minimal crop.
- **Highest resolution** you can get (ROLI press/store images, manuals, or your own camera overhead).
- Plain/transparent background if possible (easy to mask out).

## Naming (so each maps to a GRAB device family)

Use these names (`.png` or `.jpg`) — they match the templates and GRAB's `Family`/`Control` types:

| File | Device | Template it refines |
|---|---|---|
| `seaboard-block.*` | Seaboard Block (24-key) | `seaboard-keywave.svg` |
| `seaboard-rise-25.*` | Seaboard RISE 25 | `seaboard-rise.svg` |
| `seaboard-rise-2.*` | Seaboard RISE 2 | `seaboard-rise.svg` (RISE 2 variant) |
| `seaboard-rise-49.*` | Seaboard RISE 49 (original) | `seaboard-rise.svg` (wide) |
| `seaboard-grand.*` | Seaboard GRAND | `seaboard-keywave.svg` (grand radius) |
| `lumi-keys.*` / `roli-piano-m.*` | LUMI Keys / ROLI Piano M | `lumi-piano-keybed.svg` |
| `roli-piano-49.*` | ROLI Piano 49 | `lumi-piano-keybed.svg` (wide) |
| `lightpad-block-m.*` | Lightpad Block M | `lightpad-block.svg` |
| `control-live.*` | Live Block | `control-block.svg` |
| `control-loop.*` | Loop Block | `control-block.svg` |
| `control-touch.*` | Touch Block | `control-block.svg` |
| `control-developer.*` | Developer Control Block | `control-block.svg` |

A `-detail` suffix is fine for extra close-ups (e.g. `control-live-detail.jpg` for the button icons).

Once they're in, I'll trace each to correct the template proportions (body shape, key spacing, panel
layout, button/LED positions) while keeping the layer-id contract and live-overlay geometry intact.
