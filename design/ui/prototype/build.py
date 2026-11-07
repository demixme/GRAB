#!/usr/bin/env python3
"""Build the GRAB studio prototype page (design study, not shared).

    python3 design/ui/prototype/build.py [--no-prerender]   (needs Google Chrome for the pre-render)

Writes, next to this file:
  tokens.json        the token sheet as data (single source: TOKENS below)
  grab-studio.html   the page: clickable bento prototype, hierarchy, typefaces, control states, tokens

The page is plain HTML/CSS with a small vanilla script. The window is pre-rendered once in headless
Chrome so a static read of the HTML already contains the UI (the script re-renders it live).
"""
import base64, html, json, math, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
# the device art: design/device-art/art-v2/models in the GRAB repo (art-v2/models in the design workspace)
MODELS = next(p for p in (os.path.join(ROOT, "device-art", "art-v2", "models"), os.path.join(ROOT, "art-v2", "models")) if os.path.isdir(p))
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

# ----------------------------------------------------------------------------------------------
# Tokens: the one source. CSS custom properties, the token tables and tokens.json all come from it.
# ----------------------------------------------------------------------------------------------
TOKENS = {
    "_about": "GRAB studio UI tokens, bento layout. Dark only. Hex is sRGB, alpha 0..1, sizes in px at 100% scale.",
    "colour": {
        "chrome":         ["#0E0E10", "Top bar, status bar"],
        "window":         ["#131315", "Window background, the 12 px gutters between tiles"],
        "display":        ["#0C0C0E", "The graph tile: recessed, darker than the others"],
        "panel":          ["#1B1B1E", "Every other tile, popup menus, dialogs"],
        "tileInset":      ["#151517", "A sunken row inside a tile (fixed velocity)"],
        "well":           ["#111113", "Sunken controls: combo, stepper, segmented track, CC cards"],
        "wellHover":      ["#18181B", "Sunken control, hover"],
        "raised":         ["#1C1C1F", "Button fill, small pills"],
        "raisedHover":    ["#242428", "Button hover, chip hover, menu item hover"],
        "segmentOn":      ["#2C2C31", "Pressed button; selected segment, chip, menu item"],
        "knobFace":       ["#232327", "Knob disc"],
        "knobTrack":      ["#2C2C31", "Knob and slider track"],
        "switchOff":      ["#3A3A3F", "Switch track, off"],
        "switchOffHover": ["#46464C", "Switch track, off, hover"],
        "divider":        ["#26262A", "Thin separators in bars"],
        "chromeLine":     ["#222226", "Line under the top bar / over the status bar"],
        "grid":           ["#1E1E22", "Graph grid, the graph tile's 1 px inner edge"],
        "ink":            ["#F2F2EF", "Primary text, values, focus ring, changed dot"],
        "ink2":           ["#B8B8B4", "Secondary text: row names, idle buttons, help text"],
        "label":          ["#8C8C88", "Captions, axis labels, units"],
        "disabled":       ["#5E5E5A", "Disabled controls only, never readable text"],
        "primaryFill":    ["#F2F2EF", "Save to device, when there is something to save"],
        "primaryHover":   ["#FFFFFF", "Primary button, hover"],
        "primaryPressed": ["#D6D6D2", "Primary button, pressed"],
        "primaryText":    ["#0E0E10", "Text on the primary button; switch thumb when on"],
    },
    "dimension": {
        "_about": "One hue per 5D dimension, the only saturated colours in the app. 'was' = GRAB today.",
        "strike": ["#F0302F", "#0BE4F4"],
        "glide":  ["#FF9A1F", "#73FE91"],
        "slide":  ["#A8DC3C", "#FF77FD"],
        "press":  ["#27CBDD", "#52D7BA"],
        "lift":   ["#7B6DFF", "#FBC435"],
    },
    "status": {"ok": "#A8DC3C", "warn": "#FF9A1F", "error": "#F0302F"},
    "type": {
        "_about": "One family, four sizes, five roles. Sentence case everywhere; capitals only in acronyms and product names. Tabular figures for every number. UI strings stay ASCII.",
        "family": "Red Hat Display (SIL OFL, Google Fonts); fallback system-ui",
        "readout": {"weight": 500, "size": 20, "line": 24, "use": "Knob readouts in the inspector. The only large numbers. (The name GRAB is in the window title bar, not the window.)"},
        "title":   {"weight": 600, "size": 13, "line": 18, "use": "Tile titles, the device name, the selected dimension's name, dialog titles."},
        "body":    {"weight": "400 / 500", "size": 13, "line": 18, "use": "Row names, buttons, menus, legend chips, descriptions."},
        "control": {"weight": "400 / 500", "size": 12, "line": 16, "use": "Values inside controls: combo, segment, stepper, slider value, CC number. One step below the row name."},
        "caption": {"weight": 400, "size": 11, "line": 14, "use": "Device facts, change counts, units, axis labels, knob captions, CC card names, status bar."},
    },
    "space": {"unit": 4, "scale": [4, 8, 12, 16, 20, 24], "gutter": 12, "tilePad": [12, 16], "barInset": 28,
              "rowHeight": 32, "rowGap": 8, "controlColumn": 200, "compactControl": 104},
    "radius": {"segmentInner": 7, "control": 10, "card": 10, "inset": 12, "tile": 16, "switch": 999},
    "size": {"topBar": 52, "statusBar": 28, "control": 32, "controlLarge": 36, "iconButton": 36,
             "knob": 52, "knobCompact": 48, "knobTrack": 4, "switchTrack": [34, 20], "switchThumb": 14,
             "sliderTrack": 2, "sliderThumb": 12, "changedDot": 6, "chip": 30, "deviceArt": "3 units tall, 1 unit = 32 px (96)"},
    "border": {"chromeLine": 1, "graphEdge": 1, "focusRing": 2, "focusOffset": 2},
    "shadow": {"deviceArt": {"drop": "0 6px 8px rgba(0,0,0,0.50)"}},
    "zones": {
        "_about": "Heights in px. The native title bar is outside these numbers (GRAB's setSize (920, 910) is the content).",
        "full":   {"width": 920, "height": 910, "header": 0, "device": 120, "graph": 334, "deviceUnit": 32, "settingsRow1": 204,
                   "settingsRow2": 164, "footer": 28, "inspectorWidth": 240, "gutter": 12, "scrolls": "nothing (RISE, GRAND, Seaboard Block)"},
        "narrow": {"width": 600, "height": 910, "header": 0, "device": "auto (88 drawing, controls wrap)", "graph": 272, "inspector": 116,
                   "footer": 28, "scrolls": "the area between header and footer; settings tiles fold"},
        "breakpoint": 760,
        "minWidth": 600,
    },
    "curve": {
        "stroke": {"idle": 1.5, "focus": 4},
        "glow": {"width": 0, "alpha": 0},
        "fillTopAlpha": {"focus": 0.32},
        "dimAlpha": {"rest": 0.4, "hover": 0.25},
        "handle": {"radius": 7, "ring": 2, "hitRadius": 14, "position": "knee (Strike/Press/Lift), t=0.8 on the bezier (Glide/Slide)"},
        "node": {"size": 10, "radius": 2, "stroke": 2},
        "ghost": {"dash": [3, 3], "alpha": 0.5, "stroke": 1},
        "dragRate": 0.6, "fineRate": 0.15,
        "plotPad": {"left": 62, "right": 16, "top": 16, "bottom": 46},
        "gridDivisions": 4,
    },
}

FACES = [
    ("Red Hat Display", "Geometric grotesque with crisp terminals and a taller x-height (0.50 em), so 11 px stays clear. Tabular figures. SIL OFL."),
]


def kebab(k):
    return re.sub(r"([A-Z])", r"-\1", k).lower()


def css_vars():
    out = [f"--{kebab(k)}:{v}" for k, (v, _) in TOKENS["colour"].items()]
    out += [f"--{k}:{v[0]}" for k, v in TOKENS["dimension"].items() if not k.startswith("_")]
    out += [f"--{k}:{v}" for k, v in TOKENS["status"].items()]
    out += [f"--r-{kebab(k)}:{v}px" for k, v in TOKENS["radius"].items()]
    return ";\n  ".join(out) + ";"


def tokens_json():
    t = json.loads(json.dumps(TOKENS))
    t["colour"] = {k: {"hex": v[0], "use": v[1]} for k, v in TOKENS["colour"].items()}
    t["dimension"] = {k: ({"hex": v[0], "was": v[1]} if not k.startswith("_") else v) for k, v in TOKENS["dimension"].items()}
    return t


def art_uri(slug):
    s = open(os.path.join(MODELS, slug + ".svg")).read()
    s = re.sub(r"<rect[^>]*id=\"selection-frame\"[^>]*/>", "", s)
    for gid in ("button-leds", "panel-live", "live-overlay"):
        s = s.replace(f'<g id="{gid}"', f'<g id="{gid}" display="none"')
    s = re.sub(r"<\?xml[^>]*\?>\s*|<!--.*?-->", "", s, flags=re.S)
    return "data:image/svg+xml;base64," + base64.b64encode(s.encode()).decode()


# ----------------------------------------------------------------------------------------------
# Static sections
# ----------------------------------------------------------------------------------------------
E = html.escape
HUES = {k: v[0] for k, v in TOKENS["dimension"].items() if not k.startswith("_")}
ICON = {
    "down":  '<svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><path d="M2 3.5l3 3 3-3" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    "minus": '<svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><path d="M2 5h6" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>',
    "plus":  '<svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><path d="M2 5h6M5 2v6" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>',
    "tools": '<svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true"><path d="M2.5 4.5h11M2.5 11.5h11" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><circle cx="6" cy="4.5" r="1.8" fill="var(--chrome)" stroke="currentColor" stroke-width="1.5"/><circle cx="10" cy="11.5" r="1.8" fill="var(--chrome)" stroke="currentColor" stroke-width="1.5"/></svg>',
}


def knob_svg(value, lo, hi, hue, size=60, state="", bipolar=False):
    f = (value - lo) / (hi - lo) if hi > lo else 0
    def pol(deg, r):
        a = math.radians(deg); return 50 + r * math.cos(a), 50 + r * math.sin(a)
    def arc(a0, a1, r):
        if abs(a1 - a0) < 0.01: return ""
        (x0, y0), (x1, y1) = pol(a0, r), pol(a1, r)
        return f"M{x0:.2f} {y0:.2f}A{r} {r} 0 {1 if abs(a1 - a0) > 180 else 0} {1 if a1 > a0 else 0} {x1:.2f} {y1:.2f}"
    a = 135 + 270 * f
    a0 = 270 if bipolar else 135
    dis = state == "disabled"
    face = "var(--raised-hover)" if state in ("hover", "drag") else "var(--knob-face)"
    (px0, py0), (px1, py1) = pol(a, 14), pol(a, 30)
    return (f'<svg class="knob-svg" width="{size}" height="{size}" viewBox="0 0 100 100" aria-hidden="true">'
            f'<path d="{arc(135, 405, 42)}" fill="none" stroke="var(--knob-track)" stroke-width="{TOKENS["size"]["knobTrack"] * 100 / size:.2f}" stroke-linecap="round"/>'
            + ("" if dis else f'<path d="{arc(a0, a, 42)}" fill="none" stroke="{hue}" stroke-width="{TOKENS["size"]["knobTrack"] * 100 / size:.2f}" stroke-linecap="round"/>') +
            f'<circle class="kface" cx="50" cy="50" r="32" fill="{face}"/>'
            + ("" if dis else f'<path d="M{px0:.2f} {py0:.2f}L{px1:.2f} {py1:.2f}" stroke="var(--ink)" stroke-width="{200 / size:.2f}" stroke-linecap="round"/>') +
            '</svg>')


def knob_block(caption, value, lo, hi, hue, state="", bipolar=False):
    dis = state == "disabled"
    shown = "-" if dis else (f"+{value}" if bipolar and value > 0 else str(value))
    return (f'<div class="knob{" is-disabled" if dis else ""}">{knob_svg(value, lo, hi, hue, 60, state, bipolar)}'
            f'<span class="t-readout knob-v">{shown}</span><span class="t-caption">{caption}</span></div>')


def sample(caption, inner, wide=False, tile=False):
    cls = "smp" + (" wide" if wide else "")
    st = "smp-stage" + (" on-tile" if tile else "")
    return f'<figure class="{cls}"><div class="{st}">{inner}</div><figcaption class="t-caption">{E(caption)}</figcaption></figure>'


def stepper(v, unit="", state="", w104=False):
    u = f' <span class="unit">{E(unit)}</span>' if unit else ""
    return (f'<div class="st{" w104" if w104 else ""}{" is-" + state if state else ""}"><button class="sb" tabindex="-1" aria-label="Down">{ICON["minus"]}</button>'
            f'<span class="st-v">{v}{u}</span><button class="sb" tabindex="-1" aria-label="Up">{ICON["plus"]}</button></div>')


def seg(opts, on, state=""):
    return ('<div class="sg" role="radiogroup">' + "".join(
        f'<button type="button" role="radio" aria-checked="{"true" if i == on else "false"}"{" class=is-hover" if state == "hover" and i == (on + 1) % len(opts) else ""}>{E(o)}</button>'
        for i, o in enumerate(opts)) + "</div>")


def combo(text, state=""):
    return f'<span class="combo{" is-" + state if state else ""}"><span class="cv">{E(text)}</span>{ICON["down"]}</span>'


def switch(on, state="", hue=None):
    st = f' style="--sw-on:{hue}"' if hue else ""
    return f'<span class="sw{" is-" + state if state else ""}" role="switch" aria-checked="{"true" if on else "false"}"{st}></span>'


def slider(v, lo, hi, unit, state=""):
    f = (v - lo) / (hi - lo)
    return (f'<div class="slw"><div class="sl{" is-" + state if state else ""}"><span class="tr"></span><span class="fi" style="width:calc((100% - 12px) * {f:.3f})"></span>'
            f'<span class="th" style="left:calc(6px + (100% - 12px) * {f:.3f})"></span></div><span class="val">{v} <span class="unit">{unit}</span></span></div>')


def row(label, ctl, changed=False, dots=(), compact=False):
    d = "".join(f'<span class="hd" style="background:{HUES[x]}"></span>' for x in dots)
    d = f'<span class="hdots">{d}</span>' if d else ""
    return (f'<div class="row{" compact" if compact else ""}{" changed" if changed else ""}">'
            f'<span class="lbl"><span class="t">{E(label)}</span>{d}<span class="cd"></span></span><div class="ctl">{ctl}</div></div>')


def chip(name, val, state="", changed=False):
    cls = {"sel": " is-sel", "hover": " is-hover", "dim": " is-dim"}.get(state, "")
    return (f'<span class="chip{cls}{" changed" if changed else ""}" style="--hue:{HUES[name.lower()]}"><span class="dot"></span>'
            f'<span class="nm">{name}</span><span class="v">{val}</span><span class="cd"></span></span>')


def card(name, v, state="", changed=False):
    return (f'<div class="card{" is-" + state if state else ""}{" changed" if changed else ""}"><span class="cap">{E(name)}<span class="cd"></span></span>'
            f'<span class="vrow"><span class="v">{v}</span><span class="cb"><button class="sb" tabindex="-1" aria-label="Down">{ICON["minus"]}</button>'
            f'<button class="sb" tabindex="-1" aria-label="Up">{ICON["plus"]}</button></span></span></div>')


def tile_head(title, right="", collapsible=None):
    if collapsible is None:
        return f'<div class="th"><h3>{E(title)}</h3><span class="cnt">{E(right)}</span></div>'
    return (f'<button class="th tbtn" aria-expanded="{"true" if collapsible else "false"}"><h3>{E(title)}</h3>'
            f'<span class="cnt">{E(right)}</span><span class="chev">{ICON["down"]}</span></button>')


def handle_svg(kind):
    hue = HUES["strike"]
    b = {
        "focus": f'<circle cx="40" cy="30" r="7" fill="{hue}" stroke="var(--ink)" stroke-width="2"/>',
        "drag": (f'<circle cx="40" cy="30" r="12" fill="{hue}" opacity="0.18"/><circle cx="40" cy="30" r="7" fill="{hue}" stroke="var(--ink)" stroke-width="2"/>'
                 f'<rect x="52" y="10" width="26" height="18" rx="6" fill="var(--panel)"/><text x="65" y="23" text-anchor="middle" class="svg-label" fill="var(--ink)">72</text>'),
        "node": f'<rect x="35" y="25" width="10" height="10" rx="2" fill="var(--display)" stroke="{hue}" stroke-width="2"/>',
        "other": f'<path d="M4 54 Q30 20 92 14" fill="none" stroke="{HUES["press"]}" stroke-width="1.5" opacity="0.4"/>',
        "changed": (f'<path d="M4 50 Q30 50 40 30" fill="none" stroke="var(--ink2)" stroke-width="1" stroke-dasharray="3 3" opacity="0.5"/>'
                    f'<path d="M4 54 Q24 30 40 22" fill="none" stroke="{hue}" stroke-width="2.5"/>'
                    f'<circle cx="40" cy="22" r="7" fill="{hue}" stroke="var(--ink)" stroke-width="2"/>'),
    }[kind]
    return f'<svg width="96" height="60" viewBox="0 0 96 60" aria-hidden="true">{b}</svg>'


def states_section():
    S = HUES["strike"]
    groups = [
        ("Button", "Raised fill, body 13 in ink2. Read from device, Revert, menu actions.", [
            sample("Normal", '<span class="btn">Revert</span>'), sample("Hover", '<span class="btn is-hover">Revert</span>'),
            sample("Pressed", '<span class="btn is-active">Revert</span>'), sample("Focus", '<span class="btn is-focus">Revert</span>'),
            sample("Disabled", '<span class="btn is-disabled">Revert</span>')]),
        ("Primary button", "Save to device. Primary only while something is unsaved; otherwise it rests as a plain button.", [
            sample("Unsaved changes", '<span class="btn lg primary">Save to device</span>'),
            sample("Hover", '<span class="btn lg primary is-hover">Save to device</span>'),
            sample("Pressed", '<span class="btn lg primary is-active">Save to device</span>'),
            sample("Nothing to save", '<span class="btn lg is-disabled">Save to device</span>')]),
        ("Outline button", "Small actions inside a tile (Reset in the inspector).", [
            sample("Normal", '<span class="btn sm outline">Reset</span>', tile=True),
            sample("Hover", '<span class="btn sm outline is-hover">Reset</span>', tile=True),
            sample("Disabled", '<span class="btn sm outline is-disabled">Reset</span>', tile=True)]),
        ("Icon button", "Transparent until hovered. 36 x 36, 16 px icon at 1.5 stroke.", [
            sample("Normal", f'<span class="ibtn">{ICON["tools"]}</span>'), sample("Hover", f'<span class="ibtn is-hover">{ICON["tools"]}</span>'),
            sample("Open", f'<span class="ibtn is-active">{ICON["tools"]}</span>'), sample("Focus", f'<span class="ibtn is-focus">{ICON["tools"]}</span>')]),
        ("Segmented control", "For two or three choices (MIDI mode, MPE zone). Replaces a dropdown.", [
            sample("Normal", seg(["MPE", "Multi", "Single"], 0), True, True),
            sample("Hover on another", seg(["MPE", "Multi", "Single"], 0, "hover"), True, True)]),
        ("Combo box", "Four or more choices. Sunken well, 10 px chevron; opens a menu.", [
            sample("Normal", combo("Relative bipolar"), True, True), sample("Hover", combo("Relative bipolar", "hover"), True, True),
            sample("Focus", combo("Relative bipolar", "focus"), True, True),
            sample("Open", '<div class="menu static"><button class="mi">Absolute</button><button class="mi">Relative unipolar</button>'
                           '<button class="mi on">Relative bipolar</button></div>')]),
        ("Stepper", "Small ranges and CC numbers. Drag the number to scrub, or use - and +. Units in caption grey.", [
            sample("Normal", stepper(0, "st", w104=True), tile=True), sample("Hover", stepper(0, "st", "hover", True), tile=True),
            sample("Focus", stepper(0, "st", "focus", True), tile=True), sample("With range", stepper(15, "ch 2-16"), True, True)]),
        ("Switch", "Replaces tick boxes. On = ink, or the dimension hue in the inspector.", [
            sample("Off", switch(False), tile=True), sample("Off, hover", switch(False, "hover"), tile=True),
            sample("On", switch(True), tile=True), sample("On, in hue", switch(True, hue=S), tile=True),
            sample("Disabled", switch(False, "disabled"), tile=True)]),
        ("Slider", "Wide ranges (pitch bend range). Value and unit on the right.", [
            sample("Normal", slider(48, 1, 96, "st"), True, True), sample("Hover", slider(48, 1, 96, "st", "hover"), True, True),
            sample("Dragging", slider(60, 1, 96, "st", "drag"), True, True)]),
        ("Knob", "Inspector only: min, sensitivity, max. Vertical drag, Ctrl or Cmd for fine steps.", [
            sample("Normal", knob_block("Sensitivity", 64, 0, 127, S), tile=True),
            sample("Hover", knob_block("Sensitivity", 64, 0, 127, S, "hover"), tile=True),
            sample("Dragging", knob_block("Sensitivity", 90, 0, 127, S, "drag"), tile=True),
            sample("Not on this device", knob_block("Max", 127, 0, 127, HUES["glide"], "disabled"), tile=True)]),
        ("Settings row", "Name on the tile's left margin, control in a 200 px column. Dimension dots and the unsaved dot follow the name.", [
            sample("Normal", '<div class="rows demo">' + row("Slide CC", stepper(74), dots=("slide",)) + "</div>", True, True),
            sample("Changed, not saved", '<div class="rows demo">' + row("Slide CC", stepper(75), True, ("slide",)) + "</div>", True, True)]),
        ("CC card", "Six CC numbers as cards instead of six rows. Name in caption, number in body 500.", [
            sample("Normal", '<div class="cards demo">' + card("Left fader", 107) + "</div>", tile=True),
            sample("Hover", '<div class="cards demo">' + card("Left fader", 107, "hover") + "</div>", tile=True),
            sample("Changed", '<div class="cards demo">' + card("Left fader", 108, changed=True) + "</div>", tile=True)]),
        ("Tile header", "Title on the left, what changed inside on the right. At 600 wide the header folds the tile.", [
            sample("At rest", '<div class="tdemo">' + tile_head("MIDI output") + "</div>", True, True),
            sample("With changes", '<div class="tdemo">' + tile_head("MIDI output", "1 changed") + "</div>", True, True),
            sample("Folded (narrow)", '<div class="tdemo">' + tile_head("Keyboard", "Octave 0, transpose 0 st", False) + "</div>", True, True),
            sample("Open (narrow)", '<div class="tdemo">' + tile_head("Keyboard", "", True) + "</div>", True, True)]),
        ("Graph legend chip", "One per dimension. Click to select; hover brings the curve forward.", [
            sample("Idle", '<div class="chips static">' + chip("Strike", 64) + "</div>"), sample("Hover", '<div class="chips static">' + chip("Strike", 64, "hover") + "</div>"),
            sample("Selected", '<div class="chips static">' + chip("Strike", 64, "sel") + "</div>"),
            sample("Changed", '<div class="chips static">' + chip("Strike", 72, "sel", True) + "</div>"),
            sample("Dimmed", '<div class="chips static">' + chip("Strike", 64, "dim") + "</div>")]),
        ("Curve", "Only the selected curve gets fill, handle and end nodes. The others are thin reference lines.", [
            sample("Other curve", f'<div class="disp-sw">{handle_svg("other")}</div>'),
            sample("Selected handle", f'<div class="disp-sw">{handle_svg("focus")}</div>'),
            sample("Dragging", f'<div class="disp-sw">{handle_svg("drag")}</div>'),
            sample("Min / max node", f'<div class="disp-sw">{handle_svg("node")}</div>'),
            sample("Changed: ghost of device value", f'<div class="disp-sw">{handle_svg("changed")}</div>')]),
    ]
    return "\n".join(f'<div class="sgroup"><div class="sg-h"><h3>{E(t)}</h3><p>{E(n)}</p></div><div class="sg-items">{"".join(i)}</div></div>'
                     for t, n, i in groups)


def colour_tables():
    sw = lambda h: f'<span class="sw-chip" style="background:{h}"></span>'
    rows = "".join(f'<tr><td>{sw(v)}<code>{k}</code></td><td><code>{v}</code></td><td>{E(u)}</td></tr>' for k, (v, u) in TOKENS["colour"].items())
    dims = "".join(f'<tr><td>{sw(v[0])}<code>{k}</code></td><td><code>{v[0]}</code></td><td>{sw(v[1])}<code>{v[1]}</code></td></tr>'
                   for k, v in TOKENS["dimension"].items() if not k.startswith("_"))
    st = "".join(f'<tr><td>{sw(v)}<code>{k}</code></td><td><code>{v}</code></td></tr>' for k, v in TOKENS["status"].items())
    return rows, dims, st


def type_table():
    samples = {"readout": "127", "title": "MIDI output", "body": "Pitch bend range", "control": "Relative bipolar", "caption": "Firmware 1.1.8"}
    return "".join(f'<tr><td><span class="t-{k}">{samples[k]}</span></td><td><code>{k}</code></td><td>{v["weight"]}</td>'
                   f'<td>{v["size"]} / {v["line"]}</td><td>{E(v["use"])}</td></tr>' for k, v in TOKENS["type"].items() if k in samples)


def kv_table(d):
    out = []
    for k, v in d.items():
        if k.startswith("_"): continue
        if isinstance(v, dict): v = ", ".join(f"{a} {b}" for a, b in v.items())
        elif isinstance(v, list): v = " x ".join(map(str, v)) if len(v) == 2 else ", ".join(map(str, v))
        out.append(f"<tr><td><code>{k}</code></td><td><code>{E(str(v))}</code></td></tr>")
    return "".join(out)


def zone_table():
    f, n = TOKENS["zones"]["full"], TOKENS["zones"]["narrow"]
    rows = [("Window (content)", f"{f['width']} x {f['height']}", f"{n['width']} x {n['height']}"),
            ("Header (top bar)", f["header"], n["header"]),
            ("Device tile", f["device"], n["device"]),
            ("Graph tile + inspector", f"{f['graph']}, inspector {f['inspectorWidth']} wide beside it", f"graph {n['graph']}, inspector strip {n['inspector']} below"),
            ("Settings tiles", f"row 1: {f['settingsRow1']}, row 2: {f['settingsRow2']}", "one column, each tile folds"),
            ("Footer (status bar)", f["footer"], n["footer"]),
            ("Scrolls", f["scrolls"], n["scrolls"])]
    return "".join(f"<tr><td>{a}</td><td>{b}</td><td>{c}</td></tr>" for a, b, c in rows)


def curve_table():
    c = TOKENS["curve"]
    rows = [
        ("Plot", f"Unit square: x = input 0-127, y = output 0-127. Padding L{c['plotPad']['left']} R{c['plotPad']['right']} T{c['plotPad']['top']} B{c['plotPad']['bottom']}."),
        ("Grid", f"{c['gridDivisions']} x {c['gridDivisions']} lines in grid; ticks 0 / 64 / 127 and the axis names in caption."),
        ("Curve maths", "CurveComponent::curvePath, unchanged. One shared function for the overlay."),
        ("Focus", "focus = the hovered dimension, else the selected one. Drawn last, in front."),
        ("Focus curve", f"Fill gradient {c['fillTopAlpha']['focus']} to 0, stroke {c['stroke']['focus']} px with round ends and no glow: the same stroke as the knob arcs (size.knobTrack), so curve and dials read as one family. Sensitivity handle and end nodes on top."),
        ("Other curves", f"Stroke {c['stroke']['idle']} px, no fill, no handle. {c['dimAlpha']['rest']} alpha at rest, {c['dimAlpha']['hover']} while another is hovered. Hovering one makes it the focus, so its handle appears and can be dragged at once."),
        ("Handle", f"Circle r {c['handle']['radius']}, hue fill, {c['handle']['ring']} px ink ring, hit radius {c['handle']['hitRadius']}. Sits at the {c['handle']['position']}."),
        ("Min / max nodes", f"Only where the device has min/max fields (RISE: Strike, Press, Lift). {c['node']['size']} px square, r {c['node']['radius']}, at x = 0 and x = 1."),
        ("Drag", f"Vertical, {c['dragRate']} per px; Ctrl or Cmd held: {c['fineRate']} per px. Value pill beside the handle. Writes through onItemChanged / setSensitivity."),
        ("Reset", "Double-click or right-click a handle, node, knob or setting: back to the value read from the device."),
        ("Keyboard", "Handles and nodes take focus with Tab. Up/Down = 1, Shift = 10."),
        ("Unsaved", f"The chip and tile header show it; the focus curve shows the device value as a dashed ghost ({c['ghost']['stroke']} px, ink2 at {c['ghost']['alpha']})."),
        ("Fixed velocity", "Inspector switch (Strike). On: Strike draws flat at the fixed value, its handle and middle knob set the value."),
        ("Device list", "Chips come from the device's Sensitivity fields (fieldShown), never a fixed five. GRAND = Glide only today."),
        ("Live input", "While a note is held, the focus curve shows where the input lands: a dot on the curve, dashed guides to both axes, the output value. Strike uses the note-on force, Press the pressure, Glide the sideways rock, Slide the finger position, Lift the last release. Fed by the performance MIDI GRAB already reads for live sync."),
    ]
    return "".join(f"<tr><td>{E(a)}</td><td>{E(b)}</td></tr>" for a, b in rows)


def faces_html():
    return "".join(f'''<button type="button" class="face" data-font="{f}" aria-pressed="{"true" if i == 0 else "false"}" style="--fam:'{f}', system-ui, sans-serif">
  <span class="fn"><b>{f}</b><span>SIL OFL</span></span>
  <span class="sp-r">127&nbsp;&nbsp;64&nbsp;&nbsp;+12</span>
  <span class="sp-t">MIDI output</span>
  <span class="sp-b">Pitch bend range, channel pressure (last played)</span>
  <span class="sp-c">Connected over USB&nbsp;&nbsp;Firmware 1.1.8&nbsp;&nbsp;Battery 71%</span>
  <p class="facts">{E(d)}</p></button>''' for i, (f, d) in enumerate(FACES))


def hier_html():
    cols = {'ink': 'var(--ink)', 'ink2': 'var(--ink2)', 'label': 'var(--label)', 'hue': 'var(--strike)', 'primary': 'var(--primary-fill)'}
    H = [(1, 'Window', '920 x 910', '', 'ink', True),
         (2, 'Header', 'the preset, and getting changes onto the device', '', 'ink', True),
         (3, 'Preset', 'MPE + Edited pill', 'Body 13 + Caption 11', 'ink+ink2'),
         (3, 'Save state', '3 unsaved changes, Revert, Save to device', 'Body 13', 'ink2+primary'),
         (2, 'Device tile', 'what is connected', '', 'ink', True),
         (3, 'Device name', 'Seaboard RISE 25 (opens the device list)', 'Title 13', 'ink'),
         (3, 'Device facts', 'connection dot + USB, battery icon + 71% (firmware in the menu and hover help)', 'Caption 11', 'ink2'),
         (3, 'Device actions', 'Read from device, Autosave', 'Body 13', 'ink2'),
         (2, 'Graph tile', 'how the keys respond', '', 'ink', True),
         (3, 'Legend chips', 'Strike 64', 'Body 13 + Caption 11', 'ink2+hue'),
         (3, 'Axes', '0 64 127, Strike force, Velocity', 'Caption 11', 'label'),
         (2, 'Inspector tile', 'the selected dimension', '', 'ink', True),
         (3, 'Title', 'Strike', 'Title 13', 'ink'),
         (3, 'Description', 'How hard you strike...', 'Body 13', 'ink2'),
         (3, 'Knobs', '64 over Sensitivity', 'Readout 20 + Caption 11', 'ink+label'),
         (2, 'Settings tiles', 'grouped by job', '', 'ink', True),
         (3, 'Tile header', 'MIDI output, 1 changed', 'Title 13 + Caption 11', 'ink+label'),
         (3, 'Row name', 'Pitch bend range (ink when changed)', 'Body 13', 'ink2'),
         (3, 'Row value', 'MPE, 48 st, Last played', 'Body 13 + Caption 11 unit', 'ink+label'),
         (3, 'CC card', 'Left fader 107', 'Caption 11 + Body 13 / 500', 'label+ink'),
         (2, 'Status bar', 'help', '', 'ink', True),
         (3, 'Help, status, version', 'footnote: Saved 3 settings, device confirms.', 'Caption 11', 'label')]
    out = []
    for h in H:
        d, el, eg, role, col = h[:5]; zone = len(h) > 5
        sw = ''.join(f'<i style="background:{cols[c]}"></i>' for c in col.split('+'))
        out.append(f'<div class="lv d{d}"><span><span class="{"zone" if zone else ""}">{E(el)}</span> <span class="eg">{E(eg)}</span></span>'
                   f'<span class="role">{E(role)}</span><span class="col">{sw}{col.replace("+", " / ")}</span></div>')
    return "".join(out)


# ----------------------------------------------------------------------------------------------
def build(prerender=True):
    tpl = open(os.path.join(HERE, "template.html")).read()
    rows, dims, st = colour_tables()
    tj = tokens_json()
    art = {"rise": art_uri("rise-25"), "grand": art_uri("grand-stage-61"), "block": art_uri("seaboard-block-24"),
           "lightpad": art_uri("lightpad-block"), "touch": art_uri("control-touch"), "loop": art_uri("control-loop"), "live": art_uri("control-live")}
    fills = {
        "TOKENS_CSS": css_vars(), "TOKENS_JSON": json.dumps(tj, indent=1), "TOKENS_JSON_PRE": E(json.dumps(tj, indent=1)),
        "ART_JSON": json.dumps(art), "STATES": states_section(), "HIER": hier_html(), "FACES": faces_html(),
        "COLOUR_ROWS": rows, "DIM_ROWS": dims, "STATUS_ROWS": st, "TYPE_ROWS": type_table(),
        "SPACE_ROWS": kv_table(TOKENS["space"]), "RADIUS_ROWS": kv_table(TOKENS["radius"]), "SIZE_ROWS": kv_table(TOKENS["size"]),
        "ZONE_ROWS": zone_table(), "CURVE_ROWS": curve_table(),
        "RADIUS_SAMPLES": "".join(f'<div class="rs"><span style="border-radius:{min(v, 20)}px"></span><code>{k} {v if v < 999 else "pill"}</code></div>' for k, v in TOKENS["radius"].items()),
        "SPACE_SAMPLES": "".join(f'<div class="ss"><span style="width:{v}px"></span><code>{v}</code></div>' for v in TOKENS["space"]["scale"]),
    }
    for k, v in fills.items():
        tpl = tpl.replace("{{" + k + "}}", v)
    left = re.findall(r"\{\{[A-Z_]+\}\}", tpl)
    if left: sys.exit(f"unfilled placeholders: {left}")
    out = os.path.join(HERE, "grab-studio.html")
    open(out, "w").write(tpl)
    json.dump(tj, open(os.path.join(HERE, "tokens.json"), "w"), indent=2)

    if prerender:
        with tempfile.TemporaryDirectory() as tmp:
            page = os.path.join(tmp, "p.html")
            open(page, "w").write("<!doctype html><html><head><meta charset=utf-8></head><body>" + tpl + "</body></html>")
            r = subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--window-size=1280,1400", "--virtual-time-budget=4000",
                                "--dump-dom", "file://" + page], capture_output=True, text=True, timeout=90)
            m = re.search(r'<div id="proto"[^>]*>(.*?)</div><!--/proto-->', r.stdout, re.S)
            if not m: sys.exit("prerender failed: no #proto in dumped DOM")
            tpl = tpl.replace('<div id="proto"></div><!--/proto-->', '<div id="proto">' + m.group(1) + '</div><!--/proto-->')
        open(out, "w").write(tpl)
    print("wrote", out, f"({os.path.getsize(out) // 1024} KB)")


if __name__ == "__main__":
    build(prerender="--no-prerender" not in sys.argv)
