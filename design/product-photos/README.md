# Product photos (reference for tracing device art)

Drop overhead **top-down** ROLI product shots here. They're the tracing reference for redrawing the
templates in [`design/templates/`](../templates/) accurately. Not shipped in the app; reference only.

## What makes a shot usable

* **True top-down / orthographic** — straight down on the device, not a 3/4 angle. (An angled hero
shot is still useful for material/lighting reference, but I can't trace proportions off it without
perspective-correcting. If that's all that exists, say so and I'll de-skew it.)
* **Whole device in frame**, square to the edges, minimal crop.
* **Highest resolution** you can get (ROLI press/store images, manuals, or your own camera overhead).
* Plain/transparent background if possible (easy to mask out).

## Files on hand (and what each confirmed)

Top-view shots in this folder, mapped to GRAB's `Family`/`Control` types and the template each refines.
The **measured aspect** (cropped W:H) is what re-calibrated the [design guide](../DESIGN-GUIDE.md) table.

|File|Device|Template|Aspect (W:H)|Calibration|
|-|-|-|-|-|
|`seaboard-m.png`|Seaboard Block (24-key, DNA)|`seaboard-keywave.svg`|2.00|✓ confirms 6 × 3 (282 × 141 mm)|
|`piano-m2.png`|ROLI Piano M / LUMI (24-key)|`lumi-piano-keybed.svg`|2.01|✓ confirms 6 × 3|
|`piano-49-trimmed.png`|ROLI Piano 49|`lumi-piano-keybed.svg` (wide)|3.55|↳ was 4.14 — keys pitched tighter; → ~10.6 × 3|
|`seaboard-rise-25.png`|Seaboard RISE 25|`seaboard-rise.svg`|2.41|↳ was 2.67 → ~7.2 × 3|
|`seaboard-2-silver.png` / `seaboard-2-black.png`|Seaboard RISE 2 (49-key)|`seaboard-rise.svg` (RISE 2 variant)|3.97|→ ~11.9 × 3; pitch/mod edge strips|
|`seaboard-grand.png`|Seaboard GRAND|`seaboard-keywave.svg` (grand radius)|2.78|→ ~8.35 × 3 (this key count)|
|`control-block-trimmed.png`|Control Block (generic / Live)|`control-block.svg`|2.00|✓ confirms 2 × 1 (95 × 48 mm)|
|`loop-block-trimmed.jpg`|Loop Block|`control-block.svg`|2.01|✓ 2 × 1|
|`touch-block.jpg`|Touch Block|`control-block.svg`|2.01|✓ 2 × 1|
|`developer-block.jpg`|Developer Control Block|`control-block.svg`|1.95|angled shot; see note below|

**Developer Block:** no clean top-view exists, but it's just a standard Control Block with **numbered
buttons** instead of icons — so it reuses `control-block.svg` with number legends; no separate trace needed.

**Still missing:** a clean top-down **Lightpad Block M** (94 × 94, square — its 2 × 2 footprint is already
confirmed from ROLI's specs, so this is only for surface/material reference).

A `-detail` suffix is fine for extra close-ups (e.g. `control-live-detail.jpg` for the button icons).

**The DNA blocks are already correctly scaled** (Lightpad 2 × 2, Control 2 × 1, Seaboard Block 6 × 3 — all
consistent with the 94 mm / 47 mm grid). The tracing work left is the **standalone** keybeds (RISE / GRAND /
Piano) — body shape, key spacing, panel layout, button/LED positions — keeping the layer-id contract and
live-overlay geometry intact.

