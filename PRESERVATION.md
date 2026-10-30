# ROLI gear — preservation notes

What survives ROLI's servers going dark, what doesn't, and where the open paths are. GRAB's own
job — reading and writing device **settings** over open MIDI, with no account and no server — is a
preservation tool by design. These notes record what we've found about the rest of the ecosystem.

(As of late 2026, roli.com has been reported intermittently offline, so this is not hypothetical.)

## Device configuration — safe

Configuring ROLI hardware is done entirely over **open MIDI SysEx** (`0x77` for BLOCKS, `0x78` for
RISE/GRAND) — no account, no server, no internet. GRAB does exactly this, cross-platform, and will
keep working regardless of ROLI's servers.

**ROLI Dashboard is also offline-safe for configuration.** We checked the installed build: it's a
self-contained app that configures devices locally; it references ROLI Connect only for *software
update checks* (optional) and account login only for *premium content* (NOT for device config). It
launches and configures with Connect absent and no login. So for settings, Dashboard survives a
shutdown too — only updates, analytics and content login break.

## Software instruments — server-leased (at risk)

**Equator2, ROLI Studio, Airwave Player** authenticate through **ROLI Connect** with a periodic
phone-home (~30 days / "up to 4 weeks offline"). When the servers are gone and the lease lapses,
they refuse to load — fully-paid software bricked, unless ROLI ships a licence-free build.

Mitigations while servers are up: keep the installers; keep each machine authorised; lobby ROLI /
administrators for an unlocked farewell build (the only thing that makes owned copies survive).

## Lightpad / BLOCKS apps — bundled, and loadable offline with open tooling

An "app" (Note Grid, Fader Block, Drum Block, …) is a **Littlefoot program** the device runs.

- **The apps are local.** The factory apps ship *with* ROLI Dashboard as `.littlefoot` source in
  `…\ROLI Dashboard\Littlefoot\` (+ shared includes in `LittleFootLibrary\`). Loading an app needs
  **no download** — Dashboard compiles and uploads the local file. So loading apps **survives ROLI's
  servers going dark** as long as Dashboard is installed.
- **How loading works** (`roli_blocks_basics`): `setProgram()` → `compileProgram()` (Littlefoot
  source → bytecode) → write to the device's **remote heap**, synced to the device in chunks with
  ACKs over `0x77`. Program size is capped per model (Lightpad **7200** bytes, Lightpad Pro 16384,
  Control 3000).
- **Both the Littlefoot compiler (`littlefoot/roli_LittleFootCompiler.h`) and the upload protocol
  are open source** — they're in the `roli_blocks_basics` library GRAB already vendors. So the whole
  chain is open: **local app source + open compiler + open protocol = any factory app can be loaded
  onto a BLOCKS device with no ROLI software and no servers.** That makes the apps fully preservable.
- App **settings** (Fader CCs, Drum pad notes…) are program-internal, not device config — see
  [LIGHTPAD-APPS.md](LIGHTPAD-APPS.md). Only the default **Note Grid** app's settings are standard
  config, which GRAB configures.

**GRAB itself never uploads programs** — program/firmware upload is the brick-risk line it won't
cross. The point above is that a *separate* tool could, using entirely open components.

## Airwave — Ultraleap hardware, leased software

- The camera is a pair of **Ultraleap IR 170** modules; it works with **Ultraleap's own SDK**,
  independent of ROLI. So the hardware outlives ROLI.
- **ROLI Airwave Control / Player** are server-leased like the synths above.
- The gesture output is a documented, fixed CC set (`CC21–31`) — see GRAB's in-app
  *Advanced ▸ Airwave* panel. An open, ROLI-free "Airwave" (Ultraleap SDK → those CCs) is feasible.

## What to archive now (to your own disk, while servers are up)

- **Device firmware** — GRAB's *Advanced ▸ Download firmware for archiving*.
- **ROLI Dashboard** installer (offline-safe config + the bundled apps + the open library is public).
- **Equator2 / Studio / Airwave Player** installers (even though they're licence-leased).
- **Ultraleap SDK** from ultraleap.com (the durable path for Airwave's camera).

Keep copies for yourself; don't publicly re-host ROLI's installers, firmware or manuals.
