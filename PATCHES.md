# Patches to `roli_blocks_basics`

The module is **vendored** at `third_party/roli_blocks_basics/` (its upstream
`.git` was removed), so every change we make shows up as a tracked diff in this
repo.

- **Upstream:** https://github.com/WeAreROLI/roli_blocks_basics
- **Vendored commit:** `212ba4e237638b9ae8c2a4f76dd2070110392031`
  (2026-06-29, "Merge ROLI Piano and Airwave support")
- **Module version:** 6.0.1
- **Built against:** JUCE 8.0.15

The first commit of `third_party/roli_blocks_basics/` is the pristine upstream
copy, so `git log` on that subtree is the authoritative patch list. This file is
the human-readable summary.

## Policy
- Each patch is the **minimum** needed to compile/link against JUCE 8, one
  concern per commit.
- No behavioural change to the on-the-wire protocol.
- Nothing here enables or auto-triggers firmware update, factory reset,
  program (Littlefoot) upload, `saveProgramAsDefault`, or bootloader entry.

## Patches

_(none yet — Phase 1 build in progress)_

## Safety note (for Phase 2, not yet acted on)
The module exposes these destructive operations as explicit methods only — none
observed to fire automatically from this grep, but the connect/topology
handshake must still be audited before any device run:
- `Block::factoryReset()`
- `Block::sendFirmwareUpdatePacket()`
- `Block::saveProgramAsDefault()`
- Littlefoot program upload via `LittleFootRemoteHeap` / `HostPacketBuilder`
