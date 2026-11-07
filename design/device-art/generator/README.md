# Device-art generator

```sh
python3 design/device-art/generator/kit.py   # writes ../art-v2/kit + ../art-v2/models, then validates
```

| File | What it is | Who edits it |
|-|-|-|
| `style.json` | All visual decisions: materials, light, colours, radii, key/LED looks, Control Block accents | designer |
| `build.py` | Drawing library: geometry (mirrors `KeyboardView.h` + photo measurements), shapes, Control Block icons | code |
| `kit.py` | Cuts the devices into tiles / 3-slice surrounds / fixed parts, writes `manifest.json`, assembles the 12 models | code |
| `validate.py` | Checks any SVG (generated, hand-edited or Figma-exported) against GRAB's contract | run it |

`python3 validate.py some.svg` checks a single file. It rejects anything JUCE's Drawable can't render
(filters, masks, CSS, text), checks the layer-id contract in DESIGN-GUIDE.md, and checks that
`live-overlay` exists, is empty and is topmost. So Figma effects like drop shadows or blur can't slip
through.

Control Block icons are the `ICONS` table in `build.py`. Each icon is a path in units of the button
cap's radius, centred on 0,0, and stroked at 0.075 × radius like the printed legends.

Pure standard-library Python 3, no dependencies.
