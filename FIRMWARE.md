# ROLI firmware — images and on-wire flash

How ROLI device firmware is packaged and flashed, reverse-engineered from the `.syx`
images themselves and from a live BLE capture of ROLI Dashboard flashing a Seaboard Block.
Companion to [PROTOCOL.md §12](PROTOCOL.md#12-firmware-update-mechanism-dev) (the
firmware *fetch* chain and the `.syx` opcode map) and
[tools/FIRMWARE-CAPTURE.md](tools/FIRMWARE-CAPTURE.md) (archiving the images over HTTPS).
Everything here is **reverse-engineering / preservation reference** — the shipping GRAB
does not flash firmware.

## Why this matters

ROLI is in administration. Owners of second-hand or new-old-stock hardware are stranded:
downloading ROLI Dashboard requires *registering* a device, which is blocked when the
device is locked to a previous owner's account or is brand-new and unregisterable — with no
ROLI Support left to unlock it. The firmware *fetch* needs no account (a static
`X-API-Key`, see §12), so the missing piece for an account-free recovery tool was the
on-wire flash protocol. This documents what we have decoded toward that.

## Image catalogue (what we have archived)

Fetched account-free while ROLI's servers are up (gitignored under `captures/firmware/`;
ROLI's copyrighted files, never committed). Four **distinct** images:

Each image is fetched account-free while ROLI's servers are up:
`GET my.roli.com/firmware_versions/update_to/<product>/<any-version>` with a static
`X-API-Key` (no login — see [PROTOCOL.md §12](PROTOCOL.md#12-firmware-update-mechanism-dev))
303-redirects to a signed `builds.roli.com/hardware/<product>/<ver>/<file>`. Seven distinct
images:

| Product name (on the server) | Image id (`I:`) | Version | File | Devices | Family |
|---|---|---|---|---|---|
| `Seaboard Block` | `seaboard` | 1.4.3 | `BLOCKS seaboard 1.4.3 RC.syx` | Seaboard Block | BLOCKS |
| `Live` / `Loop` / `Touch` / `Control Block` | `control` | 0.1.2 | `BLOCKS control 0.1.2.syx` | Live / Loop / Touch (one shared image) | BLOCKS |
| `LUMI Keyboard` | `lightkey` | 1.3.9 | `BLOCKS lightkey 1.3.9.syx` | LUMI Keyboard / Piano M | BLOCKS |
| `Lightpad M` | `padmini` | 0.1.2 | `BLOCKS padmini 0.1.2.syx` | Lightpad (Block) M | BLOCKS |
| `Seaboard RISE` | — | 1.1.8 | `MAIN_1.1.8.syx` | Seaboard RISE (49) | RISE |
| `Seaboard RISE 25` | — | 1.0.9 | `MAIN_1.0.9.syx` | Seaboard RISE 25 | RISE |
| `Seaboard RISE2` | — | 2.0.2 | `RISE 2.0.2 RC6.syx` | Seaboard RISE 2 | RISE |

**No downloadable build** (product registered, but the server returns 204): `Seaboard GRAND`,
`ROLI Piano 49`.

**The product names come from ROLI Dashboard's own strings, and they are not obvious:** it is
`Lightpad M` (not "Lightpad Block M"), `LUMI Keyboard` (not "LUMI Keys" or "Piano M"), and
`Seaboard RISE2` (no space). Confirmed by hash, the Seaboard Block image ships under several
product names but is one file, and **Live/Loop/Touch/Control are byte-identical** — one
shared `control` image. Fetch coordinates / opaque product+version IDs: PROTOCOL.md §12.

> **Note on a different LUMI:** there is also a Linux-based LUMI product (the standalone
> *LUMI Keys* / Studio Edition) whose firmware is an **encrypted `.bin`** (not a `.syx`),
> delivered by the LUMI / ROLI Learn app, not this endpoint. Its container format is
> documented by the community `Wh1terat` "Lumi FW Unpacker" gist (a `LumiHdr` header → an
> obfuscated tar → RC4-variant-encrypted components such as `linux.bin`). We have not
> obtained that image; it is out of scope for the SysEx flash below.

> **The firmware images themselves are ROLI's copyrighted property and are NOT committed to
> this repository.** This document publishes only the reverse-engineered *details* (formats,
> names, structures). Archiving is done locally, outside version control.

## Two firmware families

All ROLI `.syx` firmware is a concatenation of `F0 00 21 10 <ns> … F7` messages, and the
payloads are high-entropy (~6.4 bits/byte against the 7-bit ceiling) — a compressed or
encrypted image, not readable code. But the two families are structured very differently:

| | **BLOCKS** (seaboard, control, lightkey, padmini) | **RISE** (49, 25, RISE 2) |
|---|---|---|
| Namespace(s) | `0x47` only | `0x47` data + `0x48` control |
| Per-message structure | `op + address`, 80-byte 7-bit payload | flat **128-byte** block, no op/address |
| Self-describing manifest | **yes** — keyed ASCII (`V:`/`I:`/…) | **no** — version only in the filename |
| Image regions | opcodes `04/05/07` segments + `00`/`01` bracket | two regions split by a `0x48` marker |
| How it reaches the device | transcoded to `0x77` `firmwareUpdatePacket`, **in-band over BLE**, every packet ACKed | the `.syx` **streamed verbatim over USB** after physical bootloader entry |
| Bootloader gate | none (in-band) | **hold both octave buttons at power-on** |

### BLOCKS image anatomy

`F0 00 21 10 47 <op> <payload…> F7`, 86 bytes for a data message (80-byte payload):

- `op 0x00` — **manifest / START** (1 message). Keyed ASCII, `\r\n`-separated:
  `ROLI BLOCKS Update`, `V:` version, `I:` target image (`seaboard`, `control`, `lightkey`,
  `padmini`), then
  binary-valued `A`/`a` (load address), `M`/`m` (hash/CRC), `S`/`s` (signature slot, empty
  in these builds), `B`/`b` (build). A `.syx` thus identifies its own version + target from
  its bytes — a match-before-flash check that doesn't trust the filename.
- `op 0x04 / 0x05 / 0x07` — **data** (address-stepped). seaboard uses all three
  (942/1838/100); control uses `04`/`07` only (910/67). The different opcodes mark the image
  segments (cf. Dashboard's `ROLI_SYSEX_UPDATE_FIRMWARE_*` vs `…_BINARY_*`).
- `op 0x01` — **commit / END** (1 message, 13 B seaboard / 12 B control): CRC + size trailer.

Counts (messages): seaboard 2882, control 979, lightkey 2356, padmini 1723 — all the same
BLOCKS shape, differing only in size and image id.

### RISE image anatomy

`F0 00 21 10 47 <128 bytes> F7` — the byte after the namespace is **not** an opcode, it is
raw payload (it takes the full `0x00`–`0x7F` range). The blocks are a flat, sequentially
written stream with **no per-message address**, so ordering is everything and there is no
random access — a single dropped or corrupt block ruins the image (which is why RISE gates
the flash behind a physical bootloader gesture and a reliable USB link). Skeleton:

```
F0 00 21 10 47 F7            × 3   empty "sync / flush"
F0 00 21 10 47 <128 B> F7    …     region 1 data blocks  (+ one short tail block)
F0 00 21 10 48 F7                  ← 0x48 section boundary (region 1 → 2)
F0 00 21 10 47 <128 B> F7    …     region 2 data blocks  (+ one short tail block)
F0 00 21 10 48 7F F7               ← 0x48 commit / finalise
```

So the **`0x48` namespace carries the control markers**: an empty `48 F7` splits the image
into two regions (as BLOCKS splits via opcodes), and `48 7F F7` is the final commit. There
is no keyed manifest — the only version marker is the filename. Counts:
RISE 49 (1.1.8) = 619 messages (~78 KB payload, boundary at msg 492); RISE 25 (1.0.9) = 509
(~64 KB, boundary at msg 387); **RISE 2 (2.0.2) = 727 messages** (~90 KB).

**RISE 2 firmware is RISE-family, not something new.** Despite the RISE 2 using the `0x78`
dialect for *config/topology*, its firmware image has the identical RISE shape above (flat
`0x47` 128-byte blocks, two `0x48` markers, three empty syncs). So a stuck RISE 2 is recovered
the same way as any RISE — stream the `.syx` after physical bootloader entry. This is the
recovery image for [issue #42](https://github.com/demixme/GRAB/issues/42) (a RISE 2 stuck in
bootloader); a real flash capture is still wanted to confirm the per-message ACK/timing before
guiding anyone through it.

## BLOCKS on-wire flash over BLE (decoded from a capture)

Captured ROLI Dashboard reflashing a Seaboard Block over Bluetooth LE (Wireshark 5.0.0rc2
`etwdump` "Bluetooth Host Radio" HCI capture; decoded with `seadecode --blemidi`). 15,200
ATT writes reassembled to 3,674 logical SysEx messages; the device confirmed
`deviceVersion '1.4.3'` after rebooting.

**Dashboard does NOT send the raw `0x47` `.syx` over the wire.** It transcodes each `.syx`
message **1:1** into a `firmwareUpdatePacket` — host message type `0x04` carried in the
`0x77` packedProtocol (`F0 00 21 10 77 <idx> 04 <sub> … F7`). The 2882 `.syx` messages map
to exactly 2882 packets:

| `.syx` message (`0x47`) | on-wire `firmwareUpdatePacket` (`0x77`/`0x04`) | count |
|---|---|---|
| `47 00` manifest / START | `04 69 00 "ROLI BLOCKS Update\r\nV:1.4.3\r\nI:seaboard…"` | 1 |
| `47 04/05/07` DATA (addr-stepped) | `04 50 04 <addr…> <payload…>` — addr steps `0x20` | 2880 |
| `47 01` commit / END | `04 07 01 …` (CRC/size trailer) | 1 |

**Flow control:** the device replies `firmwareUpdateACK` (device message type `0x03`) —
2882 of them, one per packet (strict 1:1 handshake). Dashboard interleaves `ping`
keepalives. Full sequence: `beginAPIMode` → `requestTopology` → manifest → 2880 data (each
ACKed) → commit → `endAPIMode` → **device reboots** (`F0 00 21 10 78 3F F7` presence polls)
→ `beginAPIMode` → `requestTopology` → `requestFactorySync` → device reports
`deviceVersion '1.4.3'`.

This is exactly `HostPacketBuilder::addFirmwareUpdatePacket()` in the vendored
`third_party/roli_blocks_basics` SDK (`MessageFromHost::firmwareUpdatePacket 0x04`), with
`MessageFromDevice::firmwareUpdateACK 0x03` as the per-packet reply — so an account-free
flasher needs **no hand-rolled bit-packing**. End-to-end recovery path:

> fetch `.syx` account-free (§12) → parse its `0x47` messages → `addFirmwareUpdatePacket`
> per message → send, waiting on each `0x03` ACK → commit → device reboots.

Firmware is accepted **only over BLE**: a Block sent the identical `firmwareUpdatePacket`
bytes over USB-MIDI **ignores them** — it never replies with a `firmwareUpdateACK` and never
enters update mode (harmless to attempt, no change to the device). So Dashboard's "Firmware
updates require BT LE MIDI" is a **real device-side requirement, not just an app policy** —
a BLOCKS recovery flasher must use BLE.

### Capture method (BLE HCI, below the app)

The flash is invisible at the app layer (USBPcap can't see USB-MIDI class traffic; macOS
MIDI Monitor's CoreMIDI spy catches only the heartbeat — Dashboard's firmware path is a
private BLE route). Capture below the app, at the Bluetooth HCI layer:

1. **Wireshark ≥ 4.7.3** (we used 5.0.0rc2) — older builds lack the Bluetooth ETW provider.
2. Run Wireshark **as Administrator** → interface **`etwdump`** → provider **"Bluetooth
   Host Radio (Microsoft-Windows-BTH-BTHPORT)"**.
3. Start the capture, then in Dashboard do **Manually update firmware** and point it at a
   `.syx` (a same-version reflash captures the full sequence at lowest risk). Let it finish
   and the device reboot; stop; save as `.pcapng`.
4. Decode with `tools/decode-capture.sh <cap>.pcapng` (auto-detects BLE, runs
   `seadecode --blemidi` on the ATT writes `0x52` and notifications `0x1b`).

On the wire: host→device is ATT Write Command (`0x52`) on handle `0x001b` (the BLE-MIDI
characteristic); device→host is Handle Value Notification (`0x1b`). Long SysEx is split
across BLE-MIDI fragments; `seadecode --blemidi` reassembles them.

## RISE flash over USB (confirmed from a capture)

The RISE `.syx` is streamed **verbatim over USB** after the RISE is put into **bootloader
mode physically** — connect USB and **hold both octave-shift buttons while switching it on**.
In bootloader mode the RISE enumerates with a distinct USB identity (**PID_0200**, vs
**PID_0210** for the normal running app); the bootloader does *not* answer the app-protocol
identify (`F0 00 21 10 78 3F F7`), so silence there is expected in this state.

The protocol was confirmed by a RISE 2 (2.0.2) update capture + a disassembly of ROLI
Dashboard 4.7.2's RISE updater, contributed on
[issue #42](https://github.com/demixme/GRAB/issues/42):

- **No commit/CRC/activate bracket.** Dashboard sends one sync `F0 00 21 10 47 F7`, then
  streams the `.syx` messages with ~100 ms sleeps between them. The raw `.syx` *is* the whole
  flash — there is no separate commit handshake.
- **Ack-paced.** The device replies `F0 00 21 10 79 F7` to **every `0x47`** message (data
  block and sync). The two **`0x48` end-of-image messages get no reply.** Progress is reported
  as `F0 00 21 10 50 <page> 78 F7` (the `0x78` RISE dialect), counting pages committed —
  up to `0x63` (99 pages) for 2.0.2.
- **Two images, bracketed by the two `0x48` markers:** `aid:1` = MAIN (RISE STM32 main app),
  `aid:2` = SUB (RISE SUB, an STM32F0 co-processor). A long **~93 s stall** sits at the
  MAIN-end → SUB-accept boundary; the last progress markers arrive ~5 s after the final
  `48 7F`; ~17 s after that the main MCU restarts and the bootloader jumps to the app once.

Our archived `RISE 2.0.2 RC6.syx` (727 messages) matches the contributed send byte-for-byte,
and an independent decode of the log corroborates it exactly: **725 `0x79` acks for 727 sends
→ the only two unacked messages are the `0x48` ends**, 99 `50 <page> 78` progress markers, and
the 93 s stall at the MAIN→SUB boundary (page `0x30`→`0x31`), total ~136 s.

**Still open (the key diagnostic):** whether a *healthy* RISE 2 also stalls ~93 s after the
MAIN end — i.e. whether that stall is normal or a symptom. In issue #42 the device flashes and
the app *does* launch, then dies in ~1 ms from an STM32 power-supervisor (PVD) trip at boot —
pointing to a **hardware power-subsystem fault, not a firmware/flash problem**. A MIDI capture
of one Dashboard update on a *working* RISE 2 (with notes on the LED/power-button behaviour
during and after) would settle it.

## ROLI's own recovery method + firmware history (support forum)

ROLI's (now-defunct) support forum at `support.roli.com` is preserved in the Internet Archive
and is a useful historical record. Two things are worth keeping here.

**ROLI's documented manual firmware recovery for the RISE** (verbatim method, posted by ROLI
support staff — it matches our decoded protocol exactly and needs no ROLI account):

- *macOS:* install **SysEx Librarian** (Snoize). Connect the RISE by USB and — **while holding
  both octave-shift buttons** — switch it on, then release (this is the physical bootloader
  entry). Drag the firmware `.syx` into SysEx Librarian, set **Destination = "Seaboard RISE"**,
  select the file, press **Play**. The RISE restarts automatically when done (~30 s) — do not
  touch it until it has finished.
- *Windows:* install **MIDI-OX**. Enter bootloader the same way (hold both octave buttons at
  power-on). In MIDI-OX: **Options → MIDI Devices**, select the Seaboard RISE under *MIDI
  Outputs*; then **Actions → Send → SysEx File** and choose the `.syx`. Takes 20–30 s; don't
  touch the RISE until complete, then restart it.

This confirms original-RISE firmware flows over **USB-MIDI** (not BLE), and that the raw `.syx`
is streamed with no app involvement — the same conclusion as the issue-#42 capture above.
ROLI staff also confirmed on the forum that the **RISE 25 and RISE 49 share one firmware**.

**Firmware version history (from the forum's Announcements, ~2018–2019 BLOCKS/RISE era):**

| Date | ROLI Dashboard | BLOCKS firmware | RISE firmware | Highlights |
|---|---|---|---|---|
| ~Aug 2018 | 4.1.0 | 0.4.3 | 1.1.7 | Approved **MPE spec** (Lower/Upper Zone); BT sticking-notes fix |
| Nov 2018 | — | 0.4.4 | — | Fix "save Littlefoot to flash" failure + mistriggering (all three Blocks) |
| ~May 2019 | 4.1.6 | — | — | Fixes a 4.1.4 launch crash |
| ~Sep 2019 | — | 0.4.5 | — | **Auto MIDI-channel reassignment over DNA** (avoids MPE channel clash); DNA/BT stability; Control-Block LED colour correction. ⚠ inter-block comms changed → all Blocks must be updated together. Introduced a cosmetic Lightpad grid flicker. |

Notes:
- The forum's firmware attachments (e.g. `BLOCKS seaboard 0.4.4.syx` 215 KB, `BLOCKS lightpad
  0.4.4 (1).syx` 245 KB, `BLOCKS control 0.4.4.syx` 99.5 KB, `RISE 1.1.7.syx` 79.6 KB) are
  **not recoverable from the Archive** — Freshdesk served attachments via an expiring redirect,
  so every archived `/helpdesk/attachments/*` capture is a 302 stub, not the file. The filenames
  do confirm the `seaboard`/`lightpad`/`control` image-id naming (the modern `padmini` is a
  later rename of `lightpad`).
- The **original Seaboard GRAND does not appear on this forum at all.** Its final firmware was
  **v1.1.0** (per ROLI support staff), it predates this forum, and it shipped in the 2014 Mac
  GRAND/Equator app rather than the Blocks/RISE update server — which is why modern
  `my.roli.com` returns `204 no-build` for it.

## Safety / scope

This research runs on the maintainer's own Blocks, with one kept untouched as a control
reference and the brick risk accepted. Raw captures, the copyrighted ROLI `.syx` images, and
the `X-API-Key` credential all stay **out of git** (gitignored under `captures/firmware/`);
only this decoded write-up is tracked. The show-exact-bytes-and-wait rule applies before any
write to a device.

A console dev tool — **`seaprobe flash <file.syx> [--commit] [--transport usb|ble]
[--serial <S>]`** — implements the BLOCKS recovery path above: it parses the `.syx`, drives
`Block::sendFirmwareUpdatePacket` per message, and waits on each `0x03` ACK. It is **dry-run
by default** (prints the exact packets, transmits nothing), **serial-gated** (refuses to
guess which block when more than one is connected), and does a **match-before-flash** check
(manifest `I:` vs device type). It has been validated by reflashing the maintainer's own
Seaboard Block 1.4.3 over BLE — byte-identical to Dashboard, all 2882 packets ACKed, device
rebooted cleanly. The **GRAB GUI does not flash firmware yet**; folding this account-free
recovery path into GRAB (for orphaned/NOS devices) is the planned next step.
