# GRAB studio UI: prototype and tokens

This is the design side of the two-track redesign plan in `docs/design/`: a clickable prototype of the
whole window plus the token sheet. It is design material only. Nothing here is wired into the app.

- **Live page:** https://claude.ai/artifact/1xCNVyNNF1Jm7sZfMPuPz8
- **Same page in the repo:** [`prototype/grab-studio.html`](prototype/grab-studio.html). The window is
  pre-rendered into the HTML, so a static read already has the full UI. Every token is a CSS custom
  property on `:root` and is also embedded as JSON.
- **Tokens as data:** [`prototype/tokens.json`](prototype/tokens.json)
- **Rebuild:** `python3 design/ui/prototype/build.py` (needs Google Chrome for the pre-render). It reads
  the device art from `design/device-art/art-v2/models`, so the prototype always shows the current art.

The page covers the 920 x 910 window and a 600-wide version, the four devices (RISE 25, GRAND Stage,
Seaboard Block, and a Seaboard Block + 3 Lightpads + 3 Control Blocks cluster), every control state, the
hierarchy, the 5D graph spec, zones and reflow, and the full token tables.

## The layout

At 920 x 910 nothing scrolls. Every tile edge lines up with the tiles above and below it (one shared
column grid).

1. **Title bar**, merged into the window: window colour, no separate strip, 40 px.
2. **Top row**, three tiles, 120 px tall:
   - **Drawing tile.** The device drawing, or a keystroke map. A small Device / Keys switch sits in its
     top-right corner. The tile is wide enough for the widest arrangement (a 12-unit cluster lock-up).
   - **Device dashboard.** Device name (opens the device / connection menu), then Connection, Battery,
     Firmware, and the settings count (or "Block n of m" in a cluster). Its position never changes.
   - **Controls tile.** Exactly as wide as the inspector below it. Three rows: preset; saving (In sync, or
     Revert and "Save N changes"); device bar (Live view, Read from device, Autosave, Tools).
3. **Graph row**, 334 px: all of the device's 5D curves on one graph, plus the inspector for the selected
   dimension (Min / Sensitivity / Max knobs, Fixed velocity).
4. **Settings tiles**, grouped by job: MIDI output, Expression, Keyboard, Controller CCs, Ports & power.
   Tiles come from the device's groups; a row of three puts its last tile under the inspector.
5. **Status bar**, 28 px: hover help as a footnote, and the version.

At 600 wide the area between title bar and status bar scrolls, the settings tiles fold to a summary line,
and the scrollbar overlays the gutter so the margins stay equal.

## Rules worth keeping

- **One drawing scale.** Every device SVG is 3 units tall and 1 unit = 32 px, in every view and at both
  widths. A Seaboard Block is 192 x 96 alone and inside a cluster. Nothing resizes, so live touches can be
  drawn on it.
- **Type.** Red Hat Display (SIL OFL, embed with `juce_add_binary_data`). Sizes are 20 (knob readouts
  only), 13 (names, reading), 12 (values inside controls, one step below the row name) and 11 (captions,
  units). Sentence case everywhere, with capitals only in acronyms and product names. Tabular figures.
  Interface strings stay ASCII.
- **One stroke.** The selected curve and the knob arcs share one stroke: 4 px, round caps, no glow. The
  other curves are 1.5 px reference lines.
- **Colour.** Greyscale everywhere. The five dimension hues are only for curves, chips, knob arcs and the
  dots that tie a setting to its dimension. Live touches are rings in the Press hue.
- **Unsaved.** A white dot marks anything that differs from the device: on the row, the chip, the tile
  header, and a cluster block's thumbnail. Save and Read clear them. With Autosave on, each change is
  written as it's made, so nothing stays unsaved. That's a change from today, where the dots last until
  a reload.

## Things that need your call or your data

1. **GRAND dimensions.** The plan says GRAND is 4D, but `isGrandField` only shows Glide sensitivity. The
   prototype follows the code (one Glide chip).
2. **Live view and keystroke map.** Both are meant to be fed by the performance MIDI GRAB already reads
   for live sync: note on/off, pitch bend (glide), CC74 (slide) and pressure. On a cluster, Lightpad touches
   would come from the BLOCKS touch data.
3. **Settings regrouping.** The tiles regroup today's groups (MIDI Settings, Play mode, MIDI CC
   Assignments, Power) by job, and Slide CC moves next to Slide mode. That needs a group field in the field
   table.
4. **Sentence-case labels** mean changing GRAB's own strings (and their `TRANS` keys), for example
   "Pitch Bend Range" to "Pitch bend range".
5. **Title bar.** macOS keeps the native traffic lights with a transparent, full-size-content title bar.
   JUCE has no switch for this, so it needs a few lines of Objective-C++ on the peer's `NSWindow`
   (`titlebarAppearsTransparent`, `NSWindowStyleMaskFullSizeContentView`, hidden title), plus a 78 px
   inset on the left for the lights. On Windows and Linux, use `setUsingNativeTitleBar (false)` and draw
   the bar in `GrabLookAndFeel`, with minimise / maximise / close on the right.
6. **Minimum window.** 600 x 700 is suggested (`setResizeLimits`). GRAB has no limit today.
7. **Placeholders in the cluster demo:**
   - Touch and Loop Block button names are "Button 1-8".
   - The Lightpad scale options are examples.
   - Sleep time is shown as a raw number (units unknown).

## Device art in this PR

The RISE and GRAND casings are now flat (`rise.trace.flat`, `grand.trace.flat` in
`design/device-art/generator/style.json`):

- **RISE:** no light lip, no darker front band.
- **GRAND:** no lit front lip, a plain master button, and the pills drawn without halos or toes.

Only RISE and GRAND files change. Running the generator also prints a validator note that the Control
Blocks no longer carry `button-leds` / `btnled-*`. That follows from your change to light the icon instead
of a ring, so the validator's id list may want updating.
