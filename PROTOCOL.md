# ROLI Seaboard Block — observed SysEx protocol

Status key: **[dev]** = confirmed against our device over USB with seaprobe;
**[src]** = taken from roli_blocks_basics / blocksd source, not yet re-confirmed
on the wire.

Device under test: Seaboard Block, serial `SBBUI5PBW5X03XU9`, firmware `1.4.3`,
model "Seaboard BLOCK (6x3)", connected over **USB** (`usb#vid_2af4&pid_0700`).
BLE port (`...blemidi_seaboardblock3xu9...`) deliberately skipped.

## 1. MIDI transport & port identification [dev]
- The Seaboard presents on Windows as two transports with distinct identifiers:
  - USB: `\\?\usb#vid_2af4&pid_0700#...\seaboardblock`  (ROLI USB VID `0x2af4`)
  - BLE: `\\?\swd#midisrv#midiu_blemidi_seaboardblock3xu9_...`
- We select by identifier (USB `usb#`/`vid_`, BLE `blemidi`/`bthle`). The in and
  out ports of the USB Seaboard share the same identifier string.
- Windows MIDI ports are exclusive: if ROLI Dashboard/Connect/driver or a DAW
  holds the port, `MidiInput/Output::openDevice` fails. seaprobe reports this and
  does not work around it.

## 2. SysEx framing [dev]
Every ROLI message:
```
F0  00 21 10  77   <idx>   <packed 7-bit body...>   <cksum>   F7
^-- sysex    ^--   ^--     ^-- payload              ^-- 1     ^-- end
    start    mfr   ns      (topology index + bits)      byte
```
- Manufacturer ID: `00 21 10` **[dev]** (present in every message).
- Namespace byte after the manufacturer ID:
  - `0x77` main packed protocol — topology / config / control **[dev]**
  - `0x78` block serial-number dump request/response **[src]**
  - `0x49` resetMaster special (never sent by seaprobe) **[src]**
- First body byte is the **topology index** (0..63) **[dev]**.
  - Host→device uses the plain index (e.g. `00`, `2E`=46).
  - Device→host sets bit 6: our block at index 46 replies with `0x6E` = `0x40|0x2E` **[dev]**.
- The byte immediately before `F7` is a checksum **[dev]**, now derived and
  validated (see §8).
- The library strips `F0 00 21 10 77` and the trailing `F7` before decoding; the
  decoder sees `<idx><body>`.
- Body fields are a **7-bit, LSB-first** bit stream: each MIDI data byte carries 7
  payload bits, low bits first; multi-bit fields span byte boundaries **[dev]**.

## 3. Host → device messages

### 3a. Device-control messages (message type `0x01`) [dev]
Form: `F0 00 21 10 77 <idx> 01 <cmd> 00 <cksum> F7` (11 bytes).
`cmd` values (BlocksProtocol::DeviceCommands):

| cmd  | meaning              | observed bytes (idx shown)                     |
|------|----------------------|------------------------------------------------|
| 0x00 | beginAPIMode         | not observed — see note below **[src]**        |
| 0x01 | requestTopology      | `F0 00 21 10 77 00 01 01 00 5D F7` **[dev]**    |
| 0x02 | endAPIMode           | `F0 00 21 10 77 2E 01 02 00 60 F7` **[dev]**    |
| 0x03 | ping (keepalive)     | `F0 00 21 10 77 2E 01 03 00 63 F7` **[dev]**    |
| 0x05 | saveProgramAsDefault | never sent by seaprobe **[src]**               |

### Phase 3 write result (config set) [dev]
Confirmed write of Pressure Sensitivity (id 13) 0 → 64 on fw 1.4.3:
- Sent: `F0 00 21 10 77 2E 10 50 01 10 00 00 00 00 0B F7`
  (idx 46, `10`=configMessage, packed `setConfig`/item=13/value=64, cksum `0B`).
- Immediate read-back (via factory re-sync): **64** — accepted.
- **Persistence across power cycle: CONFIRMED.** After unplugging/replugging USB,
  id 13 still read **64**. So on fw 1.4.3 a plain `setConfig` (0x00) write is
  persisted to flash by the firmware — the separate `setConfigState` (0x06)
  flash/activation command is **not** required to persist a value.
- Restored to original `0` afterwards (read-back `0`). Restore packet:
  `F0 00 21 10 77 2E 10 50 01 00 00 00 00 00 7B F7`.

### 3b. Config messages [dev]
- Factory config sync request (message type `0x10`):
  `F0 00 21 10 77 2E 10 02 44 F7` **[dev]** — triggers the device to stream all
  current config values back.
- Per-item config request / set (`requestConfig`=0x01, `setConfigState`=0x06,
  config-set) exist in source **[src]**; not exercised in Phase 2 (read-only).

## 4. Device → host messages [dev]
- Topology/status reply (42-byte body), carrying type/serial/version/battery:
  `F0 00 21 10 77 6E 00 00 00 00 10 10 20 00 40 29 21 61 6A 64 1A 28 61 6B 1A 2C
   58 19 6C 6A 1C 57 5F 41 4B 21 4C 0B 4D 6B 0C 60 41 2B 00 00 7A F7`
  Decoded by the library to: uid, type=Seaboard (6x3), serial `SBBUI5PBW5X03XU9`,
  fw `1.4.3`, battery, master flag.
- Repeating heartbeat: `F0 00 21 10 77 6E 00 00 00 00 20 00 00 6D F7` (~every 0.5–1s).
- Config updates: streamed in response to the factory-sync request and decoded to
  (id, value, min, max); seaprobe prints them (see §6).

## 5. API mode / keepalive [dev]
- The library keeps the link alive by sending `ping` (cmd 0x03) to the block
  roughly every 400 ms, and by re-sending `endAPIMode`+`beginAPIMode` for any
  block it considers dropped (no device heartbeat for >6 s). seaprobe logs every
  one of these (OUT lines).
- Note: in our sessions the device emitted its heartbeat immediately, so the
  library treated the block as already API-connected and **did not need to send
  beginAPIMode** — config sync still succeeded. beginAPIMode bytes are therefore
  from source only so far.

## 6. Config items [dev]
Full ID catalogue is in `roli_BlockConfigId.h`; names/ranges/defaults in
`roli_BlockConfigManager.h`. seaprobe reads the live values via factory sync.
Spot-checks confirmed against ROLI Dashboard for this device:

| id | name              | device value          | Dashboard |
|----|-------------------|-----------------------|-----------|
| 2  | MIDI Mode         | 2 (Single Channel)    | Single Channel ✓ |
| 3  | Pitch Bend Range  | 48                    | 48 ✓ |
| 4  | Octave            | -2                    | -2 ✓ |
| 7  | Slide Mode        | 0 (Absolute)          | Absolute ✓ |
| 13 | Pressure Sens.    | 0                     | (custom curve) |

- The device also reports a per-item **min/max** that can differ from the static
  defaults (e.g. Pitch Bend Range range came back `[0..96]`, Octave `[-4..5]`).
- **"Persisted" flag:** not directly exposed by this API. `ConfigMetaData.isActive`
  means "the device reported this item in this session", which we print as the
  `active` column. The protocol's flash-persist bit (`setConfigState`) is
  write-only in the library and not read back; determining true persistence will
  need a power-cycle test (Phase 3) or a Dashboard capture.

## 8. Checksum & message bit-layouts [dev]
Derived from `roli_BitPackingUtilities.h` and **validated** by `seadecode` against
real captured packets (serial, battery, firmware and config values all reproduce).

### Checksum
Over `payload` = the bytes after the index byte, excluding the checksum itself:
```
c = (uint8) payloadLength
for each payload byte b:  c = (uint8)(c + (c*2 + b))   // i.e. c = 3*c + b (mod 256)
checksum = c & 0x7f
```
Verified on requestTopology (`5D`), ping (`63`), factory-sync (`44`),
config-set (`0B`), and the 40-byte topology reply (`7A`).

### Field sizes (bits)
MessageType 7 · PacketTimestamp 32 · ProtocolVersion 8 · DeviceCount 7 ·
ConnectionCount 8 · DeviceCommand 9 · ConfigCommand 4 · topologyIndex 7 ·
BatteryLevel 5 · BatteryCharging 1 · ConnectorPort 5 · serial char 7 (×16).

### Packet body
- **host→device:** `<MessageType><message...>` repeated; no timestamp.
  - deviceCommand (MT 0x01): `DeviceCommand(9)` = begin/requestTopology/end/ping/save.
  - config (MT 0x10): `ConfigCommand(4)` then — setConfig: `item(8) value(32)`;
    requestConfig: `0(32) item(8)`; requestFactorySync/UserSync: none.
- **device→host:** `PacketTimestamp(32)` then `<MessageType><message...>` repeated,
  terminated by a 0 MessageType.
  - topology (MT 0x01): `ProtocolVersion(8) DeviceCount(7) ConnectionCount(8)`, then
    per device `serial(16×7) index(7) battery(5) charging(1)`, then per connection
    `idx(7) port(5) idx(7) port(5)`.
  - deviceVersion/deviceName (MT 0x06/0x07): `index(7) len(7) chars(len×7)`.
  - config (MT 0x18): `ConfigCommand(4)` then — updateConfig: `item(8) value(32)
    min(32) max(32)`; setConfig: `item(8) value(32)`; factorySyncEnd/Reset/cluster: none.

**Library inconsistency noted:** `BitSizes::topologyDeviceInfo` is defined as
`serial*7 + battery + charging` (118) but `handleTopologyDevice` also reads a
7-bit `index` (→125). Only a capacity pre-check uses the constant, so it is
harmless in practice; our decoder reads the real 125-bit layout.

## 9. Offline decoding (seadecode) [dev]
`seadecode` (portable, in `libs/sea_sysex`) decodes a hex dump or a USB-MIDI
capture into the above. It reassembles USB-MIDI 32-bit event packets (CIN
0x4–0x7) into SysEx, verifies checksums, and prints each message. Validated: it
reproduces serial `SBBUI5PBW5X03XU9`, fw `1.4.3`, battery, and the full config
set (e.g. MIDI Mode=2, Octave=-2 [-4..5]) from raw captured bytes.

## 10. ROLI Dashboard ground-truth capture [dev]
Captured Dashboard ↔ Seaboard with USBPcap/Wireshark and decoded with `seadecode`.
Note: Dashboard connected over **BLE**, not USB — so the MIDI rode on Bluetooth
ATT (writes = opcode 0x52 host→device, notifications = 0x1b device→host), not
USB-MIDI. BLE-MIDI just wraps each ROLI message with a header + timestamp byte;
`seadecode --blemidi` strips that. **The ROLI SysEx protocol is identical over BLE
and USB**, so this is valid ground truth. seadecode verified 475/476 host writes
and 268/272 notifications (the few failures are lost/split BLE packets).

**What Dashboard sends (host→device), complete list:**
- `ping` (0x03) keepalive · `requestTopology` (0x01) · `requestFactorySync` ·
  and `setConfig item=13 value=N` — one per slider position while dragging
  Pressure Sensitivity (…73, 89, 31, … down to 0).

**Key confirmations:**
- Dashboard writes config with the **same `setConfig` (0x00)** message we use —
  byte-for-byte the same format.
- Dashboard **never** sends `setConfigState` (0x06 flash), `saveProgramAsDefault`,
  `beginAPIMode`/`endAPIMode`, firmware, or factory-reset for a normal edit. This
  independently confirms Phase 3: a plain `setConfig` is persisted by the
  firmware; no explicit flash command is needed, and Dashboard doesn't use one.
- Device notifications decode identically to USB: topology (serial
  `SBBUI5PBW5X03XU9`, battery, fw `1.4.3`), config `updateConfig`/`setConfig`,
  `packetACK`.

## 11. Still open
- The `updateUserConfig` (0x05) layout (user-config metadata) — not yet seen.
- `setConfigState` (0x06) exact fields — Dashboard never sent it, so unobserved.
- Touch / control-button / program-event message layouts (not needed for config).
- A clean **USB** capture (disable the Seaboard's Bluetooth pairing so Dashboard
  uses USB) if we ever want to confirm USB-MIDI framing against Dashboard too.
