#!/usr/bin/env python3
"""V2: the device art as a tiling KIT instead of one SVG per model.

  tiles/      one key = one tile. GRAB scales each tile to (pitch, key-region height) and places it
              with the same loop KeyboardView already runs. Seaboard: white, gap, black (+ GRAND pill).
              LUMI: white, black.
  surrounds/  each body as a 3-slice: left cap (fixed), mid (stretched), right cap (fixed), plus the
              GRAND's centred master button as an ornament.
  fixed/      Lightpad and Control Block (no key count to vary): body + one icon set per variant.
  manifest.json  where the keys go in each surround, the pitch, which tiles to use.

Then every model is ASSEMBLED from the kit (assemble()), exactly as GRAB would, into art-v2/models/
and validated against the same layer-id contract as V1.

Run:  python3 generator/kit.py
"""
import json, os, re, sys
import build as B
from build import Doc, rr, poly, tones, S, L, f

ROOT = B.ROOT
OUT = os.path.join(ROOT, "art-v2")
KIT, MODELS = os.path.join(OUT, "kit"), os.path.join(OUT, "models")
H = 300                                   # every keyboard surround is 3 units tall

TW = 100                                  # tile width = one white-key pitch, in tile units
SEA_TH, LUMI_TH = 700, 550                # tile heights (roughly each family's key aspect)


def slab(x0, x1, y0, y1, r, tl=False, tr=False, br=False, bl=False):
    """Rect path with per-corner rounding - lets a body be cut into caps without seams."""
    p = f"M{f(x0 + (r if tl else 0))} {f(y0)}H{f(x1 - (r if tr else 0))}"
    if tr: p += f"A{f(r)} {f(r)} 0 0 1 {f(x1)} {f(y0 + r)}"
    p += f"V{f(y1 - (r if br else 0))}"
    if br: p += f"A{f(r)} {f(r)} 0 0 1 {f(x1 - r)} {f(y1)}"
    p += f"H{f(x0 + (r if bl else 0))}"
    if bl: p += f"A{f(r)} {f(r)} 0 0 1 {f(x0)} {f(y1 - r)}"
    p += f"V{f(y0 + (r if tl else 0))}"
    if tl: p += f"A{f(r)} {f(r)} 0 0 1 {f(x0 + r)} {f(y0)}"
    return p + "Z"


def body_slice(d, x0, x1, r, t, left=False, right=False):
    """A cut of the device shell: edge outline (as a 1px-larger fill, so cuts show no seam), base, lit rim."""
    d.add("path", d=slab(x0 - left, x1 + right, 2, H - 2, r + 1, left, right, right, left), fill=t["edge"])
    d.add("path", d=slab(x0, x1, 3, H - 3, r, left, right, right, left), fill=t["base"])
    lx0 = x0 + (r if left else 0)
    lx1 = x1 - (r if right else 0)
    if not left:
        d.add("path", d=f"M{f(lx0)} 4H{f(lx1)}", stroke=t["lit"], stroke_width=1, fill="none")
    if right:
        d.add("path", d=f"M{f(lx1)} 4A{f(r - 1)} {f(r - 1)} 0 0 1 {f(x1 - 1)} {f(3 + r)}V{f(H * 0.45)}",
              stroke=t["lit"], stroke_width=1, fill="none", stroke_linecap="round")


def tray_slice(d, x0, x1, y0, y1, r, t, left=False, right=False, floor="base"):
    """A cut of a sunken tray: deep strip along the top (and right wall on the right cap), floor."""
    k = L["inset"]
    d.add("path", d=slab(x0 - left, x1 + right, y0 - 1, y1 + 1, r + 1, left, right, right, left), fill=t["edge"])
    d.add("path", d=slab(x0, x1, y0, y1, r, left, right, right, left), fill=t["deep"])
    d.add("path", d=slab(x0, x1 - (k if right else 0), y0 + k, y1, max(1, r - k / 2), left, right, right, left), fill=t[floor])


# --------------------------------------------------------------------------------------------
# Tiles
# --------------------------------------------------------------------------------------------
def tile_doc(name, vb, th, family):
    x, w = vb
    d = Doc(w, th, name, {"tile": family, "anchor": "x=0 is the key's left edge (white) or the boundary it sits on (black/gap)"})
    d.vb = f"{f(x)} 0 {f(w)} {th}"
    return d


def sea_tiles():
    K = S["keywave"]
    t = tones("block")
    top, step, bot = K["railTop"] * SEA_TH, K["step"] * SEA_TH, K["railBottom"] * SEA_TH
    bw, band, pen = TW * K["blackW"], TW * K["band"], TW * K["penumbra"]
    hw = bw / 2
    x1, x2, x3, x4 = -hw - band, -hw, 0, band
    jog = (x3 - x1) * K["jog"]
    core = [(x1, top), (x2, top), (x2, step), (x4, step + jog), (x4, bot), (x3, bot), (x3, step + jog), (x1, step)]
    soft = [(x1 - pen, top), (x2, top), (x2, step), (x4 + pen, step + jog), (x4 + pen, bot), (x3, bot), (x3, step + jog), (x1 - pen, step)]

    white = tile_doc("Seaboard white key", (0, TW), SEA_TH, "seaboard")
    white.add("rect", id="key", x=0, y=top, width=TW, height=bot - top, fill=t["base"])

    gap = tile_doc("Seaboard gap (no black key on this boundary)", (-10, 45), SEA_TH, "seaboard")
    gx = -band * 0.25
    gap.add("rect", x=gx, y=top, width=band + pen, height=bot - top, fill=t["mid"])
    gap.add("rect", x=gx, y=top, width=band, height=bot - top, fill=t["deep"])

    black = tile_doc("Seaboard black key (ridge + its S-shadow)", (-62, 104), SEA_TH, "seaboard")
    black.add("path", d=poly(soft, True), fill=t["mid"])
    black.add("path", d=poly(core, True), fill=t["deep"])
    black.add("rect", id="key", x=-hw, y=top, width=bw, height=step - top, fill=t["lit"])
    cw = 1.6 * TW / 42.4                       # 1.6px at the Seaboard Block's pitch
    ly0, ly1 = top + (step - top) * 0.22, top + (step - top) * 0.93
    black.add("rect", x=-cw / 2, y=ly0, width=cw, height=ly1 - ly0, rx=cw / 2, fill=t["white"])

    # GRAND: pale raised pill. Radii are set so it reads round at the GRAND's own pitch/height.
    sx, sy = 21.75 / TW, 186 / SEA_TH          # tile -> px at GRAND scale
    pill = tile_doc("GRAND black key (pill + its S-shadow)", (-62, 104), SEA_TH, "seaboard")
    pill.add("path", d=poly(soft, True), fill=t["mid"])
    pill.add("path", d=poly(core, True), fill=t["deep"])
    pw = bw * 0.82
    rpx = pw * sx / 2
    cast = L["cast"]
    pill.add("rect", x=-pw / 2 - cast / sx, y=top + 2 + cast / sy, width=pw, height=step - top - 2, rx=rpx / sx, ry=rpx / sy, fill=t["deep"])
    pill.add("rect", id="key", x=-pw / 2, y=top + 2, width=pw, height=step - top - 2, rx=rpx / sx, ry=rpx / sy,
             fill=S["devices"]["grand"]["pill"])
    return {"seaboard-white": white, "seaboard-gap": gap, "seaboard-black": black, "grand-black": pill}


def lumi_tiles():
    dev, t = S["devices"]["lumi"], tones("lumi")
    gap, fw, face_w, fh = TW * dev["gap"], TW * dev["frameW"], TW * dev["faceW"], LUMI_TH * dev["blackH"]
    rim = (fw - face_w) / 2

    white = tile_doc("LUMI white key", (0, TW), LUMI_TH, "lumi")
    white.add("rect", id="key", x=gap / 2, y=0, width=TW - gap, height=LUMI_TH - 2, rx=7, fill=dev["whiteKey"])
    white.add("rect", x=gap / 2, y=0, width=TW - gap, height=fh, fill="#ffffff", fill_opacity=dev["keyTopWash"])
    white.add("rect", x=0, y=0, width=TW, height=7, fill="#000000", fill_opacity=0.5)

    black = tile_doc("LUMI black key (frame + face + its shadow)", (-60, 110), LUMI_TH, "lumi")
    s = TW * 0.22
    xa, xb, y1 = -fw / 2, fw / 2 - rim * 1.2, fh
    black.add("path", d=poly([(xa, y1 - rim * 1.2), (xa, y1 - rim * 0.3), (xa + rim * 0.9, y1), (xb, y1),
                              (xb - s, y1 + s), (xa - s, y1 + s - rim * 0.3), (xa - s, y1 - rim * 1.2 + s)], True),
              fill="#000000", fill_opacity=dev["castShadow"])
    black.add("rect", x=-fw / 2, y=-4, width=fw, height=fh + 4, rx=rim * 1.2, fill=t["edge"])
    black.add("rect", id="key", x=-face_w / 2, y=0, width=face_w, height=fh - rim, rx=face_w / 2, fill=dev["blackKey"])
    black.add("rect", x=-face_w / 2, y=0, width=face_w, height=fh - rim, rx=face_w / 2, fill="#ffffff", fill_opacity=dev["blackWash"])
    return {"lumi-white": white, "lumi-black": black}


# --------------------------------------------------------------------------------------------
# Surrounds (3-slice) - drawn in each slice's local coordinates
# --------------------------------------------------------------------------------------------
MID = 10      # the mid slice is authored 10 units wide; GRAB stretches it horizontally


def surrounds():
    out, spec = {}, {}

    # Seaboard Block --------------------------------------------------------------------------
    t, r, Lw, Rw = tones("block"), S["devices"]["block"]["bodyRadius"], 20, 20
    gy = 3 + (H - 6) * 0.07
    glyph = dict(fill="none", stroke=t["white"], stroke_opacity=0.5, stroke_width=0.9, stroke_linecap="round", stroke_linejoin="round")
    a = Doc(Lw, H, "Seaboard Block - left cap", {}); body_slice(a, 3, Lw, r, t, left=True)
    a.add("path", d=B.chevron_path(16, gy, 3.2, -1), **glyph)
    m = Doc(MID, H, "Seaboard Block - mid", {}); body_slice(m, 0, MID, r, t)
    z = Doc(Rw, H, "Seaboard Block - right cap", {}); body_slice(z, 0, Rw - 3, r, t, right=True)
    z.add("path", d=B.chevron_path(Rw - 16, gy, 3.2, 1), **glyph)
    out.update({"block-left": a, "block-mid": m, "block-right": z})
    spec["block"] = dict(left=Lw, right=Rw, keys=dict(left=3, right=3, top=3, bottom=3), radius=r)

    # RISE (25 and 49 share it) ---------------------------------------------------------------
    tb, tk, r = tones("rise", "body"), tones("rise"), S["devices"]["rise"]["bodyRadius"]
    Lw, Rw, ty0, ty1 = 150, 44, 29, 263
    a = Doc(Lw, H, "RISE - left cap (control panel)", {})
    body_slice(a, 3, Lw, r, tb, left=True)
    tray_slice(a, 144, Lw, ty0, ty1, 5, tk, left=True)
    B.rise_panel(a, tb)
    m = Doc(MID, H, "RISE - mid", {}); body_slice(m, 0, MID, r, tb); tray_slice(m, 0, MID, ty0, ty1, 5, tk)
    z = Doc(Rw, H, "RISE - right cap", {}); body_slice(z, 0, Rw - 3, r, tb, right=True); tray_slice(z, 0, 6, ty0, ty1, 5, tk, right=True)
    out.update({"rise-left": a, "rise-mid": m, "rise-right": z})
    spec["rise"] = dict(left=Lw, right=Rw, keys=dict(left=146, right=40, top=ty0 + 2, bottom=H - ty1 + 2), radius=r)

    # GRAND -----------------------------------------------------------------------------------
    tb, tk, r = tones("grand", "body"), tones("grand"), S["devices"]["grand"]["bodyRadius"]
    Lw, Rw, ty0, ty1 = 30, 30, 84, 274
    a = Doc(Lw, H, "GRAND - left cap", {}); body_slice(a, 3, Lw, r, tb, left=True); tray_slice(a, 24, Lw, ty0, ty1, 5, tk, left=True)
    m = Doc(MID, H, "GRAND - mid", {}); body_slice(m, 0, MID, r, tb); tray_slice(m, 0, MID, ty0, ty1, 5, tk)
    z = Doc(Rw, H, "GRAND - right cap", {}); body_slice(z, 0, Rw - 3, r, tb, right=True); tray_slice(z, 0, 6, ty0, ty1, 5, tk, right=True)
    o = Doc(60, 84, "GRAND - master button (centred ornament)", {})
    cx, cy, rb = 30, 44, 25
    o.add("circle", cx=cx, cy=cy, r=rb + 1, fill=tb["edge"])
    B.raised_circle(o, cx, cy, rb, tb["lit"], tb)
    o.add("circle", cx=cx, cy=cy, r=rb * 0.42, fill=tb["mid"], stroke=tb["deep"], stroke_width=0.8)
    out.update({"grand-left": a, "grand-mid": m, "grand-right": z, "grand-button": o})
    spec["grand"] = dict(left=Lw, right=Rw, keys=dict(left=26, right=26, top=ty0 + 2, bottom=H - ty1 + 2), radius=r,
                         ornament=dict(file="grand-button", centre_x=0.5, y=0))

    # LUMI / Piano ----------------------------------------------------------------------------
    t, r, bar = tones("lumi"), S["devices"]["lumi"]["bodyRadius"], round(H * 0.205)
    Lw, Rw, cy = 150, 70, bar / 2 + 2

    def lumi_slice(d, x0, x1, left=False, right=False):
        d.add("path", d=slab(x0, x1, 3, H - 3, r, left, right, right, left), fill=t["edge"])
        d.add("path", d=slab(x0, x1, 3, bar, r, left, right, False, False), fill=t["base"])
        d.add("path", d=f"M{f(x0 + (r if left else 0))} 4H{f(x1 - (r if right else 0))}", stroke=t["lit"], stroke_width=1, fill="none")
    a = Doc(Lw, H, "LUMI - left cap (controls)", {}); lumi_slice(a, 3, Lw, left=True)
    B.well(a, 30.5, cy - 9.5, 19, 19, 9.5, t, inset=1.6)
    B.well(a, 68, cy - 11, 70, 22, 11, t, inset=1.8)
    st = dict(fill="none", stroke=t["white"], stroke_width=1.4, stroke_linecap="round", stroke_linejoin="round")
    a.add("path", d=B.chevron_path(88, cy + 0.9, 4, -1), **st)
    a.add("path", d=B.chevron_path(118, cy + 0.9, 4, 1), **st)
    B.led_dot(a, "power-led", 39.5, cy + 0.8, 1.6, "#3ddc84", True)
    m = Doc(MID, H, "LUMI - mid", {}); lumi_slice(m, 0, MID)
    z = Doc(Rw, H, "LUMI - right cap (grille)", {}); lumi_slice(z, 0, Rw - 3, right=True)
    for i in range(4):
        rr(z, 16 + i * 7 + (3 if i >= 2 else 0), cy - 9, 2.4, 18, 1.2, fill=t["white"], fill_opacity=0.85)
    out.update({"lumi-left": a, "lumi-mid": m, "lumi-right": z})
    spec["lumi"] = dict(left=Lw, right=Rw, keys=dict(left=3, right=3, top=bar, bottom=4), radius=r)
    return out, spec


# --------------------------------------------------------------------------------------------
# Manifest: how GRAB assembles each family
# --------------------------------------------------------------------------------------------
def manifest(spec):
    sea = {"white": "seaboard-white", "gap": "seaboard-gap", "black": "seaboard-black", "tileHeight": SEA_TH}
    return {
        "_about": "GRAB device-art kit. Tiles are authored at 100 units per white-key pitch; scale each to "
                  "(pitch, key-region height) and place it: whites at their left edge, blacks and gaps on the "
                  "boundary between two whites. Surrounds are 3-slice: left/right fixed, mid stretched.",
        "tileWidth": TW,
        "families": {
            "block": {"surround": "block", **spec["block"], "tiles": sea, "pitch": 42.43},
            "rise":  {"surround": "rise", **spec["rise"], "tiles": sea, "pitch": 35.8},
            "grand": {"surround": "grand", **spec["grand"], "tiles": {**sea, "black": "grand-black"}, "pitch": 21.75},
            "lumi":  {"surround": "lumi", **spec["lumi"],
                      "tiles": {"white": "lumi-white", "black": "lumi-black", "gap": None, "tileHeight": LUMI_TH},
                      "pitch": 42.43, "pitchOverrides": {"ROLI Piano 49": 36.5}},
        },
        "fixed": {"lightpad": "lightpad-block", "control": "control-block + icons-<variant>"},
    }


# --------------------------------------------------------------------------------------------
# Assembly - what GRAB's KeyboardView would do with the kit
# --------------------------------------------------------------------------------------------
def inner(doc):
    return "\n".join(doc.root)


def place(doc, tx, ty, sx=1.0, sy=1.0, rename=None):
    body = inner(doc)
    if rename:
        body = body.replace('id="key"', f'id="{rename}"')
    return f'<g transform="translate({f(tx)} {f(ty)}) scale({f(sx)} {f(sy)})">{body}</g>'


def assemble(name, family, low, count, kit, man, pitch=None):
    fam = man["families"][family]
    pitch = pitch or fam["pitch"]
    keys, whites = B.key_layout(low, count)
    k = fam["keys"]
    W = round(k["left"] + whites * pitch + k["right"])
    kx, ky, kh = k["left"], k["top"], H - k["top"] - k["bottom"]
    tiles, th = fam["tiles"], fam["tiles"]["tileHeight"]
    sx, sy = pitch / TW, kh / th
    sur = fam["surround"]
    parts = [place(kit[f"{sur}-mid"], fam["left"] - 1, 0, (W - fam["left"] - fam["right"] + 2) / MID, 1),   # 1px overlap: no seams
             place(kit[f"{sur}-left"], 0, 0),
             place(kit[f"{sur}-right"], W - fam["right"], 0)]
    if "ornament" in fam:
        o = kit[fam["ornament"]["file"]]
        parts.append(place(o, W * fam["ornament"]["centre_x"] - o.w / 2, fam["ornament"]["y"]))
    blacks_at = {wc for kind, _, wc in keys if kind == "b"}
    lumi = family == "lumi"
    wpre, bpre = ("key-w-", "key-b-") if lumi else ("w-", "b-")
    whites_g = [place(kit[tiles["white"]], kx + wc * pitch, ky, sx, sy, f"{wpre}{i}") for kind, i, wc in keys if kind == "w"]
    gaps_g = [place(kit[tiles["gap"]], kx + wc * pitch, ky, sx, sy) for wc in range(1, whites)
              if tiles["gap"] and wc not in blacks_at]
    blacks_g = [place(kit[tiles["black"]], kx + wc * pitch, ky, sx, sy, f"{bpre}{i}") for kind, i, wc in keys if kind == "b"]

    meta = {"family": family, "footprint": f"{W/100:g}x{H/100:g}", "active-area": f"{f(kx)} {f(ky)} {f(whites * pitch)} {f(kh)}",
            "keys": f"{low} {count}", "assembled-from": "kit"}
    r = fam["radius"]
    sf = S["selectionFrame"]
    if lumi:
        layers = (f'<g id="chassis">{"".join(parts)}</g><g id="keys"><g id="keys-white">{"".join(whites_g)}</g>'
                  f'<g id="keys-black">{"".join(blacks_g)}</g></g><g id="diffuser"></g>')
    else:
        layers = (f'<g id="chassis">{"".join(parts)}</g><g id="surface"></g><g id="keys-white">{"".join(whites_g)}</g>'
                  f'<g id="shading">{"".join(gaps_g)}</g><g id="keys-black">{"".join(blacks_g)}</g>')
    m = " ".join(f'data-grab-{a}="{v}"' for a, v in meta.items())
    svg = (f'<?xml version="1.0" encoding="UTF-8"?>\n<!-- Assembled from the GRAB device-art kit by generator/kit.py -->\n'
           f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" {m}>\n<title>{name}</title>\n'
           f'{layers}\n<rect id="selection-frame" x="1.5" y="1.5" width="{W - 3}" height="{H - 3}" rx="{f(r + 1.5)}" fill="none" '
           f'stroke="{S["accent"]}" stroke-width="{sf["width"]}" stroke-opacity="{sf["opacity"]}"/>\n<g id="live-overlay"></g>\n</svg>\n')
    return svg, W


MODELS_LIST = [   # (file, title, family, lowNote, keyCount, pitch override)
    ("seaboard-block-24", "Seaboard Block", "block", 48, 24, None),
    ("rise-25", "Seaboard RISE 25", "rise", 48, 25, None),
    ("rise-49", "Seaboard RISE 2 / RISE 49", "rise", 48, 49, None),
    ("grand-studio-37", "Seaboard GRAND Studio", "grand", 48, 37, None),   # 608 x 338 mm
    ("grand-stage-61", "Seaboard GRAND Stage", "grand", 48, 61, None),     # 937 x 338 mm
    ("grand-88", "Seaboard GRAND 88 First Edition", "grand", 21, 88, None),  # 1313 x 338 mm, A0-C8, 88 made
    ("lumi-24", "LUMI Keys / ROLI Piano M", "lumi", 48, 24, None),
    ("piano-49", "ROLI Piano 49", "lumi", 48, 49, 36.5),
]


def write_doc(path, d):
    svg = d.svg()
    if getattr(d, "vb", None):
        svg = re.sub(r'viewBox="[^"]+"', f'viewBox="{d.vb}"', svg, count=1)
    with open(path, "w") as fh:
        fh.write(svg)
    return svg


def kit_check(paths):
    """Tiles/slices: JUCE-safe, and every tile has exactly one id="key" (the fillable face)."""
    import xml.etree.ElementTree as ET
    import validate as V
    ok = True
    for p in paths:
        root = ET.parse(p).getroot()
        errs = []
        for el in root.iter():
            tag = el.tag.replace(V.NS, "")
            if tag not in V.ALLOWED:
                errs.append(f"<{tag}>")
            errs += [a for a in el.attrib if a in V.BANNED_ATTRS]
        if root.get("data-grab-tile") and "gap" not in p and sum(1 for el in root.iter() if el.get("id") == "key") != 1:
            errs.append("tile needs exactly one id=key")
        if errs:
            ok = False
            print("FAIL", os.path.basename(p), errs)
    print(f"kit: {len(paths)} parts {'ok' if ok else 'with errors'}")
    return ok


def main():
    for p in (KIT, MODELS, os.path.join(KIT, "tiles"), os.path.join(KIT, "surrounds"), os.path.join(KIT, "fixed")):
        os.makedirs(p, exist_ok=True)
    kit = {**sea_tiles(), **lumi_tiles()}
    sur, spec = surrounds()
    kit.update(sur)
    man = manifest(spec)
    paths = []
    for name, d in kit.items():
        sub = "tiles" if name in ("seaboard-white", "seaboard-gap", "seaboard-black", "grand-black", "lumi-white", "lumi-black") else "surrounds"
        p = os.path.join(KIT, sub, name + ".svg"); write_doc(p, d); paths.append(p)
    # Fixed devices: Lightpad as-is; Control = one body + one icon layer per variant.
    p = os.path.join(KIT, "fixed", "lightpad-block.svg"); write_doc(p, B.lightpad())
    for v in ("live", "loop", "touch", "developer"):
        d = B.control(v)
        icons = next(e for e in d.root if e.startswith('<g id="icons"'))
        if v == "live":
            d.root = ['<g id="icons"></g>' if e is icons else e for e in d.root]
            open(os.path.join(KIT, "fixed", "control-block.svg"), "w").write(d.svg())
        open(os.path.join(KIT, "fixed", f"icons-{v}.svg"), "w").write(
            f'<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100" width="200" height="100">\n'
            f'<title>Control Block icons - {v}</title>\n{icons}\n</svg>\n')
    with open(os.path.join(KIT, "manifest.json"), "w") as fh:
        json.dump(man, fh, indent=2)
    ok = kit_check(paths)

    import validate
    built = []
    for slug, title, fam, low, count, pitch in MODELS_LIST:
        svg, W = assemble(title, fam, low, count, kit, man, pitch)
        p = os.path.join(MODELS, slug + ".svg")
        open(p, "w").write(svg)
        built.append((slug, title, W, p))
    for slug, title in (("lightpad-block", "Lightpad Block M"),) + tuple((f"control-{v}", f"Control Block · {v.title()}") for v in ("live", "loop", "touch", "developer")):
        d = B.lightpad() if slug == "lightpad-block" else B.control(slug.split("-")[1])
        p = os.path.join(MODELS, slug + ".svg"); open(p, "w").write(d.svg())
        built.append((slug, title, d.w, p))
    ok = validate.run([b[3] for b in built]) and ok
    n = sum(len(fs) for _, _, fs in os.walk(KIT))
    print(f"kit: {n} files -> {KIT}\nmodels: {len(built)} assembled -> {MODELS}")
    json.dump([(s, t, w) for s, t, w, _ in built], open(os.path.join(OUT, "models.json"), "w"))
    json.dump({k: v.title for k, v in kit.items()}, open(os.path.join(OUT, "parts.json"), "w"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
