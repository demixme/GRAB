# mitm-firmware.py — capture ROLI Connect/Dashboard firmware traffic with mitmproxy.
#
# Run (from the repo root, so output lands in captures/firmware/):
#   mitmdump -s tools/mitm-firmware.py
#
# It logs every api.roli.com request, saves JSON/manifest responses (which reveal
# the firmware version and download URL), and saves any binary payload (the
# firmware itself) to captures/firmware/. See tools/FIRMWARE-CAPTURE.md for the
# full procedure (cert trust + routing ROLI Connect through the proxy).
#
# Read-only: this only observes and archives traffic; it sends nothing to the device.

import os
import re
import datetime
from mitmproxy import http

OUT = os.path.join("captures", "firmware")
os.makedirs(OUT, exist_ok=True)
LOG = os.path.join(OUT, "roli-http.log")


def _log(line: str) -> None:
    stamp = datetime.datetime.now().strftime("%H:%M:%S")
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(f"[{stamp}] {line}\n")
    print("ROLI:", line)


def _safe_name(url: str, maxlen: int = 140) -> str:
    name = re.sub(r"[^A-Za-z0-9._-]", "_", url.split("://", 1)[-1])
    return name[:maxlen]


def _looks_like_firmware(url: str, content_type: str) -> bool:
    u = url.lower()
    return (
        any(k in u for k in ("firmware", ".bin", ".img", ".dfu", ".rfw"))
        or "octet-stream" in content_type
        or "application/mac-binary" in content_type
    )


def response(flow: http.HTTPFlow) -> None:
    url = flow.request.pretty_url
    host = flow.request.host.lower()

    # Only care about ROLI (and its CDN/S3) — ignore everything else.
    if not any(h in host for h in ("roli.com", "cloudfront.net", "amazonaws.com")):
        return

    ct = flow.response.headers.get("content-type", "")
    size = len(flow.response.content or b"")
    _log(f"{flow.request.method} {url} -> {flow.response.status_code} {ct} {size}B")

    # Save small JSON/text responses from the ROLI API (manifests reveal the
    # firmware version + download URL).
    if "roli.com" in host and ("json" in ct or "text" in ct) and size < 200_000:
        path = os.path.join(OUT, "json_" + _safe_name(url) + ".json")
        with open(path, "wb") as f:
            f.write(flow.response.content or b"")
        _log(f"  saved API response -> {path}")

    # Save any binary payload (the firmware itself).
    if _looks_like_firmware(url, ct) and size > 0:
        path = os.path.join(OUT, "fw_" + _safe_name(url) + ".bin")
        with open(path, "wb") as f:
            f.write(flow.response.content or b"")
        _log(f"  SAVED BINARY ({size}B) -> {path}")
