# Capturing ROLI Dashboard ↔ device traffic

GRAB can **read** a device by asking it for its settings, but to **write** a setting it
has to send the exact MIDI message ROLI Dashboard sends. GRAB can't record that itself
(Dashboard owns the port), so we need a short, **passive** recording of **Dashboard talking to
the device** while you change a few settings — about 10 minutes. **This only observes traffic:
it sends nothing to the device and never touches firmware.**

**This guide works for any ROLI device** — the steps are the same whatever you own. Right now a
capture helps most for:

- **Lightpad Block / Pad** — its basic settings work, but its **apps/modes** (Note Grid, Fader
  Block, Drum Block, …) don't. Capture Dashboard while you change those app/mode settings.
- **Live / Loop / Touch** control blocks — only basic settings are read today; a capture is what
  adds the rest.

(The Seaboard **Block / RISE 2 / RISE 25 / 49 / GRAND**, **ROLI Piano** and **Piano M / LUMI** are
already fully mapped — for those, a one-click **Contribute…** profile is all that's needed.)

Pick the method for your computer:

- **macOS → [Method C](#method-c--macos-midi-monitor-spy)** (easiest — a free MIDI monitor app, no admin).
- **Windows → [Method A](#method-a--wireshark-point-and-click-no-repo-needed)** (point-and-click Wireshark) or **[Method B](#method-b--repo-script-for-developers)** (a script, for developers).

All three produce the same thing: the messages Dashboard sends. Send us the result and we
do the decoding.

> ⚠️ **Connect the device by USB cable, not Bluetooth.** We need what Dashboard sends *to the
> device* (the setting/app **writes**). Over USB those are captured directly. Over **Bluetooth**,
> the capture tools only see the device's *output* — touch/sensor notifications, battery, etc. —
> **not** Dashboard's writes, so a Bluetooth capture comes back with nothing to decode. Plug the
> USB cable in first, and let Dashboard connect over USB.
>
> ⚠️ **Plugging in USB isn't enough — turn Bluetooth off too.** BLOCKS (Lightpad, Seaboard Block…)
> keep using their Bluetooth link even once the USB cable is in, so Dashboard talks over BT and the
> USB capture stays empty. **Turn off your computer's Bluetooth** (or "Forget/Unpair" the device),
> then confirm in Dashboard that it's connected **over USB** before you capture.

## What to change (while the capture runs)

Open Dashboard, let it connect, and change settings **one at a time**, pressing **Save/Edit**
after each. Deliberately **odd values** are easy to spot in the decode.

**Any device — start with these:**

| Setting | Set it to |
|---|---|
| Pitch Bend Range | **12** |
| Octave | **+2** |
| Strike Sensitivity | **77** |
| Slide CC | **101** |

**Then, for your specific device:**

- **Lightpad / Pad** — this is the valuable part. In Dashboard, open an **app** (Note Grid,
  Fader Block, Drum Block), change its **mode/layout settings**, and **switch between apps/modes**.
  Each app's settings are what we're missing — change as many as you can.
- **Live / Loop / Touch** — change every setting Dashboard exposes for the block.

The more distinct settings you change in one session, the more we confirm at once.

---

## Method A — Wireshark (point-and-click, no repo needed)

Best if you're just running the released GRAB and want to help.

1. **Install Wireshark** (free): https://www.wireshark.org/download.html — during install,
   **keep "Install USBPcap" ticked** (that component captures USB).
2. **Connect your ROLI device with a USB cable** (not Bluetooth) — USBPcap captures USB, and only
   a USB connection carries Dashboard's writes to the device.
3. **Close ROLI Dashboard and GRAB** completely.
4. Open **Wireshark** and double-click **USBPcap1** in the interface list to start
   recording. (If step 6 shows no ROLI traffic, redo with **USBPcap2** — your device is on one
   of the two.)
5. **Open ROLI Dashboard**, let it connect **over USB**, and make the changes in the table above.
6. **Stop** the capture (red square), then **File → Save As** → save a `.pcapng`.
7. **Send us the file** — attach it to [issue #1](../../issues/1) (or zip and share). It
   contains only MIDI/USB traffic.

You don't need to decode it — send the raw `.pcapng` and we'll do the rest.

## Method B — repo script (for developers)

If you've cloned the repo, `tools/capture-dashboard.ps1` automates the same capture (it
records both USB hubs and handles the timing).

Requirements: **Wireshark + USBPcap installed** (for `tshark`), and an **Administrator**
PowerShell (USBPcap needs admin).

```powershell
# In an ELEVATED PowerShell, from the repo root:
powershell -ExecutionPolicy Bypass -File tools\capture-dashboard.ps1 -Seconds 60
```

Ordering matters (USBPcap only sees a MIDI pipe opened *after* it starts):
1. **Close ROLI Dashboard first.**
2. Run the script — it arms the capture, waits, then prints **GO**.
3. On **GO**: open Dashboard, let it connect, make the changes in the table above, then let
   the capture finish. (If Dashboard was already open, instead **unplug/replug** the device
   after GO and let it reconnect.)

It writes `captures\dashboard-<timestamp>-hub1.pcapng` and `…-hub2.pcapng`. Decode locally
with:

```bash
tools/decode-capture.sh      # auto-detects USB vs BLE, runs seadecode
```

…or just send the `.pcapng` files and we'll decode them.

## Method C — macOS (MIDI Monitor "spy")

macOS has a MIDI "spy" that shows what *other* apps send to a device — so you can watch
Dashboard talk to the RISE directly, no USB capture or admin needed.

1. **Install MIDI Monitor** (free, by Snoize): https://www.snoize.com/MIDIMonitor/
   (or `brew install --cask midi-monitor`).
2. **Open MIDI Monitor.** The first time you use spying it installs a small CoreMIDI
   component into `~/Library/Audio/MIDI Drivers` — approve it and **relaunch the app** if it
   asks. (macOS 10.13+; spying works on 11.2 and later.)
3. In the **Sources** list, tick the entry under **"Spy on output to destinations"** for
   **your device** (that's the destination Dashboard sends to). Leave MIDI Monitor running.
4. **Open ROLI Dashboard**, let it connect, and make the changes in the table above
   (Save/Edit after each). You'll see the messages appear in MIDI Monitor as you go.
5. **File → Save** to export the log (it keeps the SysEx).
6. **Send us the file** — attach it to [issue #1](../../issues/1).

If the device connects over **Bluetooth**, connect it first in **Audio MIDI Setup → MIDI
Studio → Bluetooth**, then spy on it the same way.

---

## Notes

- **Which method:** macOS → Method C (MIDI Monitor spy, easiest). Windows → Method A
  (Wireshark) or B (script). The Windows `USBPcap` route is Windows-only; the macOS MIDI
  spy is macOS-only — each OS has its own easy path above.
- **Privacy:** captures contain MIDI traffic and the device serial, nothing else. Share
  them the way you're comfortable with.
- **Safety:** capturing is passive. GRAB and these tools never send firmware-update,
  factory-reset, or program-upload messages — see the hard safety rules in
  [CONTRIBUTING.md](../CONTRIBUTING.md).
