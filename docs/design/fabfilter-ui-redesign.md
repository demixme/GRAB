# FabFilter-inspired UI redesign — design brief

A handoff brief for restyling **the whole GRAB UI** in a FabFilter-inspired visual
language — not just the 5D curves. Fred's design artifact is the **canonical visual
spec** (layout, colours, typography, spacing); this doc maps that design onto GRAB's
current code so the work lands without breaking the device I/O, presets, or live sync.

> **The artifact is the source of truth for *how it looks*.** This doc is the source of
> truth for *what to change in the code* and *what must not break*. Open Fred's artifact
> alongside this.

## Scope

Reskin and re-lay-out the entire app in the FabFilter style: the window chrome, the
device graphic, the settings panels, and — as the centrepiece — the **5D response graph
overlaying all five dimensions** (see its own section below). Keep every existing feature
and all device behaviour; this is a presentation-layer change, not a protocol change.

## Current UI → what to restyle (component map)

| Zone | Component (file) | Notes |
|---|---|---|
| **App-wide look** | `GrabLookAndFeel` ([`src/gui/Theme.h`](../../src/gui/Theme.h)) | The single central hook — set as the default LookAndFeel in [`src/gui/Main.cpp`](../../src/gui/Main.cpp). Most of the FabFilter restyle (colours, fonts, control painting) goes **here**, so it applies everywhere at once. |
| **Header** | `DeviceInfoBar` ([`DeviceInfoBar.h`](../../src/gui/DeviceInfoBar.h)) + the button row in `MainComponent` | Battery icon+%, device name, firmware, transport; plus Diagnose / Reload / Save / Autosave / Transport / device switcher / Advanced. |
| **Device graphic** | `KeyboardView` ([`KeyboardView.h`](../../src/gui/KeyboardView.h)) | The device render (recently rebuilt on Fred's SVG art, adaptive height). Restyle the frame/placement to match, but keep the art + live touch/fader sync. |
| **Settings** | `SectionCard` ([`SectionCard.h`](../../src/gui/SectionCard.h)) holding `ConfigRow`s + `CurveComponent`s | Titled cards (Sensitivity, MIDI Settings, Play mode, Pedals, CC Assignments). Laid out in a scrolling `viewport`. |
| **Footer** | `FooterBar` ([`FooterBar.h`](../../src/gui/FooterBar.h)) | Version, hover-help text, update-available notice. |
| **RISE/GRAND page** | `Rise2Component` ([`Rise2Component.h`](../../src/gui/Rise2Component.h)) | Its own header/reload/save/viewport; mirror the same restyle. |

Two host pages share these widgets — **the BLOCKS page in `MainComponent`** and
**`Rise2Component`**. Anything new (a shared graph, restyled cards) must drop into both.

## Design system

- **Start in `GrabLookAndFeel`.** Pull Fred's exact hex colours, font family/sizes, corner
  radii, and spacing from the artifact into the theme tokens in `Theme.h` (current tokens:
  `windowBg`, `panelBg`, `panelBgAlt`, `accent`, `textPrimary/Secondary`, `outline`, and
  the 5D colours). Override the relevant `LookAndFeel_V4` paint methods (buttons, sliders,
  combo boxes, scrollbars) so the whole app shifts at once.
- **JUCE only** (`juce_gui_basics` / `juce_gui_extra`), no new dependencies. Keep the
  existing dark base; FabFilter is a refinement of it, not a rewrite of the rendering.
- Match the artifact's **panel language** (card backgrounds, borders, section headers) in
  `SectionCard` and `ConfigRow`.

## The 5D response graph (centrepiece)

Replace the row of five separate curve graphs with **one large graph overlaying all five
curves** — Strike, Glide, Slide, Press, Lift — FabFilter Pro-Q style.

- **Reuse the curve math.** [`CurveComponent.h`](../../src/gui/CurveComponent.h) already maps
  a sensitivity 0–127 to the exact shape measured from ROLI Dashboard 4.7.2 (+ min/max end
  nodes for Strike/Press/Lift). **Extract that into a shared function** so the overlay draws
  identical curves — the feel must match Dashboard.
- **Dimensions / colours / params** — [`Rise2Component.cpp`](../../src/gui/Rise2Component.cpp):
  `kStrikeSens…kLiftSens` (SysEx `0x1a/0x17/0x18/0x19/0x1b`), `*Min`/`*Max` (`0x46`–`0x4b`),
  and the `{sensId, name, colourARGB, minId, maxId}` table. Colours (`Theme.h`): Strike
  `#0be4f4`, Glide `#73fe91`, Slide `#ff77fd`, Press `#52d7ba`, Lift `#fbc435`.
- **Interaction:** hovered/active dimension brightens and comes to front, others dim; drag a
  curve vertically to set its sensitivity; Strike/Press/Lift min/max nodes stay draggable.
- **Don't touch the write path** — dragging sets the `kXxxSens` value through the existing
  `onItemChanged` / `CurveComponent::setSensitivity`, which already throttled-writes to the
  device and tracks the "changed" state.

## Constraints / must-not-break

- **GRAND is 4D** — no Slide, no Strike (`isGrandHidden` / `fieldShown` in `Rise2Component`);
  drive the dimension list from the field table, never a hardcoded five.
- Preserve **presets (built-in + user), Reload, Save/Autosave, live sync, and the per-field
  "changed" dots**.
- Keep the **device graphic** (`KeyboardView`) and its live touch/fader mirror intact.
- Keep every config field and the device I/O untouched — this is presentation only.

## Suggested approach

1. **LookAndFeel + tokens first** — land the colour/font/control restyle in `GrabLookAndFeel`
   and `Theme.h`; the whole app shifts with low risk.
2. **Zone layout** — restructure `MainComponent` (and mirror in `Rise2Component`) to the
   artifact's header / device-graphic / main-panel / footer proportions.
3. **Panels** — restyle `SectionCard` / `ConfigRow`.
4. **5D overlay graph** — the new shared component, replacing the `CurveComponent` row.

Do it on a branch, behind the existing patterns; it builds on Windows + macOS CI. Verify
devices with the preview shortcuts — `Ctrl/⌃ + Alt/⌥ + Shift + R` (RISE 5D), `… + G`
(GRAND 4D), `… + L`/`P` (LUMI/Piano), `… + C` (Control) — see
[`docs/DEV-SHORTCUTS.md`](../DEV-SHORTCUTS.md).
