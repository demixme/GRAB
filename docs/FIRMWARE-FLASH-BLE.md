# On-wire firmware flash — Seaboard Block over BLE (decoded)

How ROLI Dashboard actually flashes a **Seaboard Block** on the wire, reverse-engineered
from a live capture of Dashboard reflashing a Block over Bluetooth LE. This is the
companion to [PROTOCOL.md §12](../PROTOCOL.md#12-firmware-update-mechanism-dev) (the
firmware *fetch* chain + the `.syx` file format) and
[tools/FIRMWARE-CAPTURE.md](../tools/FIRMWARE-CAPTURE.md) (archiving the firmware over
HTTPS). Everything here is **reverse-engineering / preservation reference** — the shipping
GRAB does not flash firmware.

## Why this was worth decoding

ROLI is in administration. Owners of second-hand or new-old-stock hardware are stranded:
downloading ROLI Dashboard requires *registering* a device, and that is blocked when the
device is locked to a previous owner's account or is brand-new and unregisterable — with
no ROLI Support left to unlock it. The firmware *fetch* needs no account (a static
`X-API-Key`, see §12), so the only missing piece for an account-free recovery tool was the
**on-wire flash protocol**. This is that piece.

## The question it settles

§12 documents the `.syx` file as a stream of `0x47`-namespace messages and notes that
ROLI's own "no-Dashboard" method (for RISE) streams that `.syx` more or less verbatim with
a generic SysEx tool. The open question was: **when Dashboard flashes a Block, does it send
those raw `0x47` messages, or does it repackage them?**

**Answer: for a Block over BLE, Dashboard does NOT send raw `0x47`. It transcodes each
`.syx` message 1:1 into a `firmwareUpdatePacket` — host message type `0x04` carried in the
`0x77` packedProtocol.** The `.syx` is a transport-neutral *container*; the Block flash path
re-emits its contents as in-band `0x77` host messages. (RISE is different — see the end.)

## Capture method (BLE HCI, below the app)

Earlier attempts failed because the flash is invisible at the app layer: USBPcap can't see
USB-MIDI class traffic, and macOS MIDI Monitor's CoreMIDI spy only catches the Block's
heartbeat — Dashboard's firmware path is a private BLE route. The flash can only be seen
**below the app, at the Bluetooth HCI layer**:

1. **Wireshark ≥ 4.7.3** (we used 5.0.0rc2) — older builds lack the Bluetooth ETW provider.
2. Run Wireshark **as Administrator** → interface **`etwdump`** → provider
   **"Bluetooth Host Radio (Microsoft-Windows-BTH-BTHPORT)"**.
3. Start the capture, then in Dashboard do **Manually update firmware** and point it at a
   `.syx` (a same-version reflash works and is the lowest-risk way to capture the full
   sequence). Let it finish and the device reboot; stop the capture; save as `.pcapng`.
4. Decode with `tools/decode-capture.sh <cap>.pcapng` (auto-detects BLE and runs
   `seadecode --blemidi` on the ATT writes `0x52` and notifications `0x1b`).

On the wire: host→device is **ATT Write Command (opcode `0x52`)** on handle **`0x001b`**
(the BLE-MIDI characteristic); device→host is **Handle Value Notification (opcode `0x1b`)**.
Long SysEx is split across BLE-MIDI fragments; `seadecode --blemidi` reassembles them.

## What a flash looks like (seaboard 1.4.3 → 1.4.3, same-version reflash)

15,200 ATT writes reassembled to **3,674 logical SysEx messages**; the device confirmed
`deviceVersion '1.4.3'` after rebooting.

### 1:1 transcode — `.syx` `0x47` → on-wire `0x77` msg type `0x04`

Each `firmwareUpdatePacket` is `F0 00 21 10 77 <deviceIndex> 04 <sub> … F7`. There were
exactly **2882** of them — the same count as the 2882 messages in the `.syx`:

| `.syx` message (`0x47`)            | on-wire `firmwareUpdatePacket` (`0x77`/`0x04`)        | count |
|------------------------------------|-------------------------------------------------------|-------|
| `47 00` manifest / START           | `04 69 00 "ROLI BLOCKS Update\r\nV:1.4.3\r\nI:seaboard…"` | 1  |
| `47 04 / 05 / 07` DATA (addr-stepped) | `04 50 04 <addr…> <payload…>` — addr steps `0x20`   | 2880 |
| `47 01` commit / END (CRC+size)    | `04 07 01 …` (CRC/size trailer)                       | 1    |

The manifest's keyed ASCII fields (`V:`/`I:`/`M:`/`m:`/`S:`/`s:`/`B:`/`b:`) match the
`.syx` header documented in §12. The data address steps by `0x20` (32-byte aligned), 7-bit
packed — the flat memory image from the `.syx`, re-wrapped.

### Flow control — every packet is ACKed

The device replies **`firmwareUpdateACK` = device message type `0x03`** — **2882** of them,
one per firmware packet (strict 1:1 handshake, i.e. the pacing mechanism). Dashboard
interleaves **`ping` (deviceCommand `0x03`)** writes as keepalive (≈773 over the transfer).

### Full sequence

1. `beginAPIMode` (`0x00`), `requestTopology` (`0x01`) — handshake.
2. Manifest packet (`04 69 00 …`).
3. 2880 data packets (`04 50 04 <addr> …`), **each ACKed** by the device (`0x03`); pings
   interleaved for keepalive.
4. Commit packet (`04 07 01 …` + CRC/size trailer).
5. `endAPIMode` (`0x02`) → **device reboots** (short `F0 00 21 10 78 3F F7` presence polls
   bridge the reboot window).
6. `beginAPIMode` again on the new session, `requestTopology`, `requestFactorySync`
   (`10 02`).
7. Device reports `topology devices=1`, `deviceVersion idx=46 '1.4.3'`, and streams its
   `config updateConfig` items back (factory sync). Reflash verified.

## Why this is the clean result

This is **exactly `HostPacketBuilder::addFirmwareUpdatePacket()`** in the vendored
`third_party/roli_blocks_basics` SDK (`MessageFromHost::firmwareUpdatePacket 0x04`), with
`MessageFromDevice::firmwareUpdateACK 0x03` as the per-packet reply. So an account-free
recovery flasher needs **no hand-rolled bit-packing** — the SDK already builds these packets.
End-to-end recovery path, now fully specified:

> fetch `.syx` account-free (§12) → parse its `0x47` messages → `addFirmwareUpdatePacket`
> per message → send, waiting on each `0x03` ACK → commit → device reboots.

The `0x77` packedProtocol itself is transport-neutral (config/topology traffic runs over
both USB-MIDI and BLE), but **firmware is accepted only over BLE**: a Block sent the exact
same `firmwareUpdatePacket` bytes over USB-MIDI **ignores them** — it never replies with a
`firmwareUpdateACK` and never enters update mode (no change to the device, so this is
harmless to try). So Dashboard's "Firmware updates require BT LE MIDI" is a **real
device-side requirement, not just an app policy** — a recovery flasher must use BLE.

## BLOCKS vs RISE — two different flash paths

- **BLOCKS** (this capture): flashed **in-band** as `0x77` `firmwareUpdatePacket`, no
  bootloader gesture. A Seaboard Block has **no USB-enumerating bootloader** (verified: the
  octave/white-button power combos never enumerate a DFU device — they are power/reset
  gestures). The `.syx` is parsed by the host and re-emitted packet-by-packet.
- **RISE** (per ROLI's archived support articles, not yet captured here): the raw `.syx`
  (`0x47`, plus a `0x48` begin/commit pair) is streamed with a generic SysEx tool after
  **physical bootloader entry** — hold both octave-shift buttons while powering on over USB.
  That manual gesture is the safety gate.

So §12's earlier remark that "the on-wire flash is a dedicated `0x47` protocol" holds for
**RISE**, but **not** for a Block flashed by Dashboard over BLE, which is `0x77`/`0x04`.

## Safety / scope

This research runs on the maintainer's own Blocks, with one kept untouched as a control
reference, and the brick risk accepted. The raw `.pcapng` capture, the copyrighted ROLI
`.syx` images, and the `X-API-Key` credential all stay **out of git** (gitignored under
`captures/firmware/`); only this decoded write-up is tracked. The show-exact-bytes-and-wait
rule applies before any write to a device, and the publicly released GRAB does not flash
firmware — shipping a flasher would be a separate, explicit decision.
