# FabFilter-inspired UI redesign — two-track plan

A plan for bringing GRAB's whole UI into a FabFilter-inspired visual language. It splits
the work into two lanes so each side does what it's best at: **Fred designs; we translate
to code.** The UI redesign touches the device I/O, presets, and live-sync wiring, so the
code half must be done by someone who can verify none of that breaks — not shipped into
the repo unsupervised.

This replaces the earlier "point Fred's session at the repo" idea.

---

## Why two tracks

The device art worked as a straight handoff (Fred draws SVGs, we drop them in) because the
art can't break anything functional. The UI redesign is different: it reworks
`GrabLookAndFeel`, the layout of both host pages, the config widgets, and the paths that
write to the device. The risk isn't "does it look right" (Fred can judge that) — it's
"did Save-to-device, Autosave, presets, GRAND's 4D handling, and the throttled live-writes
survive the refactor." That's exactly what a non-coder can't test, so the code lands on our
side, verified against CI and a real device.

---

## Track A — Fred (design)

**Deliverable: a clickable prototype + a token sheet.** Both are design artifacts, not
production code.

### A1. A clickable prototype (HTML/React artifact)
Nails the look and the interactions so there's nothing to guess when we translate:
- The full window at a realistic size, and at a **reduced width** (GRAB is resizable).
- All four zones laid out: **header**, **device graphic**, **main settings panel**,
  **footer**.
- The **5D response graph** as the centrepiece — all five curves overlaid on one graph,
  with the hover/select/drag behaviour shown.
- **Control states** visible: normal / hover / active / "changed-from-device" for buttons,
  sliders, combo boxes, and the curves.

> A static-leaning prototype is ideal — if the artifact is plain HTML/CSS we can read the
> exact tokens straight out of it. A heavy dynamic app renders nothing to a static read.

### A2. A token sheet (can be a panel in the same artifact, or a short doc)
The concrete values we'll port into the theme:
- **Colours** — every hex: backgrounds, panels, borders, text (primary/secondary), accent,
  and the five 5D colours (current ones: Strike `#0be4f4`, Glide `#73fe91`, Slide
  `#ff77fd`, Press `#52d7ba`, Lift `#fbc435` — keep or restate).
- **Typography** — font family, sizes, weights per role (titles, labels, values).
- **Spacing & shape** — the spacing grid, panel/card corner radii, border widths, padding.
- **Zone proportions** — header / device-graphic / main-panel / footer heights, and how
  they reflow when the window narrows.
- **5D graph spec** — axes and grid, how a curve is selected and dragged, hover focus
  (active brightens/forward, others dim), and the min/max end-node handles.

Fred can drive his **own** Claude session for Track A (build the prototype, extract the
tokens from his existing artifact) — that's design work, and the ideal use of his session.

---

## Track B — us (translate to JUCE/C++)

We port Fred's prototype + tokens into GRAB, on a branch, verified on CI and a device.

### Where each piece lives (component map)

| Zone | Component (file) | Notes |
|---|---|---|
| **App-wide look** | `GrabLookAndFeel` ([`src/gui/Theme.h`](../../src/gui/Theme.h)) | The single central hook (set in [`Main.cpp`](../../src/gui/Main.cpp)). Most of the restyle — colours, fonts, control painting — goes here and propagates app-wide. |
| **Header** | `DeviceInfoBar` ([`DeviceInfoBar.h`](../../src/gui/DeviceInfoBar.h)) + the button row in `MainComponent` | Battery/name/fw/transport + Diagnose/Reload/Save/Autosave/Transport/device-switcher/Advanced. |
| **Device graphic** | `KeyboardView` ([`KeyboardView.h`](../../src/gui/KeyboardView.h)) | Recently rebuilt on Fred's SVG art, adaptive height. Restyle the frame/placement; keep the art + live touch/fader sync. |
| **Settings** | `SectionCard` ([`SectionCard.h`](../../src/gui/SectionCard.h)) + `ConfigRow` / `CurveComponent` | Titled cards in a scrolling viewport. |
| **Footer** | `FooterBar` ([`FooterBar.h`](../../src/gui/FooterBar.h)) | Version, hover-help, update notice. |
| **RISE/GRAND page** | `Rise2Component` ([`Rise2Component.h`](../../src/gui/Rise2Component.h)) | Its own header/reload/save/viewport — mirror the same restyle. |

Two host pages share these widgets (the **BLOCKS page in `MainComponent`** and
**`Rise2Component`**) — anything new drops into both.

### The 5D overlay graph
- **Reuse the curve math.** [`CurveComponent.h`](../../src/gui/CurveComponent.h) already maps
  sensitivity 0–127 to the exact shape traced from Dashboard 4.7.2 (+ min/max nodes).
  Extract it into a shared function so the overlay draws identical curves.
- **Dimensions/colours/params** — [`Rise2Component.cpp`](../../src/gui/Rise2Component.cpp):
  `kStrikeSens…kLiftSens` (`0x1a/0x17/0x18/0x19/0x1b`), `*Min`/`*Max` (`0x46`–`0x4b`), and
  the `{sensId, name, colourARGB, minId, maxId}` table.
- **Don't touch the write path** — a drag sets `kXxxSens` via the existing `onItemChanged`
  / `CurveComponent::setSensitivity`, which already throttled-writes and tracks "changed".

### Guarantees we hold (must-not-break)
- **Device I/O** unchanged — this is presentation only.
- **Presets** (built-in + user), **Reload**, **Save / Autosave**, **live sync**, and the
  per-field **"changed" dots** all preserved.
- **GRAND is 4D** — no Slide/Strike; drive the dimension list from the field table
  (`fieldShown` / `isGrandHidden`), never a hardcoded five.
- **Device graphic + live touch/fader mirror** kept intact.
- Verified on **Windows + macOS CI** and with the preview shortcuts (`Ctrl/⌃+Alt/⌥+Shift+`
  `R` RISE / `G` GRAND / `L`·`P` LUMI·Piano / `B` Lightpad / `C` Control — see
  [`docs/DEV-SHORTCUTS.md`](../DEV-SHORTCUTS.md)) and a real device where possible.

### Order of work (low-risk first)
1. **Tokens + LookAndFeel** — port colours/fonts/shape into `Theme.h` / `GrabLookAndFeel`;
   the whole app shifts at once.
2. **Zones** — restructure `MainComponent` (mirror in `Rise2Component`) to the artifact's
   proportions and reflow.
3. **Panels** — restyle `SectionCard` / `ConfigRow`.
4. **5D overlay graph** — the new shared component replacing the `CurveComponent` row.

---

## Hand-off checklist

- [ ] Fred: clickable prototype (full + narrow width, all zones, 5D graph, control states)
- [ ] Fred: token sheet (colours, type, spacing/shape, zone proportions, 5D graph spec)
- [ ] Us: port tokens → `GrabLookAndFeel` / `Theme.h`
- [ ] Us: zones → `MainComponent` + `Rise2Component`
- [ ] Us: restyle `SectionCard` / `ConfigRow`
- [ ] Us: 5D overlay graph (shared curve math)
- [ ] Us: verify I/O, presets, live sync, GRAND 4D on CI + device
