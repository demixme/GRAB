# GRAB profile store (demix.me backend)

A tiny PHP + MySQL backend for the device-profile "self-learn" store, implementing
the contract in [../DEVICE-PROFILES.md](../DEVICE-PROFILES.md):
- `POST /grab/v1/profiles` — accept a device profile.
- `GET  /grab/v1/names?family=&type=` — return the learned name map.

## Setup (shared hosting / cPanel style)
1. **Database:** create a MySQL database + user, then load the schema:
   ```sh
   mysql -u <user> -p <dbname> < schema.sql
   ```
2. **Credentials:** create `config.php` (gitignored — never commit creds). Either
   fill in host/db/user/pass directly (option A in `config.sample.php`), or — to
   reuse an existing secret like `your_db_user` in a `/private` file — have
   `config.php` `require` that file and map its keys (option B), so no credentials
   sit in the web root. (You can also point the `GRAB_CONFIG` env var at a file
   that returns the array.) The tables can live in your existing database; just run
   `schema.sql` there.
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

## Notes
- Requires PHP 7.4+ (8.x fine) with PDO MySQL, and MySQL 5.7+/MariaDB 10.2+ for the
  `JSON` columns (swap to `LONGTEXT` in `schema.sql` if your host is older).
- Serials arrive already hashed by the client; nothing identifying is stored.
- Add rate-limiting / the `writeToken` if you see spam.
