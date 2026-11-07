# FabFilter-inspired 5D sensitivity UI — design brief

A handoff brief for building the new 5D sensitivity display in GRAB: one large
response graph that overlays all five dimensions, the way FabFilter Pro-Q overlays
its EQ bands, replacing the current row of five separate curve graphs.

Sam flagged this direction (the "5D as a larger overlay like FabFilter" / response-graph
mockup). This doc is the starting context for that work.

## Goal

Replace the row of five separate sensitivity curve graphs with **one large response
graph that overlays all five 5D curves** — **Strike, Glide, Slide, Press, Lift** — on a
shared grid. Each dimension is a coloured curve; you read the whole feel of the
instrument at a glance and drag any curve to set that dimension's sensitivity.

## What exists today (build on this — don't rewrite the data path)

- **[`src/gui/CurveComponent.h`](../../src/gui/CurveComponent.h)** — the per-dimension
  widget. It already contains the **response-curve math**: it maps a sensitivity 0–127
  to the exact curve shape measured from ROLI Dashboard 4.7.2, plus min/max end-nodes for
  Strike/Press/Lift. **Extract that curve-shape math into a shared free function** (e.g.
  `grab::sensitivityCurve(dimension, sens, min, max)`), so the overlay graph draws
  identical curves — the device feel must match what Dashboard shows.
- **Dimensions, colours, param ids** — [`src/gui/Rise2Component.cpp`](../../src/gui/Rise2Component.cpp):
  - `kStrikeSens … kLiftSens` (SysEx params `0x1a / 0x17 / 0x18 / 0x19 / 0x1b`) and the
    `*Min` / `*Max` ids (`0x46`–`0x4b`).
  - the `{ sensId, name, colourARGB, minId, maxId }` table (Glide/Slide have no min/max).
  - colours in [`src/gui/Theme.h`](../../src/gui/Theme.h):
    Strike `#0be4f4`, Glide `#73fe91`, Slide `#ff77fd`, Press `#52d7ba`, Lift `#fbc435`.
- **Two hosts use the curves** — `Rise2Component` (RISE / GRAND) and the BLOCKS page in
  `MainComponent` (the `curves` / `curveRows` vectors). Build the new graph as **one
  reusable component** that both drop in.
- **Live write path** — don't touch the device protocol. Reuse `onItemChanged(id, value)`
  / `CurveComponent::setSensitivity`: dragging a curve sets its `kXxxSens` value, which
  already throttled-writes to the device and marks the field "changed".

## Design intent

- One graph, full width of the Sensitivity card, roughly 2–3× the current height. X = input
  (touch), Y = response, with shared axes and grid.
- Five curves overlaid, each in its dimension colour. The **hovered / active** dimension
  brightens and comes to the front; the others dim (FabFilter-style focus). A small legend
  or per-curve handle selects and drags a dimension.
- Drag a curve vertically → that dimension's sensitivity. The Strike/Press/Lift **min/max
  end nodes** stay draggable (they are real params `0x46`–`0x4b`).
- Keep everything else (MIDI Settings, Play mode, Pedals, MIDI CC Assignments) exactly as
  is — **only the Sensitivity section** becomes the overlay graph.

## Constraints / gotchas

- **JUCE only** (`juce_gui_basics`), no new dependencies. Match the existing dark theme and
  the accent/hover system.
- **GRAND is 4D** — no Slide, no Strike (see `isGrandHidden` / `fieldShown` in
  `Rise2Component.cpp`); the graph must show only the applicable dimensions. BLOCKS may
  expose a different subset too — drive the curve list from the field table, never a
  hardcoded five.
- Preserve **presets, reload, live sync, and the per-curve "changed" dots** (differs-from-device
  state).
- The device exposes one sensitivity per dimension (plus min/max); the curve *shape* is
  interpolated, not free-draw. Keep it faithful to `CurveComponent`'s current math.

## Deliverable

A new `SensitivityGraph` component (overlay of N dimension curves) that replaces the row
of `CurveComponent`s in both hosts, sharing the extracted curve math. Builds clean on the
Windows + macOS CI. Verify with the hidden preview shortcuts — `Ctrl/⌃ + Alt/⌥ + Shift + R`
for RISE (5D), `… + G` for GRAND (4D) — see [`docs/DEV-SHORTCUTS.md`](../DEV-SHORTCUTS.md).
