# GRAB dev shortcuts (device previews, no hardware)

GRAB has a set of hidden keyboard shortcuts that render a device page from **synthetic data** — no
hardware connected. They're for working on UI for instruments you don't have in front of you (and for
A/B-ing device art). The GRAB window must have **keyboard focus** (it grabs focus on launch).

Implemented in [`src/gui/MainComponent.cpp`](../src/gui/MainComponent.cpp) `keyPressed()`.

## The chord

- **Windows / Linux:** `Ctrl + Alt + Shift + <letter>`
- **macOS:** `Control + Option + Shift + <letter>` — i.e. `⌃ + ⌥ + ⇧ + <letter>`.
  Note it is the literal **Control** key, **not** Command (`⌘`); the handler checks `isCtrlDown()`.

## Shortcuts

| Device preview | Windows / Linux | macOS | Notes |
|---|---|---|---|
| **Seaboard RISE** | `Ctrl+Alt+Shift+R` | `⌃⌥⇧R` | Each press toggles **RISE 2 (verified)** ↔ **original RISE**. |
| **ROLI Piano** (49) | `Ctrl+Alt+Shift+P` | `⌃⌥⇧P` | Simulated Piano config page. Press again to exit. See [PIANO.md](../PIANO.md). |
| **Piano M / LUMI Keys** (24) | `Ctrl+Alt+Shift+L` | `⌃⌥⇧L` | Four modes' colour scheme + editable key-colour picker. Press again to exit. See [LUMI.md](../LUMI.md). |
| **Seaboard GRAND** | `Ctrl+Alt+Shift+G` | `⌃⌥⇧G` | GRAND page on the RISE component (4D, 3 pedals), from a synthetic `0x3a`. See [RISE-GRAND.md](../RISE-GRAND.md). |
| **Lightpad** (+ DNA layout) | `Ctrl+Alt+Shift+B` | `⌃⌥⇧B` | A 5-tile DNA arrangement (incl. a Seaboard Block and a Control block). Press again to exit. |
| **Control Block** | `Ctrl+Alt+Shift+C` | `⌃⌥⇧C` | Each press cycles the subtype: **Loop → Live → Touch → Developer**. |
| **BLOCKS app LED preview** | `Ctrl+Alt+Shift+A` | `⌃⌥⇧A` | Opens the app inspector and runs its built-in LED demo. |

There is no standalone **Seaboard Block** shortcut — it appears inside the Lightpad/DNA layout (`B`).

## Related: previewing the new device art (V2)

The designer's SVG device art is being wired in behind a toggle (phase 1 = Control blocks). To compare
it against the hand-drawn art:

1. Summon a device preview — e.g. `Ctrl+Alt+Shift+C` to cycle the Control-block subtypes.
2. **Advanced… → New device art (preview)** toggles the SVG kit on/off (persisted in `settings.properties`).

Only families already wired carry V2 art; the rest fall back to the drawn art automatically.
