#!/usr/bin/env python3
"""Check generated (or hand-edited / Figma-exported) SVGs against GRAB's contract.

  1. JUCE-safe: only elements/attributes JUCE's SVG parser renders (no filter/mask/pattern/text/CSS).
  2. Layer-id contract from DESIGN-GUIDE.md (every live element present, ids unique).
  3. live-overlay exists, is empty, and is the topmost element.
  4. viewBox matches the device footprint (1 unit = 100 px).
  5. Every url(#ref) resolves.

Usage: python3 generator/validate.py art/*.svg
"""
import re, sys
import xml.etree.ElementTree as ET

NS = "{http://www.w3.org/2000/svg}"
ALLOWED = {"svg", "title", "desc", "defs", "g", "rect", "circle", "ellipse", "path", "line", "polyline",
           "polygon", "linearGradient", "radialGradient", "stop"}
BANNED_ATTRS = {"style", "class", "filter", "mask", "mix-blend-mode", "clip-path"}


def keybed_ids(prefix_w, prefix_b, low, count):
    w = b = 0
    for s in range(count):
        if (low + s) % 12 in {0, 2, 4, 5, 7, 9, 11}: w += 1
        else: b += 1
    return [f"{prefix_w}{i}" for i in range(w)] + [f"{prefix_b}{i}" for i in range(b)]


def required(root):
    fam = root.get("data-grab-family")
    req = ["chassis", "selection-frame", "live-overlay"]
    if fam in ("block", "rise", "grand"):
        low, count = map(int, root.get("data-grab-keys").split())
        req += ["surface", "keys-white", "keys-black", "shading"] + keybed_ids("w-", "b-", low, count)
    if fam == "rise":
        req += ["panel", "xy-pad", "xy-surface", "fader-tracks", "buttons", "button-leds", "labels", "panel-live",
                "btn-mode", "btn-oct-down", "btn-oct-up", "btnled-mode", "btnled-oct-down", "btnled-oct-up", "xy-dot"]
        req += [f"fader-track-{i}" for i in range(3)] + [f"fader-thumb-{i}" for i in range(3)]
    if fam == "lumi":
        low, count = map(int, root.get("data-grab-keys").split())
        req += ["keys", "diffuser"] + keybed_ids("key-w-", "key-b-", low, count)
    if fam == "lightpad":
        n = int(root.get("data-grab-grid", 15))
        req += ["grid-leds"] + [f"led-{r}-{c}" for r in range(n) for c in range(n)]
    if fam == "control":
        req += ["led-strip", "buttons", "button-leds", "icons", "btn-pill"]
        req += [f"led-{i}" for i in range(13)] + [f"btn-{i}" for i in range(8)] + [f"btnled-{i}" for i in range(10)]
    return fam, req


def check(path):
    errs = []
    root = ET.parse(path).getroot()
    ids = {}
    for el in root.iter():
        tag = el.tag.replace(NS, "")
        if tag not in ALLOWED:
            errs.append(f"unsupported element <{tag}>")
        for a in el.attrib:
            if a in BANNED_ATTRS:
                errs.append(f"<{tag}> uses banned attribute '{a}'")
        if "id" in el.attrib:
            if el.get("id") in ids:
                errs.append(f"duplicate id '{el.get('id')}'")
            ids[el.get("id")] = el
    fam, req = required(root)
    if fam is None:
        errs.append("missing data-grab-family")
    missing = [r for r in req if r not in ids]
    if missing:
        errs.append(f"missing ids: {', '.join(missing[:12])}{' ...' if len(missing) > 12 else ''}")
    ov = ids.get("live-overlay")
    if ov is not None:
        if len(ov):
            errs.append("live-overlay must be empty")
        if list(root)[-1] is not ov:
            errs.append("live-overlay must be the topmost (last) element")
    fp = root.get("data-grab-footprint")
    if fp:
        w, h = (float(v) * 100 for v in fp.split("x"))
        vb = [float(v) for v in root.get("viewBox", "").split()]
        if len(vb) != 4 or abs(vb[2] - w) > 0.5 or abs(vb[3] - h) > 0.5:
            errs.append(f"viewBox {vb} does not match footprint {fp}")
    for m in re.finditer(r"url\(#([^)]+)\)", open(path).read()):
        if m.group(1) not in ids:
            errs.append(f"unresolved url(#{m.group(1)})")
    return errs


def run(paths):
    ok = True
    for p in paths:
        errs = check(p)
        name = p.rsplit("/", 1)[-1]
        if errs:
            ok = False
            print(f"FAIL {name}")
            for e in sorted(set(errs)):
                print(f"     - {e}")
        else:
            print(f"ok   {name}")
    return ok


if __name__ == "__main__":
    sys.exit(0 if run(sys.argv[1:]) else 1)
