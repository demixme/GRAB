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
import math
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


def rounded(pts, radii):
    """Closed polygon with each corner rounded (cubic, ~circular). radii[i] is clamped to half of each
    neighbouring edge so caps can go fully round."""
    n, out = len(pts), []
    for i in range(n):
        (px, py), (cx, cy), (nx, ny) = pts[i - 1], pts[i], pts[(i + 1) % n]
        la, lb = math.hypot(cx - px, cy - py), math.hypot(nx - cx, ny - cy)
        c = min(radii[i], la / 2, lb / 2)
        ax, ay = cx + (px - cx) * c / la, cy + (py - cy) * c / la
        bx, by = cx + (nx - cx) * c / lb, cy + (ny - cy) * c / lb
        k = 0.5523
        out.append(("L" if out else "M") + f"{f(ax)} {f(ay)}")
        if c > 0:
            out.append(f"C{f(ax + (cx - ax) * k)} {f(ay + (cy - ay) * k)} {f(bx + (cx - bx) * k)} {f(by + (cy - by) * k)} {f(bx)} {f(by)}")
    return "".join(out) + "Z"


def merge(a, b):
    """Deep merge: b's values override a's (used for per-device trace overrides)."""
    out = dict(a)
    for kk, v in b.items():
        out[kk] = merge(a[kk], v) if isinstance(v, dict) and isinstance(a.get(kk), dict) else v
    return out


def sea_tiles_trace(K, T=None, prefix="seaboard", title="Seaboard", px_scale=None):
    """Keywave redrawn from an Illustrator trace (Seaboard M by default): same structure as before
    (white / gap / black tiles), shapes and greys measured from the trace, then simplified.
    With T["pill"] the black key is a raised pill (GRAND) instead of the ridge."""
    T = T or K["trace"]; c = T["colour"]; th = SEA_TH
    X = lambda v: v * TW
    Y = lambda v: v * th
    cap, bend = K.get("caps", 0) * 999, K.get("bend", 0) * TW
    V, R = T["valley"], T["ridge"]

    def s_shape(ux0, ux1, lx0, lx1, top, bot):
        pts = [(X(ux0), Y(top)), (X(ux1), Y(top)), (X(ux1), Y(V["stepTop"][0])), (X(lx1), Y(V["stepTop"][1])),
               (X(lx1), Y(bot)), (X(lx0), Y(bot)), (X(lx0), Y(V["stepBottom"][1])), (X(ux0), Y(V["stepBottom"][0]))]
        return rounded(pts, [cap, cap, bend, bend, cap, cap, bend, bend])

    def band(d, x0, x1, y0, y1, fill, r=None, **a):
        w = X(x1) - X(x0)
        d.add("rect", x=X(x0), y=Y(y0), width=w, height=Y(y1) - Y(y0), rx=w / 2 if r is None else r, fill=fill, **a)

    white = tile_doc(f"{title} white key", (0, TW), th, "seaboard")
    white.add("rect", id="key", x=0, y=Y(V["top"]) - 8, width=TW, height=Y(V["bottom"]) - Y(V["top"]) + 16, fill=c["face"])
    if "toe" in T:                                    # lit rounded end of each white key (GRAND)
        to = T["toe"]
        w = X(to["x"][1] - to["x"][0]); sx, sy = px_scale
        white.add("rect", x=X(to["x"][0]), y=Y(to["y"][0]), width=w, height=Y(to["y"][1] - to["y"][0]),
                  rx=w / 2, ry=min(Y(to["y"][1] - to["y"][0]) / 2, w / 2 * sx / sy), fill=c["toe"])

    G = T["gap"]
    gap = tile_doc(f"{title} gap (no black key on this boundary)", (-10, 70), th, "seaboard")
    band(gap, G["soft"][0], G["soft"][1], *G["y"], c["soft"])
    band(gap, G["deep"][0], G["deep"][1], *G["y"], c["deep"])

    black = tile_doc(f"{title} black key ({'pill' if 'pill' in T else 'ridge'} + its S-shadow)", (-62, 124), th, "seaboard")
    LL = T.get("lowerLit")
    if LL:
        black.add("rect", x=X(LL["x"][0]), y=Y(LL["y"][0]), width=X(LL["x"][1] - LL["x"][0]), height=Y(LL["y"][1] - LL["y"][0]),
                  rx=X(0.06), fill=c["ridge"])
    black.add("path", d=s_shape(V["softUpper"], V["upper"][1], V["softLower"][0], V["softLower"][1], V["top"] - 0.01, V["bottom"] + 0.01), fill=c["soft"])
    black.add("path", d=s_shape(V["upper"][0], V["upper"][1], V["lower"][0], V["lower"][1], V["top"], V["bottom"]), fill=c["deep"])
    if "pill" in T:                                   # GRAND: pale pill in a dark halo, round at the GRAND's own scale
        P = T["pill"]; sx, sy = px_scale
        for half, y0, y1, fill, idv in ((P["halo"], P["y"][0] - P["haloY"], P["y"][1] + P["haloY"], c["halo"], None),
                                        (P["half"], P["y"][0], P["y"][1], c["pill"], "key")):
            w = X(2 * half); rpx = w * sx / 2
            black.add("rect", **({"id": idv} if idv else {}), x=-w / 2, y=Y(y0), width=w, height=Y(y1 - y0), rx=rpx / sx, ry=rpx / sy, fill=fill)
        return {f"{prefix}-white": white, f"{prefix}-gap": gap, f"{prefix}-black": black}
    rx0, rx1 = R["x"]; ry0, ry1 = R["y"]; rw = X(rx1 - rx0)
    black.add("rect", id="key", x=X(rx0), y=Y(ry0), width=rw, height=Y(ry1 - ry0), rx=rw / 2 * K.get("caps", 0), ry=min(rw / 2, X(0.12)), fill=c["ridge"])
    black.add("path", d=rounded([(X(rx0), Y(ry0)), (X(rx1), Y(ry0)), (X(rx1), Y(R["capTo"])), (X(rx0), Y(R["capTo"]))],
                                 [X(0.12), X(0.12), 0, 0]), fill=c["cap"])
    black.add("path", d=rounded([(X(rx0), Y(R["capTo"])), (X(R["shoulderTo"]), Y(R["capTo"])), (X(R["shoulderTo"]), Y(ry1)), (X(rx0), Y(ry1))],
                                 [0, 0, X(0.06), X(0.06)]), fill=c["shoulder"])
    black.add("path", d=rounded([(X(R["litFrom"]), Y(R["litY"])), (X(rx1), Y(R["litY"])), (X(rx1), Y(ry1)), (X(R["litFrom"]), Y(ry1))],
                                 [X(0.07), 0, X(0.06), X(0.07)]), fill=c["litEdge"])
    cw = X(T["crest"]["w"])
    band(black, -T["crest"]["w"] / 2, T["crest"]["w"] / 2, *T["crest"]["y"], c["crest"])
    return {f"{prefix}-white": white, f"{prefix}-gap": gap, f"{prefix}-black": black}


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
    cap, bend = K.get("caps", 0) * 999, K.get("bend", 0) * TW
    radii = [cap, cap, bend, bend, cap, cap, bend, bend]   # TL, TR, step bends, BR, BL, bends
    core_d, soft_d = rounded(core, radii), rounded(soft, radii)

    white = tile_doc("Seaboard white key", (0, TW), SEA_TH, "seaboard")
    white.add("rect", id="key", x=0, y=top, width=TW, height=bot - top, fill=t["base"])

    gap = tile_doc("Seaboard gap (no black key on this boundary)", (-10, 45), SEA_TH, "seaboard")
    gx = -band * 0.25
    gr = K.get("caps", 0)
    gap.add("rect", x=gx, y=top, width=band + pen, height=bot - top, rx=(band + pen) / 2 * gr, fill=t["mid"])
    gap.add("rect", x=gx, y=top, width=band, height=bot - top, rx=band / 2 * gr, fill=t["deep"])

    black = tile_doc("Seaboard black key (ridge + its S-shadow)", (-62, 104), SEA_TH, "seaboard")
    black.add("path", d=soft_d, fill=t["mid"])
    black.add("path", d=core_d, fill=t["deep"])
    black.add("rect", id="key", x=-hw, y=top, width=bw, height=step - top, rx=hw * K.get("caps", 0), fill=t["lit"])
    cw = 1.6 * TW / 42.4                       # 1.6px at the Seaboard Block's pitch
    ly0, ly1 = top + (step - top) * 0.22, top + (step - top) * 0.93
    black.add("rect", x=-cw / 2, y=ly0, width=cw, height=ly1 - ly0, rx=cw / 2, fill=t["white"])

    # GRAND: pale raised pill. Radii are set so it reads round at the GRAND's own pitch/height.
    sx, sy = 21.75 / TW, 186 / SEA_TH          # tile -> px at GRAND scale
    pill = tile_doc("GRAND black key (pill + its S-shadow)", (-62, 104), SEA_TH, "seaboard")
    pill.add("path", d=soft_d, fill=t["mid"])
    pill.add("path", d=core_d, fill=t["deep"])
    pw = bw * 0.82
    rpx = pw * sx / 2
    cast = L["cast"]
    pill.add("rect", x=-pw / 2 - cast / sx, y=top + 2 + cast / sy, width=pw, height=step - top - 2, rx=rpx / sx, ry=rpx / sy, fill=t["deep"])
    pill.add("rect", id="key", x=-pw / 2, y=top + 2, width=pw, height=step - top - 2, rx=rpx / sx, ry=rpx / sy,
             fill=S["devices"]["grand"]["pill"])
    out = {"seaboard-white": white, "seaboard-gap": gap, "seaboard-black": black, "grand-black": pill}
    if K.get("look") == "trace":
        out.update(sea_tiles_trace(K))
        G = S["devices"]["grand"]
        if G.get("look") == "trace":
            out.update(sea_tiles_trace(K, merge(K["trace"], G["trace"]), "grand", "GRAND", (21.75 / TW, 186 / SEA_TH)))
    return out


def lumi_tiles_trace(dev):
    T = dev["trace"]; th = LUMI_TH; X = lambda v: v * TW; Y = lambda v: v * th
    Wt, Bk = T["white"], T["black"]; bc = Bk["colour"]
    white = tile_doc("LUMI white key", (0, TW), th, "lumi")
    g = X(Wt["gap"]) / 2
    white.add("path", d=rounded([(g, -2), (TW - g, -2), (TW - g, th), (g, th)], [0, 0, X(Wt["radius"]), X(Wt["radius"])]),
              id="key", fill=dev["whiteKey"])
    white.add("rect", x=g, y=0, width=TW - 2 * g, height=Y(Wt["topShade"]), fill="#000000", fill_opacity=0.45)

    black = tile_doc("LUMI black key (rim + frame + bevel + face)", (-50, 100), th, "lumi")
    def u(half, bottom, r, fill, **a):
        black.add("path", d=rounded([(-X(half), -8), (X(half), -8), (X(half), Y(bottom)), (-X(half), Y(bottom))], [0, 0, r, r]), **a, fill=fill)
    r = X(Bk["radius"])
    u(Bk["rim"], Bk["rimBottom"], r, bc["rim"])
    u(Bk["frame"], Bk["frameBottom"], r * 0.8, bc["frame"])
    u(Bk["bevel"], Bk["bevelBottom"], X(0.05), bc["bevel"])
    u(Bk["face"], Bk["faceBottom"], X(0.04), dev["blackKey"], id="key")
    return {"lumi-white": white, "lumi-black": black}


def lumi_tiles():
    dev, t = S["devices"]["lumi"], tones("lumi")
    if dev.get("look") == "trace":
        return lumi_tiles_trace(dev)
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


def lumi_surround_trace(out, spec, dev):
    """LUMI / Piano body from the piano-m2 trace: dark outline, flat bar with a lit top line, black keybed,
    light front lip along the bottom (and right) edge, controls at the trace's own positions."""
    T = dev["trace"]; c = T["body"]; C = T["controls"]; r = dev["bodyRadius"]
    bar = round(H * T["barH"]); Lw, Rw = 150, 70

    def sl(d, x0, x1, left=False, right=False):
        # Layers never overlap (even-odd cut-outs), so the 3-slice joins anti-alias to the same colour: no seams.
        body = slab(x0, x1, 3, H - 3, r, left, right, right, left)
        top = slab(x0, x1, 3, bar, r, left, right, False, False)
        bed = slab(x0, x1 - (2.5 if right else 0), bar, H - 5.5, r, False, False, right, left)
        d.add("path", d=slab(x0 - left, x1 + right, 2, H - 2, r + 1, left, right, right, left) + body, fill=c["outline"], fill_rule="evenodd")
        d.add("path", d=body + top + bed, fill=c["lip"], fill_rule="evenodd")
        d.add("path", d=bed, fill=c["keybed"])
        d.add("path", d=top, fill=c["bar"])
        # No lit top line: at GRAB's render size the barLit stroke reads as a thick light-grey line
        # across the top of the LUMI/Piano (like the rim we dropped on the keyboards). Leave it flat.

    a = Doc(Lw, H, "LUMI - left cap (controls)", {}); sl(a, 3, Lw, left=True)
    bx, by, br = C["button"]
    a.add("circle", cx=bx, cy=by, r=br, fill=c["controlRim"])
    a.add("circle", cx=bx, cy=by, r=br - 1, fill=c["control"])
    B.led_dot(a, "power-led", bx, by, 1.6, c["led"], True)
    px, py, pw, ph = C["pill"]
    rr(a, px, py, pw, ph, ph / 2, fill=c["controlRim"])
    rr(a, px + 1, py + 1, pw - 2, ph - 2, ph / 2 - 1, fill=c["control"])
    ax0, ax1, ay, s = C["arrows"]
    a.add("path", d=poly([(ax0 - s / 2, ay), (ax0 + s / 2, ay - s * 0.55), (ax0 + s / 2, ay + s * 0.55)], True), fill=c["glyph"])
    a.add("path", d=poly([(ax1 + s / 2, ay), (ax1 - s / 2, ay - s * 0.55), (ax1 - s / 2, ay + s * 0.55)], True), fill=c["glyph"])
    m = Doc(MID, H, "LUMI - mid", {}); sl(m, 0, MID)
    z = Doc(Rw, H, "LUMI - right cap (LED bars)", {}); sl(z, 0, Rw - 3, right=True)
    Bs = C["bars"]
    for x in Bs["x"]:
        rr(z, x, Bs["y"][0], Bs["w"], Bs["y"][1] - Bs["y"][0], Bs["w"] / 2, fill=c["glyph"])
    out.update({"lumi-left": a, "lumi-mid": m, "lumi-right": z})
    spec["lumi"] = dict(left=Lw, right=Rw, keys=dict(left=3, right=3, top=bar, bottom=5.5), radius=r)


def grand_surround_trace(out, spec, dev):
    """GRAND body from the Stage trace: cool charcoal body, lighter recessed key tray with a dark rim and a
    shadowed top strip, lit front lip, master button with a small mark. Regions never overlap, so the
    3-slice joins don't show seams."""
    c = dev["trace"]["body"]; r = dev["bodyRadius"]
    Lw, Rw, ty0, ty1, tr, k, lip = 30, 30, 84, 274, 5, 4.5, 14

    def sl(d, x0, x1, tx0, tx1, left=False, right=False):
        body = slab(x0, x1, 3, H - 3, r, left, right, right, left)
        upper = slab(x0, x1, 3, H - 3 - lip, r, left, right, right, left)
        tray_o = slab(tx0 - left, tx1 + right, ty0 - 1, ty1 + 1, tr + 1, left, right, right, left)
        tray_i = slab(tx0, tx1, ty0, ty1, tr, left, right, right, left)
        floor = slab(tx0, tx1, ty0 + k, ty1, max(1, tr - k / 2), left, right, right, left)
        d.add("path", d=slab(x0 - left, x1 + right, 2, H - 2, r + 1, left, right, right, left) + body, fill=c["outline"], fill_rule="evenodd")
        if flat:   # one body colour: no lit front lip
            d.add("path", d=body + tray_o, fill=c["base"], fill_rule="evenodd")
        else:
            d.add("path", d=body + upper, fill=c["lip"], fill_rule="evenodd")
            d.add("path", d=upper + tray_o, fill=c["base"], fill_rule="evenodd")
        d.add("path", d=tray_o + tray_i, fill=c["trayRim"], fill_rule="evenodd")
        d.add("path", d=tray_i + floor, fill=c["trayInset"], fill_rule="evenodd")
        d.add("path", d=floor, fill=c["tray"])

    flat = dev["trace"].get("flat", False)
    a = Doc(Lw, H, "GRAND - left cap", {}); sl(a, 3, Lw, 24, Lw, left=True)
    m = Doc(MID, H, "GRAND - mid", {}); sl(m, 0, MID, 0, MID)
    z = Doc(Rw, H, "GRAND - right cap", {}); sl(z, 0, Rw - 3, 0, 6, right=True)
    o = Doc(60, 84, "GRAND - master button (centred ornament)", {})
    cx, cy, rb = 30, 44, 25
    o.add("circle", cx=cx, cy=cy, r=rb + 1, fill=c["outline"])
    if flat:   # plain disc, no ring or offset shadow
        o.add("circle", cx=cx, cy=cy, r=rb, fill=c["buttonFace"])
        o.add("circle", cx=cx, cy=cy, r=1.8, fill=c["buttonMark"])
    else:
        o.add("circle", cx=cx, cy=cy, r=rb, fill=c["buttonRing"])
        o.add("circle", cx=cx + 1, cy=cy + 1, r=rb - 3, fill=c["buttonFace"])
        o.add("circle", cx=cx + 2, cy=cy, r=1.8, fill=c["buttonMark"])
    out.update({"grand-left": a, "grand-mid": m, "grand-right": z, "grand-button": o})
    spec["grand"] = dict(left=Lw, right=Rw, keys=dict(left=26, right=26, top=ty0 + 2, bottom=H - ty1 + 2), radius=r,
                         ornament=dict(file="grand-button", centre_x=0.5, y=0))


def rise_surround_trace(out, spec, dev):
    """RISE casing from the RISE 2 trace: lighter grey top body, darker front band below the tray, light lip
    along the bottom and right edges, near-black tray rim. Keys stay the Block's (same tiles). Regions never
    overlap, so the 3-slice joins don't show seams."""
    T = dev["trace"]; c = T["body"]; r = dev["bodyRadius"]
    Lw, Rw, ty0, ty1, tr, k = 150, 44, 29, 263, 5, 3
    yF, lipW = ty1 + 4, 2.5

    def sl(d, x0, x1, tx0, tx1, left=False, right=False):
        body = slab(x0, x1, 3, H - 3, r, left, right, right, left)
        xr = x1 - (lipW if right else 0)
        upper = slab(x0, xr, 3, yF, r, left, right, False, False)
        front = slab(x0, xr, yF, H - 3 - lipW, r, False, False, right, left)
        tray_o = slab(tx0 - left, tx1 + right, ty0 - 1, ty1 + 1, tr + 1, left, right, right, left)
        tray_i = slab(tx0, tx1, ty0, ty1, tr, left, right, right, left)
        floor = slab(tx0, tx1, ty0 + k, ty1, max(1, tr - k / 2), left, right, right, left)
        d.add("path", d=slab(x0 - left, x1 + right, 2, H - 2, r + 1, left, right, right, left) + body, fill=c["outline"], fill_rule="evenodd")
        if T.get("flat"):   # one body colour: no lip, no front band
            d.add("path", d=body + tray_o, fill=c["base"], fill_rule="evenodd")
        else:
            d.add("path", d=body + upper + front, fill=c["lip"], fill_rule="evenodd")
            d.add("path", d=front, fill=c["front"])
            d.add("path", d=upper + tray_o, fill=c["base"], fill_rule="evenodd")
        d.add("path", d=tray_o + tray_i, fill=c["trayRim"], fill_rule="evenodd")
        d.add("path", d=tray_i + floor, fill=c["trayInset"], fill_rule="evenodd")
        d.add("path", d=floor, fill=c["tray"])

    a = Doc(Lw, H, "RISE - left cap (control panel)", {}); sl(a, 3, Lw, 144, Lw, left=True)
    B.rise_panel(a, {**tones("rise", "body"), **T["panel"]})
    m = Doc(MID, H, "RISE - mid", {}); sl(m, 0, MID, 0, MID)
    z = Doc(Rw, H, "RISE - right cap", {}); sl(z, 0, Rw - 3, 0, 6, right=True)
    out.update({"rise-left": a, "rise-mid": m, "rise-right": z})
    spec["rise"] = dict(left=Lw, right=Rw, keys=dict(left=146, right=40, top=ty0 + 2, bottom=H - ty1 + 2), radius=r)


def surrounds():
    out, spec = {}, {}

    # Seaboard Block --------------------------------------------------------------------------
    t, r, Lw, Rw = tones("block", "body"), S["devices"]["block"]["bodyRadius"], 20, 20
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
    if S["devices"]["rise"].get("look") == "trace":
        rise_surround_trace(out, spec, S["devices"]["rise"])
    else:
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
    if S["devices"]["grand"].get("look") == "trace":
        grand_surround_trace(out, spec, S["devices"]["grand"])
    else:
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
    dev = S["devices"]["lumi"]
    if dev.get("look") == "trace":
        lumi_surround_trace(out, spec, dev)
    else:
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
            "grand": {"surround": "grand", **spec["grand"], "pitch": 21.75,
                      "tiles": ({"white": "grand-white", "gap": "grand-gap", "black": "grand-black", "tileHeight": SEA_TH}
                                if S["devices"]["grand"].get("look") == "trace" else {**sea, "black": "grand-black"})},
            "lumi":  {"surround": "lumi", **spec["lumi"],
                      "tiles": {"white": "lumi-white", "black": "lumi-black", "gap": None, "tileHeight": LUMI_TH},
                      "pitch": 42.43, "pitchOverrides": {"ROLI Piano 49": 36.5},
                      **({"blackOffsets": {pc: v for pc, v in S["devices"]["lumi"]["trace"]["blackOffsets"].items() if not pc.startswith("_")}}
                         if S["devices"]["lumi"].get("look") == "trace" else {})},
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
    offs = fam.get("blackOffsets", {})
    pcs = [(low + s) % 12 for s in range(count)]
    blacks_g = [place(kit[tiles["black"]], kx + (wc + offs.get(str(pc), 0)) * pitch, ky, sx, sy, f"{bpre}{i}")
                for (kind, i, wc), pc in zip(keys, pcs) if kind == "b"]
    if lumi and S["devices"]["lumi"].get("look") == "trace" and S["devices"]["lumi"]["trace"].get("keyColours") == "rainbow":
        rb = S["devices"]["lumi"]["trace"]["rainbow"]
        wpcs = [pc for (kind, _, _), pc in zip(keys, pcs) if kind == "w"]
        bpcs = [pc for (kind, _, _), pc in zip(keys, pcs) if kind == "b"]
        tint = lambda frag, idv, pc: re.sub(rf'(id="{idv}"[^>]*?fill=")[^"]+', rf'\g<1>{rb[str(pc)]}', frag)
        whites_g = [tint(g, f"{wpre}{i}", pc) for i, (g, pc) in enumerate(zip(whites_g, wpcs))]
        blacks_g = [tint(g, f"{bpre}{i}", pc) for i, (g, pc) in enumerate(zip(blacks_g, bpcs))]

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
        sub = "tiles" if name in ("seaboard-white", "seaboard-gap", "seaboard-black", "grand-white", "grand-gap", "grand-black", "lumi-white", "lumi-black") else "surrounds"
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
