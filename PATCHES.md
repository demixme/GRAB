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

### P1 — `dynamic_cast` for custom DeviceConnections (Phase 2)
**File:** `topology/internal/roli_ConnectedDeviceGroup.cpp` (3 sites: group ctor,
`initialiseVersionReader`, `initialiseSerialReader`)
**Change:** `static_cast<MIDIDeviceConnection*>` → `dynamic_cast<MIDIDeviceConnection*>`.

**Why:** `PhysicalTopologySource` has a public custom-transport API
(`DeviceDetector`/`DeviceConnection`). seaprobe uses it to own port selection and
log raw traffic. Our `DeviceConnection` is intentionally **not** a
`MIDIDeviceConnection`. The original unchecked `static_cast` then (a) dereferenced
our object at the wrong type to lock `criticalSecton`, and (b) constructed the
serial/version readers against it — both undefined behaviour / crashes. With
`dynamic_cast` the cast yields `nullptr` for a custom connection, so the group
takes its lock-free path and skips the MIDI-only serial/version readers. This
matches how the same file's `isBluetoothConnection()` already probes this pointer
(`dynamic_cast<const MIDIDeviceConnection*>`), so it is a consistency fix, not a
behaviour change for the stock path. Serial/firmware/battery still arrive via the
main topology handshake, so nothing is lost for a single USB block.

**Behavioural impact on wire protocol:** none.

### P2 — config comms use the base DeviceConnection (Phase 2)
**File:** `topology/internal/roli_BlockImplementation.cpp`, `updateMidiConnectionListener()`
**Change:** pass the base `DeviceConnection*` (from `getDeviceConnectionFor`) to
`config.setDeviceComms(...)`, instead of the `dynamic_cast<MIDIDeviceConnection*>`
result.

**Why:** `BlockConfigManager::setDeviceComms` takes the base
`PhysicalTopologySource::DeviceConnection*` and sends through it. The original code
passed the `MIDIDeviceConnection` downcast, which is `nullptr` for a custom
transport, so `buildAndSendPacket` hit its `deviceConnection == nullptr` early-out
and **silently dropped every config request/set**. Symptom: `requestFactoryConfigSync()`
produced no bytes and all config items read back as inactive defaults. The
`MIDIDeviceConnection` cast is still used, unchanged, for the raw-MIDI `addListener`.

**Behavioural impact on wire protocol:** none for the stock path (same object either
way); enables config traffic for custom transports.

## Safety note (for Phase 2, not yet acted on)
The module exposes these destructive operations as explicit methods only — none
observed to fire automatically from this grep, but the connect/topology
handshake must still be audited before any device run:
- `Block::factoryReset()`
- `Block::sendFirmwareUpdatePacket()`
- `Block::saveProgramAsDefault()`
- Littlefoot program upload via `LittleFootRemoteHeap` / `HostPacketBuilder`
