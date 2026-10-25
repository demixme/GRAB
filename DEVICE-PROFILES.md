# Device profiles, diagnostics & self-learning

GRAB can contribute anonymized data to a small store you control, so the project can
*self-learn* — discover config ids on new/un-named devices (e.g. RISE/GRAND), confirm
ranges, build a community name map the tool pulls back, and gather the raw protocol of
devices it can't talk to yet.

**All opt-in, on demand, previewed.** Nothing is sent automatically; every dialog shows
exactly what will be sent and offers Save-to-file / Open-issue alternatives.

## What can be contributed (all from the GUI)
| Button | What it sends | Endpoint |
|---|---|---|
| **Help add device…** | one-click: a read-only identity request + short capture (never changes a setting) | `POST /v1/captures` |
| **Contribute…** | a device **profile** — the config items a connected device reports | `POST /v1/profiles` |
| **Advanced… → Raw MIDI capture** | a **raw MIDI capture** — everything a device emits (read-only) | `POST /v1/captures` |
| **Advanced… → Active probe** | an **active-probe session** — the message(s) sent + the device's reply | `POST /v1/captures` |

Plus the self-learn **pull** and the maintainer **export**:
- `GET /v1/names?family=&type=` — the community name map; GRAB merges it to label items
  it shipped without names (see `src/common/LearnedNames.h`). **Live.**
- `GET /v1/export?kind=profiles|diagnostics|captures` — maintainer-only, token-gated,
  to develop from the gathered data.

## Privacy
- The **serial is never sent raw** — SHA-256, truncated, so a device is deduped without
  being identifiable.
- Config *values* are the user's own settings (low sensitivity), included so ranges and
  defaults can be learned; reviewable before sending.
- No audio or played notes are sent — except what a device itself emits in an explicit
  raw **Capture** (which is read-only and shown live before you submit).
- No account. The store keeps no raw IP (the rate-limiter stores only a salted hash).

## Profile schema (v1)
`POST {endpoint}/v1/profiles`  (Content-Type: application/json)

```json
{
  "schemaVersion": 1,
  "submittedAt": "2026-10-25T12:00:00Z",
  "tool": { "name": "GRAB", "version": "0.1.7" },
  "device": {
    "family": "BLOCKS",                 // BLOCKS | RISE | GRAND
    "type": "Seaboard",                 // Block::Type name, or RISE/GRAND model
    "description": "Seaboard BLOCK (6x3)",
    "firmware": "1.4.3",
    "transport": "USB",                 // USB | BLE
    "serialHash": "sha256:1a2b3c4d",    // truncated hash, never the raw serial
    "isMaster": true
  },
  "configItems": [
    {
      "id": 13, "name": "Pressure Sensitivity",
      "value": 0, "min": 0, "max": 127,
      "type": "int",                    // int | bool | options | colour | float
      "group": "Sensitivity", "active": true,
      "options": []                     // in-range option names when type == options
    }
    // ... one entry per active config id
  ],
  "unknownConfigIds": [96, 97],         // active on device but no name in our table
  "notes": ""                           // optional free text from the contributor
}
```
Response: `201` with `{ "message": "...", "id": N }`.

The **diagnostics**, **capture** and **probe** payloads follow the same shape — a
`schemaVersion`, a `kind` (`connection-diagnostic` / `midi-capture`), a `tool` + `platform`,
and their data — see `src/gui/CapturePanel.h`, `src/gui/ProbePanel.h`.

## Names map (self-learn pull)
`GET {endpoint}/v1/names?family=BLOCKS&type=Seaboard` → a map GRAB merges into its names
(see `src/common/LearnedNames.h`):
```json
{ "schemaVersion": 1,
  "names": {
    "96": { "name": "User 0", "type": "int" },
    "24": { "type": "options", "options": ["Off","Octave","Fifth", "..."] }
  } }
```
This lets GRAB label items it shipped without names, once the community has identified
them — **without a new release**.

## The backend
**Built and hosted** — see [server/](server/) (PHP + MySQL): `profiles.php`, `names.php`,
`diagnostics.php`, `captures.php`, `export.php`, with the schema in
[server/schema.sql](server/schema.sql) (tables `grab_profiles`, `grab_config_observations`,
`grab_diagnostics`, `grab_captures`). The open `POST` endpoints are **per-IP rate-limited**
(hashed IP, no PII); `GET /export` is **export-token-gated** and fails closed. The base URL
is configurable in the tool (default `https://www.demix.me/grab/v1/`), and can be left
blank to force file-only mode.

## File / issue fallback
Every dialog can **Save to file** (under the per-user GRAB data dir) or **Open issue**
(a prefilled GitHub issue) instead of submitting — attach the saved file there.

## Status — all live
Contribute, Report, Capture, Probe, the `/names` pull and `/export` are all implemented
in the GUI and deployed on demix.me. (Earlier revisions of this doc called the pull
"to build" — it's done.)
