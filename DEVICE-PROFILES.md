# Device profiles & self-learning

The tool can contribute an anonymized **device profile** — what config items a
connected device actually reports — to a central store you control. Aggregating
these lets the project *self-learn*: discover config ids on new/未-named devices
(e.g. RISE/GRAND), confirm value ranges, and build a community name map the tool
pulls back to label items it doesn't have names for.

**Opt-in, on demand.** Nothing is sent automatically. The user clicks "Contribute
device profile", sees exactly what will be sent, and chooses **Submit** (POST to
the configured endpoint) or **Save to file** (to attach to a GitHub issue/PR).

## Privacy
- The **serial number is never sent raw** — it's hashed (SHA-256, truncated) so
  the same device is counted once without being identifiable.
- Config *values* are the user's own settings (low sensitivity) and are included
  so ranges/defaults can be learned; the user can review before submitting.
- No account, IP logging, or personal data is part of the schema.

## Profile schema (v1)
`POST {endpoint}/v1/profiles`  (Content-Type: application/json)

```json
{
  "schemaVersion": 1,
  "submittedAt": "2026-10-25T12:00:00Z",
  "tool": { "name": "<tool>", "version": "0.1.0" },
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
      "options": []                     // option names when type == options
    }
    // ... one entry per active config id
  ],
  "unknownConfigIds": [96, 97],         // active on device but no name in our table
  "notes": ""                           // optional free text from the contributor
}
```

Response: `201 Created` (or `200`). Body is ignored by the client except for an
optional `{ "message": "..." }` shown to the user.

## Self-learn pull (optional, later)
`GET {endpoint}/v1/names?family=BLOCKS&type=Seaboard` → a map the tool merges into
its local name overrides (see `src/common/ConfigOverrides.h`):

```json
{ "schemaVersion": 1,
  "names": {
    "96": { "name": "User 0", "type": "int" },
    "24": { "type": "options", "options": ["Off","Octave","Fifth", "..."] }
  } }
```

This lets the tool label config items it shipped without names, once the community
has identified them — without a new release.

## Endpoint you build (demix.me)
The client just speaks **HTTP + JSON**, so the storage behind it is your choice —
a flat JSON store works, and **MySQL is a better fit** (query config ids per device
type, dedupe by `serialHash`, and build the `/names` map with SQL). A minimal
MySQL shape:
```sql
CREATE TABLE profiles (
  id          BIGINT AUTO_INCREMENT PRIMARY KEY,
  received_at DATETIME,
  family      VARCHAR(16), type VARCHAR(64), firmware VARCHAR(32),
  transport   VARCHAR(8),  serial_hash VARCHAR(80),
  raw_json    JSON
);
CREATE TABLE config_observations (
  profile_id  BIGINT, config_id INT, name VARCHAR(128),
  value INT, min_val INT, max_val INT, type VARCHAR(16), grp VARCHAR(64),
  INDEX (family_type_id)  -- or (type, config_id)
);
-- names map from curated/aggregated observations drives GET /v1/names
```

Minimal contract (same regardless of backend):
- `POST /v1/profiles` — append the posted JSON to a store (one file/record per
  submission, or keyed by `serialHash`+`firmware`). Return 201.
- `GET  /v1/names?family=&type=` — return the aggregated/curated name map above.
  (Can start as a hand-curated static file; grow it from submissions.)
- CORS/auth: none required for v1; add a write token later if spam is an issue.
- The base URL is **configurable in the tool**; default points at your demix.me
  path, and can be overridden (or left blank to force file-only mode).

## File fallback
"Save to file" writes the exact same JSON to `profiles/<type>-<fw>-<time>.json`
(gitignored). Attach it to a GitHub issue using the "Device profile" template, or
upload it to demix.me manually.

## Status
Schema + endpoint contract defined here. The **GUI client is built**: the GRAB
GUI's **Contribute...** button opens a dialog that builds this JSON from the
connected device (serial hashed), shows exactly what will be sent, lets you set the
endpoint + notes, and **Submit**s (POST) or **Save**s to a file. Still to build: the
`GET /v1/names` pull/merge step (so the tool learns names from the store). The
demix.me endpoint (MySQL-backed) is yours to host.
