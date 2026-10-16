# Archiving ROLI firmware (HTTPS capture) while api.roli.com is still up

ROLI Connect fetches device firmware over **HTTPS** from `api.roli.com` (AWS S3/
CloudFront behind it). Plain Wireshark only shows the hostname, so we use an HTTPS
intercept proxy (**mitmproxy**) to see the firmware URL and save the binary.

This only **observes and archives** network traffic. It sends nothing to the
Seaboard. It does not flash firmware.

## Caveats before you start
- **Certificate pinning:** if ROLI Connect pins its TLS certs, interception fails
  (you'll see TLS errors in mitmdump and the app can't connect). Electron apps
  often *don't* pin, so this usually works — but if it fails, stop; we can't MITM
  a pinned app without more effort.
- **Already up to date:** your Block is on `1.4.3`. If that's the latest, Dashboard
  won't download a binary — but the version-check/manifest response still reveals
  the firmware download URL, which we can then fetch directly.
- A trusted MITM root cert is a security risk if left installed. **Remove it when
  done** (last step).

## 1. Install mitmproxy (your choice of method)
- Standalone (no admin): download the Windows binaries from https://mitmproxy.org
  and unzip; or
- `winget install mitmproxy`, or `pip install mitmproxy`.

## 2. Generate + trust the mitmproxy CA cert
1. Run `mitmdump` once, then Ctrl-C. This creates
   `%USERPROFILE%\.mitmproxy\mitmproxy-ca-cert.cer`.
2. Trust it for the **Current User** (no admin needed):
   - Double-click the `.cer` → Install Certificate → Current User →
     "Place all certificates in the following store" → **Trusted Root
     Certification Authorities** → Finish. Accept the warning.
   - (Or `certutil -user -addstore Root "%USERPROFILE%\.mitmproxy\mitmproxy-ca-cert.cer"`)

## 3. Route ROLI Connect through the proxy
Pick one:
- **System proxy:** Settings → Network & internet → Proxy → Manual proxy setup →
  Address `127.0.0.1`, Port `8080`, Save. (Electron/Chromium follows this.)
- **Or launch flag:** close ROLI Connect, then start it with
  `--proxy-server=127.0.0.1:8080`.

## 4. Capture
From the repo root (so output lands in `captures/firmware/`):
```
mitmdump -s tools\mitm-firmware.py
```
Then in ROLI Connect / Dashboard:
- Click **Updates** (top-right) and let it check.
- If a firmware update is offered, **start it** to capture the binary. If it says
  you're up to date, that's fine — the manifest we captured still has the URL.

Watch the mitmdump window: it prints `ROLI: ...` lines and writes
`captures/firmware/roli-http.log`, `json_*.json` (API responses), and
`fw_*.bin` (any firmware binary).

## 5. Clean up
- Stop mitmdump (Ctrl-C).
- Turn the **system proxy back off** (or close the `--proxy-server` instance).
- **Remove the MITM cert** (security): `certutil -user -delstore Root mitmproxy`
  (or delete it from certmgr → Trusted Root).

## 6. Hand off
Give me the contents of `captures/firmware/` (the `roli-http.log` and any
`json_*`/`fw_*` files). I'll pick out the firmware version + download URL, and if
the binary wasn't downloaded (device already current), we can fetch it directly
from that URL while the server is up.
