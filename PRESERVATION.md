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

### How the licensing actually works (observed 2026-11-01)

Reconstructed from **ROLI Connect's own on-disk cache** on an authorised Windows machine — no
interception, no modification of ROLI's software. This is documented so owners understand what they
have, what will break, and when. **It is not a circumvention guide:** the entitlements are signed by
ROLI's private key, which we do not have and cannot reproduce, and GRAB will not patch the apps to
skip their licence check.

- **Login → JWT.** Connect signs in to `api.roli.com` (email/password) and receives a **JWT**
  (HS256), carried as an `x-auth-token` header and a `token` cookie scoped to `.roli.com`, valid
  **30 days** (`iat`/`exp` claims 2,592,000 s apart).
- **Signed entitlements.** Connect GETs `/api/v1/user/entitlements`, returning
  `{ uid, expiration, entitlements { "<public_id>": { id, name, enabled } … } }` — the owned products
  (Dashboard, each Equator sound pack, …). The response is **cryptographically signed** via the
  headers `x-signer-certificate`, `x-signer-certificate-signature`, `x-message-signature` (Ed25519 /
  signify-minisign `RW…` blobs). The apps verify this against ROLI's **embedded public key** — that is
  the licence proof.
- **28-day built-in expiry.** The signed blob's `expiration` is set **exactly 28 days** after issue,
  and the cache carries `cache-control: private, max-age=0, max-stale=2419200` (2,419,200 s = 28 days).
  So a machine runs **offline for up to ~28 days** on the last signed entitlement; after that an app
  must fetch a fresh one from `api.roli.com`.
- **Offline store the apps read:** Connect mirrors the signed response — **headers *and* body, so the
  signatures are included** — to `%APPDATA%\ROLI\Shared\…user…entitlements….json`
  (macOS: `~/Library/Application Support/ROLI/Shared/`). Dashboard / ROLI Studio / Equator validate
  against this when offline.
- **Node-lock:** a per-machine GUID in `C:\ProgramData\ROLI\Shared\machine.id` ties activation to the
  computer (macOS/Linux keep the same per-machine id under ROLI's shared data folder; exact path not
  verified here — treat the Windows path as the Windows-specific example).

**What it means for longevity.** As of 2026-11-01 the licence API is **still up** — `api.roli.com`,
`my.roli.com`, `content.roli.com` all answer; only the *support portal* is suspended. So authorisation
still works today: **log in on each machine while you can** to reset its 28-day clock. The hard limit is
that, once the API goes dark, a machine keeps authorising only ~28 days past its last successful refresh,
and the signed-and-expiring design means a lapsed licence **cannot legitimately be re-issued without
ROLI's signing key**. The durable fixes are all external and worth lobbying the administrator for:
**offline / perpetual licences, a minimal licence endpoint kept alive, or a licence-free farewell build.**

### ROLI Studio / Equator 2 — content libraries (download now while servers are up)

**ROLI Studio Player** and **ROLI Studio Drums** are the current JUCE instruments (VST3 + standalone;
the Studio Player hosts the **Equator 2** engine). The apps themselves are small (~35 MB each); their
**sound content is downloaded separately through ROLI Connect** from ROLI's servers, so it's at the
same risk as the licences and must be grabbed before the servers go.

#### Naming — mind the version numbers (they don't all mean the same thing)

- **Equator 1 vs Equator 2 are two different ROLI products that coexist.** **Equator 1** (a.k.a.
  "Equator" / "Equator Legacy") is the original synth with its own standalone + plugin installers
  (already in the local archive). **Equator 2** is the newer engine, shipped standalone *and* as the
  engine inside Studio Player, whose content is the big download below. Separate installers, separate
  licences, separate content libraries — so when someone says "Equator", ask **which one**. (Confusingly,
  *both* content libraries unpack under the `Equator2\` folder — see the table below.)
- **Strobe2 and Cypher2 are NOT a "version 2" split.** The "2" is just part of the product name — they're
  **FXpansion's** second-generation synths that ROLI took over. There is **no ROLI "Strobe 1 / Cypher 1"**
  to look for; the only versions in the ROLI catalogue are **Strobe2** and **Cypher2** (installers
  `v2.6.1.7`). Their distant ancestors (FXpansion's original Strobe/Cypher in "DCAM Synth Squad") predate
  ROLI and aren't part of this ecosystem. So don't go hunting for a missing "1" of these two.

How the content flows (observed on Windows): Connect downloads each library as a self-contained
**`.rpkg`** package into `%APPDATA%\ROLI\ROLI Connect\downloads\` (macOS:
`~/Library/Application Support/ROLI/ROLI Connect/downloads/`; Linux: `~/.config/ROLI/ROLI Connect/downloads/`).
On Windows these are staged as `.<hash>.<name>.rpkg` while in progress (prefix dropped when complete);
on macOS they're stored under **content-hash filenames with no extension** (e.g. `8e600e5b…-13`), so you
can't match them by name. Connect then hands the package to **ROLI Package Manager** —
a helper bundled inside Connect (`…\ROLI Connect\resources\app\resources\win32\bin\ROLI Package
Manager.exe`) — which unpacks it to `C:\Users\Public\Documents\ROLI\`.
(macOS: `~/Library/Application Support` + `/Users/Shared/ROLI` / `~/Music/ROLI`.) Because the Package
Manager ships *with* Connect, archiving the Connect installer preserves the unpack tool too; a kept
`.rpkg` should therefore be re-installable offline (worth confirming — playback still needs licence
auth regardless).

| Library | Package size | Package (`.rpkg`) | Unpacks to |
|---|---|---|---|
| **Equator2 Factory Library 1.0.0** | 4.89 GB | `Equator2 Factory Content-1.0.0.rpkg` | `Public\Documents\ROLI\Equator2\Factory Library\` |
| **Equator1 Legacy Library 1.1.0** | 1.29 GB | `Equator Legacy Content-1.1.0.rpkg` | (merges into) `Public\Documents\ROLI\Equator2\Factory Library\` |
| **Fundamentals soundpacks** (Equator / Cypher2 / Strobe2 / Drums) | ~266 MB | (bundled) | `Public\Documents\ROLI\Soundpacks\` |

Both libraries unpack under Equator 2's **own** `Equator2\` folder (~6.3 GB, ~600 presets + samples in
ROLI's format) — *not* the Equator 1 `Equator\` folder, which stays empty. Verified installed 2026-11-01.

The bundled synths **Strobe2** and **Cypher2** (FXpansion / "Make Believe Studios") also come down as
installers in the Connect downloads cache (`Strobe2_v2.6.1.7.exe`, `Cypher2_v2.6.1.7.exe`).

**To preserve:** in Studio Player, Install both libraries, then copy to your own archive:
- the two `.rpkg` packages from `…\ROLI Connect\downloads\` (cleanest — self-contained, re-installable),
- the unpacked `C:\Users\Public\Documents\ROLI\` folder (Equator libraries + Soundpacks),
- the `studioplayer-*.rpkg` / `studiodrums-*.rpkg` app packages and the Strobe2 / Cypher2 installers.

**Caveat:** archiving the content preserves the *sounds*, but Equator 2 / Studio still **authorise via
the licence model above**, so the samples alone won't play offline once a licence lapses — same reason
the only durable fix is a licence-free build. Keep the content *and* the licence-state backup.

**Owned soundpacks — download from ROLI Connect's "Sounds" tab (confirmed 2026-11-01).** The owned genre
soundpacks (5D EDM, Giant Dubstep, Hybrid Acoustic, World Colours, Session Keys, … ~30) *are* re-downloadable
— through the **canonical path**: **ROLI Connect ▸ Sounds ▸ Your Sounds**, which lists every owned pack with
an **Install** button and an **"Install All"**. They install as Equator 2 `.rpkg` into Studio Player / full
Equator / Strobe2 / Cypher2. So **log into Connect and Install All while the servers are up**, then archive.
- They are **deliberately absent from the My ROLI account page and the `/user/products` API** — ROLI's own
  docs say soundpacks aren't shown in My ROLI because *Connect* is what downloads/authorises them. (That's
  why probing `/user/products`, `/user/rpkgs` etc. finds nothing — the Sounds catalog is a separate
  Connect-only endpoint.)
- **Equator 1's** in-app Playlist/Preset Library *is* dead (*"Unable to connect to Playlist Library"*), but
  that's the old Equator-1 `.preset` delivery, not these Equator-2 `.rpkg` packs — don't confuse the two.
- **Studio Player** only tag-filters installed presets; its "Get more Sounds" opens the paid web store, not
  your owned downloads. Do the owned-pack install from **Connect's Sounds tab**, not Studio Player.
- Not bundled in the Equator app or Extended Library/Content installers (verified by reading those MSIs).

GRAB's "Archive my ROLI downloads" now **downloads owned soundpacks directly**: it reads Connect's
owned-soundpack catalog — **`GET api.roli.com/api/v1/user/products/soundpacks?variant=rpkg`** (auth via the
`x-access-token` / `token` cookie), which returns every owned pack with a pre-signed `download_url` to the
`.rpkg` on `builds.roli.com` (the signature authorises the file; no further auth). Works **free or paid** —
ownership is all that matters; there's no free/paid field. Packs already installed (their `.rpkg` in Connect's
downloads cache) are copied instead of re-fetched. The signed URLs are time-limited (~1 h), so GRAB fetches
the catalog when the panel opens and downloads in the same session. The community **"ROLI Vault"** archive
remains a useful cross-check / fallback for anyone whose account can't reach the catalog.

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
- **Your licence state** (per machine, while the API is up) — your current authorised snapshot:
  - `%APPDATA%\ROLI\Shared\` — the signed offline entitlement store (macOS: `~/Library/Application Support/ROLI/Shared/`)
  - `%APPDATA%\ROLI\ROLI Connect\API Proxy Cache\` — Connect's cached API responses (macOS:
    `~/Library/Application Support/ROLI/ROLI Connect/API Proxy Cache/<account>/…products…`; same
    per-account/URL-encoded layout as Windows — verified on a Mac Studio). **Note:** fresh/newer Connect
    installs may not write this at all — then the session token lives only in the next item.
  - **Connect's Electron Local Storage** — `…/ROLI Connect/Local Storage/leveldb/` (Windows:
    `%APPDATA%\ROLI Connect\…`; macOS: `~/Library/Application Support/ROLI Connect/…`). Holds the session
    **JWT** in readable form (plus an encrypted copy in the sibling `Cookies` DB). This is the reliable,
    always-present token source regardless of Connect version — GRAB's archive tool reads it as a fallback.
  - `C:\ProgramData\ROLI\Shared\machine.id` — the node-lock identity (Windows path; see above)
  These contain your **personal account token and licence data** — keep them **private** (never commit
  or share them), and note they stay valid only until the entitlement's embedded 28-day `expiration`.

Keep copies for yourself; don't publicly re-host ROLI's installers, firmware or manuals.
