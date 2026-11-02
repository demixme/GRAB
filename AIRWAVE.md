# ROLI Airwave — what it is, its exact MIDI, and how to outlive ROLI

**Scope:** GRAB does **not** configure an Airwave — its hand expression is produced in host software,
not on the device, so there are no on-device MIDI settings for GRAB to read or write. This document is
research and preservation: what Airwave actually is, the exact MIDI it emits, what to archive before
ROLI's servers go dark, and how the same camera can run with no ROLI software at all.

Everything here is decoded from ROLI's own Airwave manual and confirmed against the class-compliant USB
MIDI the device presents. The same material backs GRAB's in-app **Advanced ▸ Airwave** panel and the
**Airwave CC tester**; this file is the written reference. See also [PRESERVATION.md](PRESERVATION.md).

---

## What Airwave actually is

An overhead tower holding **two Ultraleap IR 170** infrared cameras plus ROLI's "ROLI Vision" software —
27 joints per hand at 90 fps, tracking range ~100–1000 mm. **The cameras are standard Ultraleap
devices** (the macOS driver is literally `UltraleapUVCDriver`), not bespoke ROLI hardware.

Over an internal USB2 hub it presents **three class-compliant devices**:

- **ROLI Airwave Expression** — the hand-gesture MIDI (the CCs below).
- **ROLI Airwave Pedal** — pedal MIDI, plus Airwave's own config + firmware via ROLI Dashboard.
- **Airwave Audio** — a class-compliant audio interface (headphone out).

The IR-camera-to-MIDI translation runs in **host software** (ROLI Airwave Control), **not on the
device**. That single fact is why Airwave is both out of scope for GRAB and highly rebuildable.

---

## The six Dimensions of Air (how it really works)

**Raise, Tilt, Glide, Slide, Flex, Grasp** — all computed from **wrist position and the plane of your
metacarpals**. It explicitly ignores individual fingers and only tracks hands above the keyboard.

- Each hand is computed separately; most dimensions use the **larger** of the two hands.
- **Grasp** is the exception — it **sums both** hands (grasp both to reach 100%).
- So it is per-**hand**, not per-note / per-finger like a Seaboard.
- Values reset to 0 when tracking is lost and no notes are held.

Notes come from your keyboard as usual; Airwave only adds the expression layer.

---

## The exact MIDI — "ROLI Airwave Expression"

Each hand on its own CC, 0–127 (unsigned, scaled). Notes arrive separately from your keyboard.

| CC | Gesture | Hand |
|----|---------|------|
| **21** | Grasp | both hands summed |
| **22** / **23** | Slide | Left / Right |
| **24** / **25** | Glide | Left / Right |
| **26** / **27** | Raise | Left / Right |
| **28** / **29** | Tilt  | Left / Right |
| **30** / **31** | Flex  | Left / Right |

> **Note:** CC28/CC29 are undefined in the MIDI spec, so some plug-ins hijack them for preset
> switching — worth knowing if Tilt seems to change patches.

This set is fixed and documented — the key to a ROLI-free rebuild and to GRAB's emulator.

---

## The ROLI Airwave Pedal

The pedal is a separate class-compliant device whose **config + firmware go over ROLI's own MIDI
protocol via ROLI Dashboard** (pedal type/model, pitch transpose). That's a small settings surface of
the kind GRAB reads/writes on other ROLI gear, so in principle GRAB could read/write the pedal config
too. **GRAB never touches firmware** (hard rule), and nobody has captured the pedal frames yet — if you
own an Airwave Pedal, a Dashboard capture ([tools/CAPTURE-DASHBOARD.md](tools/CAPTURE-DASHBOARD.md))
would let us map it.

---

## The GRAB angle

GRAB doesn't drive Airwave, but two things are useful today:

- **Capture MIDI** logs the Expression CCs directly — it's class-compliant USB MIDI — handy for study.
- **Airwave CC tester** (*Advanced ▸ Airwave ▸ CC tester*) emits the exact **CC21–31** to a MIDI output
  you pick, so you can MIDI-map and test Airwave gestures in a DAW or plug-in **without the hardware**.
  Route into a DAW via a virtual port (Windows: loopMIDI; macOS: the IAC Driver). It sends only standard
  CC messages — it reads no hardware and needs no SDK.

---

## A ROLI-free rebuild is feasible

The hard part is already open. The cameras are commodity Ultraleap, and **Ultraleap's SDK hands you
wrist position, hand orientation and grab strength directly** — you never touch the raw IR feed. The
mapping to CC21–31 is simple geometry, and the output is a documented, class-compliant CC set. "ROLI
Airwave Control" is essentially that mapper, and it's replaceable.

Ready-made stand-ins and parts:

- **[MidiPaw](https://midipaw.com/)** — free, Windows, Ultraleap → MIDI. Not open source, but the
  closest turnkey replacement.
- Open-source bridges: `Emchei/leapmotion-midi`, `ndiesslin/LeapMidiBridge`, `geekychris/leap_motion_harp`.
- The **Max/MSP "Ultraleap" package** supports the IR 170 directly.
- **[Ultraleap SDK](https://www.ultraleap.com/)** — the durable path; drives the camera with no ROLI
  software at all.

---

## Archive now — and the licence clock

To your own disk, **while ROLI's servers still respond**:

- **ROLI Airwave Control** (carries `UltraleapUVCDriver` + ROLI Vision).
- **Airwave Player** + its factory-library package.
- The **manuals**.
- The durable path: the **Ultraleap SDK** from ultraleap.com, which drives the camera with no ROLI
  software at all.

**Licence clock:** Airwave Player is server-leased like Equator 2 — it runs about **four weeks offline**
before it must phone home to ROLI again. The downloaded installers, content and the Ultraleap SDK you
keep regardless; the leased *app* authorises only ~28 days past your last successful login once the
servers go dark. (Full detail on the 28-day lease in [PRESERVATION.md](PRESERVATION.md).)

> Keep copies for yourself. **Don't publicly re-host** ROLI's installers, firmware or manuals.
