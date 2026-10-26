# Capturing ROLI Dashboard ↔ device traffic

GRAB can **read** a device by asking it for its settings, but to **write** a setting it
has to send the exact MIDI message ROLI Dashboard sends. **This guide works for any ROLI
device** — the steps are the same whatever you own. Right now it's most useful for:

- the **original Seaboard RISE (25/49, fw 1.x)** — GRAB shows it read-only; a capture
  confirms the `0x78 3D <param>` write numbers (known only on the RISE 2) and unlocks writing;
- the **Seaboard GRAND** — not mapped at all yet (if you have one, please run **Help add
  device…** in GRAB *first* so we can see its protocol, then do this capture for writes).

To fill the gap we need a capture of **Dashboard talking to the device** while a few
settings are changed. GRAB can't record this itself (Dashboard owns the port), so it's a
quick, passive capture — about 10 minutes. **This only observes traffic — it sends nothing
to the device and never touches firmware.**

Pick the method for your computer:

- **macOS → [Method C](#method-c--macos-midi-monitor-spy)** (easiest — a free MIDI monitor app, no admin).
- **Windows → [Method A](#method-a--wireshark-point-and-click-no-repo-needed)** (point-and-click Wireshark) or **[Method B](#method-b--repo-script-for-developers)** (a script, for developers).

All three produce the same thing: the messages Dashboard sends. Send us the result and we
do the decoding.

## What to change (both methods)

While the capture is running, open Dashboard, let it connect, and change these one at a
time, pressing **Save/Edit** after each. The odd values are easy to spot in the decode:

| Setting | Set it to |
|---|---|
| Pitch Bend Range | **12** |
| Octave | **+2** |
| Strike Sensitivity | **77** |
| Slide CC | **101** |

(More settings are welcome — the more you change, the more we can confirm in one go.)

---

## Method A — Wireshark (point-and-click, no repo needed)

Best if you're just running the released GRAB and want to help.

1. **Install Wireshark** (free): https://www.wireshark.org/download.html — during install,
   **keep "Install USBPcap" ticked** (that component captures USB).
2. **Close ROLI Dashboard and GRAB** completely.
3. Open **Wireshark** and double-click **USBPcap1** in the interface list to start
   recording. (If step 5 shows nothing, redo with **USBPcap2** — your device is on one of
   the two.)
4. **Open ROLI Dashboard**, let it connect, and make the changes in the table above.
5. **Stop** the capture (red square), then **File → Save As** → save a `.pcapng`.
6. **Send us the file** — attach it to [issue #1](../../issues/1) (or zip and share). It
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
   your **Seaboard RISE** (that's the destination Dashboard sends to). Leave MIDI Monitor
   running.
4. **Open ROLI Dashboard**, let it connect, and make the changes in the table above
   (Save/Edit after each). You'll see the messages appear in MIDI Monitor as you go.
5. **File → Save** to export the log (it keeps the SysEx).
6. **Send us the file** — attach it to [issue #1](../../issues/1).

If the RISE connects over **Bluetooth**, connect it first in **Audio MIDI Setup → MIDI
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
