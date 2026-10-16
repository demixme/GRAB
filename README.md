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
- **Phase 3 — Single approved write (`seaprobe set`):** wrote one config value
  (`setConfig`), read it back, and confirmed it persists across a power cycle on
  fw 1.4.3. Dry-run by default; `--commit` to transmit. _(done)_
- **Phase 4 — Minimal GUI (`SeaboardConfig`):** JUCE desktop app showing connection
  status and one control per device-reported config item (sliders/toggles/combos,
  generated — not hard-coded), with Save to device and JSON preset load/save. _(done)_

## Run the probe
```sh
cmake --build build --config Debug --target seaprobe
./build/seaprobe_artefacts/Debug/seaprobe.exe list        # enumerate ports only
./build/seaprobe_artefacts/Debug/seaprobe.exe raw   15     # raw SysEx hex dump, 15s
./build/seaprobe_artefacts/Debug/seaprobe.exe blocks       # decode topology + config
./build/seaprobe_artefacts/Debug/seaprobe.exe set 13 64 --commit   # write one value
```
Transport: add `--transport usb|ble|auto` (default **auto** = prefer USB, fall
back to BLE). The ROLI protocol is identical over both. Logs → `logs/seaprobe-<time>.log`.

Multiple blocks (connected via DNA connectors) are all detected; `blocks` dumps
each one's config, and `set` targets a specific block with `--serial <S>` (defaults
to the first block).

## Run the GUI
```sh
cmake --build build --config Debug --target SeaboardConfig
./build/SeaboardConfig_artefacts/Debug/SeaboardConfig.exe
```
Shows connection status and one control per device-reported config item. Edits are
local (changed rows are highlighted) until **Save to device**. Presets load/save as
JSON. A **Transport** dropdown selects Auto / USB / BLE (Auto prefers USB, falls
back to BLE); changing it reconnects. A **Block** dropdown selects which connected
block to view/edit when more than one is attached.

## Offline decoder (`seadecode`)
Portable CLI (no JUCE) that decodes ROLI SysEx — hex dumps or USB-MIDI captures —
into framing, direction, checksum result, and decoded messages.
```sh
cmake --build build --config Debug --target seadecode
./build/Debug/seadecode.exe F0 00 21 10 77 00 01 01 00 5D F7     # decode hex
./build/Debug/seadecode.exe --usbmidi --file usbcapdata.hex       # a USB-MIDI capture
tshark -r cap.pcapng -Y usb.capdata -T fields -e usb.capdata | ./build/Debug/seadecode.exe --usbmidi -
```
Validated against real device bytes (reproduces serial, firmware, battery, and the
full config set). Checksum algorithm and bit layouts are in `PROTOCOL.md`.

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
