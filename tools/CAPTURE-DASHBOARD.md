# Capturing ROLI Dashboard ↔ device traffic

GRAB can **read** a device by asking it for its settings, but to **write** a setting it
has to send the exact MIDI message ROLI Dashboard sends. For some devices we don't know
those messages yet — most notably the **original Seaboard RISE (25/49, fw 1.x)**, where
GRAB currently shows settings read-only because the `0x78 3D <param>` write numbers are
confirmed only on the RISE 2.

To fill that gap we need a capture of **Dashboard talking to the device** while a few
settings are changed. GRAB can't record this itself (Dashboard owns the port), so it's a
quick USB capture. **This only observes traffic — it sends nothing to the device and never
touches firmware.**

There are two ways to do it: a **point-and-click** way (anyone, no repo needed) and a
**scripted** way (for developers who've cloned the repo). Either produces the same thing.

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

---

## Notes

- **macOS / Bluetooth:** USBPcap is Windows-only. On macOS, or for a BLE connection,
  capturing is more involved — open an issue and we'll advise (Apple's PacketLogger can
  capture Bluetooth HCI; USB-MIDI on macOS needs a different tap).
- **Privacy:** captures contain MIDI traffic and the device serial, nothing else. Share
  them the way you're comfortable with.
- **Safety:** capturing is passive. GRAB and these tools never send firmware-update,
  factory-reset, or program-upload messages — see the hard safety rules in
  [CONTRIBUTING.md](../CONTRIBUTING.md).
