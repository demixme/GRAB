# GRAB profile store (demix.me backend)

A tiny PHP + MySQL backend for the device-profile "self-learn" store, implementing
the contract in [../DEVICE-PROFILES.md](../DEVICE-PROFILES.md):
- `POST /grab/v1/profiles` — accept a device profile.
- `GET  /grab/v1/names?family=&type=` — return the learned name map.
- `POST /grab/v1/diagnostics` — accept an anonymized connection diagnostic (ports
  seen + connect outcome; opt-in, for diagnosing/adding device support).
- `POST /grab/v1/captures` — accept a raw MIDI capture (what a device emitted; opt-in,
  for reverse-engineering unsupported devices; 4 MB cap).
- `GET  /grab/v1/export` — maintainer-only export of raw profiles, diagnostics
  (`?kind=diagnostics`) **or** captures (`?kind=captures`), token-gated (see below).

## Setup (shared hosting / cPanel style)
1. **Database:** create a MySQL database + user, then load the schema:
   ```sh
   mysql -u <user> -p <dbname> < schema.sql
   ```
2. **Credentials:** `db.php` finds them in this order:
   1. the `GRAB_CONFIG` env var (absolute path to a config), else
   2. a local `config.php` next to the scripts, else
   3. it **walks up the directory tree looking for `private/config.php`** — so if
      your site keeps secrets in a `/private/config.php` that `define()`s
      `DB_HOST`/`DB_NAME`/`DB_USER`/`DB_PASS` (the demix.me layout), **nothing extra
      is needed** — GRAB finds it and uses those constants.
   The config may either `return` the array (see `config.sample.php`) or `define()`
   the `DB_*` constants. Optionally `define('GRAB_DB_NAME', …)` to put the GRAB
   tables in a different database than `DB_NAME`. The tables can live in your
   existing database; just run `schema.sql` there.
3. **Upload** this folder to `https://demix.me/grab/v1/` (so the files sit at
   `/grab/v1/profiles.php`, `/grab/v1/names.php`, etc.).
4. **URLs:** with the included `.htaccess` (mod_rewrite), the endpoints are
   `/grab/v1/profiles` and `/grab/v1/names`. Without rewrite, use the `.php` names
   and point the tool's endpoint field at `.../grab/v1/profiles.php`.

The GRAB app's default endpoint is `https://demix.me/grab/v1/profiles` — set it in
the Contribute dialog (it's remembered).

## Test
```sh
# POST a sample profile
curl -i -X POST https://demix.me/grab/v1/profiles \
  -H 'Content-Type: application/json' \
  --data '{"schemaVersion":1,"tool":{"name":"GRAB","version":"0.1.0"},
           "device":{"family":"BLOCKS","type":"Seaboard","description":"test",
           "firmware":"1.4.3","transport":"USB","serialHash":"sha256:test","isMaster":true},
           "configItems":[{"id":13,"name":"Pressure Sensitivity","value":0,"min":0,"max":127,
           "type":"int","group":"Sensitivity","active":true}],"unknownConfigIds":[],"notes":"test"}'
# -> HTTP/1.1 201 {"message":"Thank you","id":1}

# GET the learned names
curl -s 'https://demix.me/grab/v1/names?family=BLOCKS&type=Seaboard'
# -> {"schemaVersion":1,"names":{"13":{"name":"Pressure Sensitivity","type":"int"}}}
```

## Export your data (maintainer only)
`GET /grab/v1/export` returns the **full raw profiles** as JSON, so you can develop
GRAB for new devices from real contributions. It's gated by an **export token** that
lives only in your server config (never in the app), and **fails closed** — until you
set a token, the endpoint returns 403.

Set the token (any long random string), e.g. in your config array `'exportToken' => '…'`
or `define('GRAB_EXPORT_TOKEN', '…')` in your private config. Then:
```sh
# everything (send the token as a header so it doesn't land in access logs)
curl -H 'X-Grab-Token: YOURTOKEN' 'https://www.demix.me/grab/v1/export'

# just one device type, newline-delimited JSON (one record per line)
curl -H 'X-Grab-Token: YOURTOKEN' \
  'https://www.demix.me/grab/v1/export?type=RISE&format=ndjson'

# the connection diagnostics instead of profiles
curl -H 'X-Grab-Token: YOURTOKEN' 'https://www.demix.me/grab/v1/export?kind=diagnostics'
```
Params: `kind=profiles|diagnostics|captures`, `family=` (profiles), `type=`, `format=json|ndjson`,
`limit=` (default 5000). JSON form is `{ "kind", "count", "records": [ { "id",
"receivedAt", "record": { …the full submitted JSON… } } ] }`. Nothing de-anonymizes
the data — serials are already hashed by the client.

## Notes
- Requires PHP 7.4+ (8.x fine) with PDO MySQL, and MySQL 5.7+/MariaDB 10.2+ for the
  `JSON` columns (swap to `LONGTEXT` in `schema.sql` if your host is older).
- Serials arrive already hashed by the client; nothing identifying is stored.
- `profiles.php` applies a best-effort **per-IP rate limit** (60 POSTs/hour by default;
  returns HTTP 429 over the limit). It stores only a salted hash of the IP in a temp
  file — no raw IPs, nothing in the DB. Tune the number in `grab_rate_limit()`. A
  `writeToken` can't gate the public GUI (the token would ship in the app), so it's
  only useful for a private/trusted uploader; rate-limiting is the open-contribution
  defense.
