# SeaboardConfig

A standalone tool to read and write ROLI Seaboard Block settings directly over
MIDI SysEx, with no dependency on ROLI Dashboard, ROLI Connect, the ROLI
hardware driver, or any login.

Built with **JUCE 8** + **CMake** (no Projucer), targeting Windows 10/11 x64
(MSVC). Code is kept portable.

## Status
- **Phase 1 — Build:** CMake project fetches JUCE 8 and builds `roli_blocks_basics`
  as a JUCE module. Target `seaprobe` proves the module compiles/links. _(current)_
- Phase 2 — Read-only probe. _(not started)_
- Phase 3 — Single approved write. _(not started)_
- Phase 4 — Minimal GUI. _(not started)_

## Layout
- `third_party/roli_blocks_basics/` — vendored, patched for JUCE 8 (see `PATCHES.md`).
- `third_party/blocksd/`, `third_party/myblocks/` — reference implementations, gitignored.
- `src/seaprobe/` — console probe (Phase 1 stub; Phase 2 real logic).

## Build (Windows, VS 2026 / MSVC)
```sh
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug --target seaprobe
```
The first configure downloads and builds JUCE 8 (`JUCE_TAG`, default 8.0.15).

## Safety
Phases 1–2 are strictly read-only. The tool never sends firmware-update,
program-upload, factory-reset or bootloader messages. It uses the **USB** MIDI
port only and deliberately skips the BLE MIDI port.
