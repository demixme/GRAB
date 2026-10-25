---
name: Connection problem
about: GRAB can't connect to / detect your ROLI device
title: "[connect] <model> - "
labels: connection
---

<!-- Note: Seaboard RISE and GRAND use a different protocol and are NOT supported yet
     (see https://github.com/demixme/GRAB/issues/1). GRAB may detect the port but never
     finish connecting — this report still helps us add support, so please file it. -->

**Device model:** (e.g. Seaboard Block, Seaboard RISE 25, Seaboard RISE 2, Seaboard GRAND)
**Firmware version (if known):**
**How connected:** USB / Bluetooth
**OS:** Windows / macOS (version)

**What happens**
<!-- e.g. "stuck on Connecting…", "No Seaboard found", "detected but no controls", a specific message. -->

**The port log (most useful part)**
GRAB logs every MIDI port it sees. Please paste the `=== MIDI inputs ===` and
`=== MIDI outputs ===` block — it shows each port's **name** and **identifier**.

Find the newest log here:
- **Windows:** `%APPDATA%\GRAB\logs\`
- **macOS:** `~/Library/Application Support/GRAB/logs/`

```
<!-- paste the MIDI inputs/outputs block here -->
```

**Anything else**
<!-- Is ROLI Dashboard/Connect installed or running? Does the device work in other MIDI apps? -->
