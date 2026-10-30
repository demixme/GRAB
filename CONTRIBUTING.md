# Contributing to GRAB

GRAB only knows what people with real ROLI hardware send in. **You don't need to write
code** — the most valuable things are a one-click **profile** or a short **capture**, done
from inside GRAB. Thanks for helping keep these instruments alive.

---

## Start here: what does *your* device need?

Find your device below. The **"Most useful thing to send"** column is the one action that
helps most — each is explained under [The two things you can send](#the-two-things-you-can-send).

| Your device | Status in GRAB | Most useful thing to send |
|---|---|---|
| **Seaboard Block / Block M** | ✅ Full read + live edit | **Contribute a profile** (1 click) — confirms names & ranges |
| **Seaboard RISE 2** | ✅ Full | **Contribute a profile** |
| **Seaboard RISE 25 / 49** (original) | ✅ Full — *one gap* | **Contribute a profile** + a **Dashboard capture** of you setting a Glide/Slide/Press **Tracking** to **"Disabled"** (so we can add that option) |
| **Seaboard GRAND** | ✅ Full (octave & tracking are write-only) | **Contribute a profile** |
| **ROLI Piano 49** | ✅ Full | **Contribute a profile** |
| **Piano M / LUMI Keys** | ✅ Full | **Contribute a profile** |
| **Lightpad Block / Pad** | 🟡 **Work in progress** — its settings read/write, but its *apps/modes* (Note Grid, Fader Block, Drum Block, …) aren't managed | A **Dashboard capture** of you changing those **app/mode settings** |
| **Live / Loop / Touch** control blocks | ◻️ Connects, basic shared settings only | **Contribute a profile** *and* a **Dashboard capture** |
| **Airwave** | — Out of scope | Nothing — it's a host-side gesture controller, no on-device settings |
| **GRAB doesn't see it at all** | — | **Advanced ▸ Help add device (auto-explore) ▸ Run & submit** (one read-only click) |

> **Rule of thumb:** if GRAB already shows your settings, **send a profile** (way 1). To show
> what a device *does*, use **Capture MIDI** (way 2, built into GRAB). To add *writing* for a
> missing setting/app/device, we need a **Dashboard capture** (way 3) — the only way to learn
> what Dashboard *sends*. It's how the Seaboard GRAND went from "connects but shows nothing" to
> fully configurable.

---

## Three ways to help (easiest first)

### 1. Send a profile — one click, any supported device
Connect your device, click **Contribute…**, review the (anonymised) data, and **Submit**.

It sends the list of config items your device reports — their ids, ranges, options and your
current values — so GRAB can **label controls correctly for everyone** (and spot settings it
doesn't have a name for yet). Your serial is **hashed, never sent raw**; nothing you play is
included. ~30 seconds. If the network submit fails (firewall, proxy, offline), GRAB **saves a
file** and tells you where — attach that to a GitHub issue instead.

### 2. Capture MIDI — built into GRAB, no extra software
**Advanced ▸ Capture MIDI.** The easiest *technical* contribution: it records everything your
device **sends** and **decodes it to plain English** (Time · Kind · Message · Value · Raw), then
lets you **Submit**, Save, or open a GitHub issue — all in one place, read-only.

Use it to show what a **not-yet-supported** device actually does: **play it, touch it, press its
buttons, switch its modes**, and capture the result. That's often exactly what's needed to start
adding a device (it's how we confirmed, for example, that the GRAND is 4D with no Slide).

This captures the **device side** (what the device emits). It **cannot** see what Dashboard
sends *to* the device — that's way 3. For a device GRAB doesn't recognise *at all*, **Advanced ▸
Help add device (auto-explore)** does an automatic, one-click version of this.

### 3. Dashboard capture — ~10 minutes, a little technical, unlocks the most
GRAB can *read* a device, but to **write** a setting it must send the exact MIDI message ROLI
Dashboard sends. GRAB can't see that itself (Dashboard owns the port), so we need a short,
**passive** recording of **Dashboard talking to the device** while you change a few settings.

This is the one contribution that **adds *writing*** — a missing setting, a Lightpad app, a whole
new device. **It only observes traffic: it sends nothing to the device and never touches
firmware.**

Full step-by-step (Windows / macOS, pick your OS):
**[tools/CAPTURE-DASHBOARD.md](tools/CAPTURE-DASHBOARD.md)**. In short:

- **Windows:** [Wireshark + USBPcap](https://www.wireshark.org/download.html) (both free) —
  record the device's USB while you edit settings in Dashboard.
- **macOS:** [MIDI Monitor](https://www.snoize.com/MIDIMonitor/) (Snoize, free) with **"Spy on
  output"** enabled — record the "to device" stream.

Then **attach the saved file to a GitHub issue** (zip it if it's large). You don't decode
anything — send the raw file and we do the rest.

---

## What the in-app tools do (and when to reach for each)

Everything below is in GRAB. Nothing is sent until you click, and each dialog shows the exact
data first and offers **Save to file** / **Open a GitHub issue** instead of submitting.

| In GRAB | What it does | Use it when |
|---|---|---|
| **Contribute…** | Sends the anonymised **profile** (settings your device reports) | Your device is supported and you want to confirm/learn names & ranges |
| **Advanced ▸ Capture MIDI** | Records **what your device emits** and **decodes it to plain English** (Time · Kind · Message · Value · Raw) | Confirming the **device side** — e.g. what a control reports. **It does *not* see Dashboard's writes** (those need a USB/MIDI monitor, above) |
| **Advanced ▸ Help add device (auto-explore)** | One read-only "who are you?" + a few seconds of what the device emits, submitted in one go — **never changes a setting** | GRAB **doesn't recognise** your device, or connects but can't configure it |
| **Advanced ▸ Active probe** | Sends **one curated, documented** message and logs the reply (firmware/reset/program messages are hard-blocked; you must Arm each send) | Only if a maintainer asks — confirming an unsupported device's control protocol |
| **Diagnose…** | Lists the MIDI ports GRAB sees and flags a port **held by another app** | GRAB won't connect / "no device found" |

**The key distinction:** GRAB's **Capture MIDI** = the **device side** (what the device sends
out). A **Dashboard capture** (external USB/MIDI monitor) = the **write side** (what Dashboard
sends in). New *writing* support always needs the write side.

---

## Tips that make captures work

- **Sharing the device.** On **Windows 11 25H2+**, MIDI is multi-client by default, so
  Dashboard + a monitor (+ GRAB) can all use the device at once. On **older Windows**, only one
  app can hold the port — for a Dashboard capture, run **Dashboard + the monitor**, not GRAB.
  **macOS/Linux** already share.
- **Change the odd values** the capture guide lists (e.g. Pitch Bend Range = 12) — they're easy
  to spot in the decode. The more settings you change in one session, the more we confirm at once.
- **State your exact model and firmware version** in the issue — firmware changes behaviour, and
  some published values are old or single-model.

---

## Privacy & safety (what GRAB never does)

- **Serials are hashed** (SHA-256, truncated) — a device is deduped, never identified. Serial &
  MAC are **stripped** from captures, probes and logs.
- **No audio, no played notes** are sent — only what a device itself emits in an explicit,
  read-only capture you preview first.
- **No account, no raw IP.** See [DEVICE-PROFILES.md](DEVICE-PROFILES.md) for exactly what each
  payload contains.
- GRAB **never** sends firmware-update, program-upload (Littlefoot), factory-reset, or
  bootloader messages. Captures are observe-only. See the hard rules below.

---

## Contributing code

Build per the [README](README.md). Keep changes small and focused. For protocol work:
- Mark observations as **confirmed-on-device** vs **from-source** (as `PROTOCOL.md` does).
- When the vendored library and a reference disagree, **flag it, don't guess**.
- Keep decoder logic in `libs/sea_sysex` (portable, no JUCE) so it also parses captures.
- Any change to `third_party/roli_blocks_basics/` must be minimal and recorded in `PATCHES.md`.

### Hard safety rules (non-negotiable)
GRAB must **never** send firmware-update, program-upload (Littlefoot), factory-reset, or
bootloader messages — a bad write can brick a device. Config writes use `setConfig` only (on
RISE/GRAND, the `0x78 3D` set-parameter, NRPN and RPN messages Dashboard itself sends for
settings). Don't add a firmware flasher. If you're capturing, it's observe-only.

### Repo pointers
- `PROTOCOL.md` — the BLOCKS SysEx protocol · `RISE-GRAND.md` — RISE 2 / RISE 25 / GRAND.
- `PIANO.md` — ROLI Piano 49 · `LUMI.md` — Piano M / LUMI Keys.
- `DEVICE-PROFILES.md` — the self-learn schema + endpoint contract.
- `tools/CAPTURE-DASHBOARD.md` — the Dashboard-capture how-to.
- `PATCHES.md` — vendored-library patches.
- `libs/sea_sysex` — portable decoder · `src/seaprobe`, `src/seadecode`, `src/gui` — the tools.

By contributing you agree your contributions are licensed under the project's MIT license.
