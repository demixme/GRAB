# SeaboardConfig

A standalone tool to read and write ROLI Seaboard Block settings directly over
MIDI SysEx, with no dependency on ROLI Dashboard, ROLI Connect, the ROLI
hardware driver, or any login.

Built with **JUCE 8** + **CMake** (no Projucer), targeting Windows 10/11 x64
(MSVC). Code is kept portable.

## Status
- **Phase 1 — Build:** CMake project fetches JUCE 8 and builds `roli_blocks_basics`
  as a JUCE module. _(done)_
- **Phase 2 — Read-only probe (`seaprobe`):** lists MIDI ports (USB selected, BLE
  skipped), dumps raw SysEx with timestamps, and decodes topology (type, serial,
  firmware, battery) + all config items via the library over a custom logging
  transport. Observations recorded in `PROTOCOL.md`. _(done)_
- Phase 3 — Single approved write. _(not started)_
- Phase 4 — Minimal GUI. _(not started)_

## Run the probe
```sh
cmake --build build --config Debug --target seaprobe
./build/seaprobe_artefacts/Debug/seaprobe.exe list        # enumerate ports only
./build/seaprobe_artefacts/Debug/seaprobe.exe raw   15     # raw SysEx hex dump, 15s
./build/seaprobe_artefacts/Debug/seaprobe.exe blocks       # decode topology + config
```
Logs are written to `logs/seaprobe-<time>.log`.

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
