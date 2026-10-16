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
- The byte immediately before `F7` is a checksum **[dev]** (its exact algorithm
  is in `roli_BitPackingUtilities.h` / blocksd — not yet re-derived here) **[src]**.
- The library strips `F0 00 21 10 77` and the trailing `F7` before decoding; the
  decoder sees `<idx><body>`.

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

## 7. Open questions / to cross-check with blocksd + a capture
- Checksum algorithm (byte before `F7`).
- Exact bit layout of the 42-byte topology/status body (serial, version, battery).
- The `0x10 02` factory-sync request's full field meaning; is there a user-sync
  variant we should also send?
- Whether beginAPIMode is required on a cold device (ours was already heartbeating).
