# Contributing to GRAB

GRAB improves as people with different ROLI hardware report what their devices do.
You don't need to write code to help — a device profile or a capture is hugely
useful. Thanks for pitching in.

## Ways to contribute (easiest first)

### 1. Submit a device profile (one click)
If you have any ROLI BLOCKS device, run the GUI, connect it, and use
**Contribute device profile**. It builds an anonymized JSON of the config items
your device reports (serial is hashed, never sent raw — see
[DEVICE-PROFILES.md](DEVICE-PROFILES.md)), which you can submit to the project
store or save to a file to attach to an issue. This is how we learn config ids and
names across devices (especially unnamed/user items and new hardware).

### 2. Report a device that works / doesn't
Open an issue with: device model, firmware version, transport (USB/BLE), and what
happened. If config reads back but some items are unnamed or wrong, attach a
device-profile JSON.

### 3. Capture a protocol you want supported
For settings GRAB doesn't understand yet — or whole families like **RISE/GRAND** —
capture ROLI Dashboard talking to the device and share the decode:
- `tools/capture-dashboard.ps1` / `capture-usb-replug.ps1` (USBPcap, run elevated),
- `tools/decode-capture.sh` (auto-detects USB vs BLE and runs `seadecode`),
- or any MIDI monitor for the plain-SysEx RISE/GRAND.
See [RISE-GRAND.md](RISE-GRAND.md) for the known RISE/GRAND parameters and the
capture-and-map workflow.

### 4. Code
Build per the [README](README.md). Keep changes small and focused. For protocol
work:
- Mark observations as **confirmed-on-device** vs **from-source** (as `PROTOCOL.md` does).
- When the vendored library and a reference (blocksd) disagree, flag it, don't guess.
- Keep decoder logic in `libs/sea_sysex` (portable, no JUCE) so it also parses captures.
- Any change to `third_party/roli_blocks_basics/` must be minimal and recorded in
  `PATCHES.md`.

## Hard safety rules (non-negotiable)
GRAB must **never** send firmware-update, program-upload (Littlefoot), factory-reset,
or bootloader messages — a bad write can brick a device. Config writes use
`setConfig` only. Don't add a firmware flasher. If you're capturing, it's
observe-only.

## When reporting protocol values
Firmware changes behaviour, and some published values are old/single-model. Always
**state the exact device model and firmware version**, and note which values you
verified yourself versus took from a source. Put this in the PR/issue.

## Repo pointers
- `PROTOCOL.md` — BLOCKS protocol; `RISE-GRAND.md` — RISE/GRAND notes.
- `DEVICE-PROFILES.md` — the self-learn schema + endpoint contract.
- `PATCHES.md` — vendored-library patches.
- `libs/sea_sysex` — portable decoder; `src/seaprobe`, `src/seadecode`, `src/gui` — tools.

By contributing you agree your contributions are licensed under the project's MIT
license.
