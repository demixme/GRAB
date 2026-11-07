// KeyboardView — the ROLI device surface drawn at the top of the window. Our own vector art
// (not ROLI's device renders).
//
// BLOCKS are modular: however the user snaps them, the SDK reports each block's place in a shared
// logical-unit grid (getBlockAreaWithinLayout + getRotation), so we draw the real X/Y arrangement
// as tiles — a Seaboard keybed (6x3 units), a Lightpad pad-grid (2x2), a Control block (2x1), a
// LUMI/Piano flat keybed — each with live 5D-touch rings (Strike disc + Press ring) and, in Chord
// mode, half-opacity ghost notes. RISE/GRAND aren't modular; they draw as a single keybed tile.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "DeviceArtData.h"   // embedded device-art SVGs (CMake: GRABDeviceArt)

#include <atomic>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

class KeyboardView  : public juce::Component,
                      private juce::Timer
{
public:
    enum class Family { block, rise, grand, lumi, lightpad, control };
    // Control-block subtype - picks the LED colour + per-button icons (all are the same 10-button
    // hardware; only the printed icons / default functions differ). generic = Developer Control.
    enum class Control { generic, live, loop, touch, developer };

    // One drawn block, positioned in the shared logical-unit grid (origin = master's top-left).
    struct Tile
    {
        juce::uint64 uid = 0;        // block UID (the touch source); 0 = the single RISE/GRAND keybed
        Family family = Family::rise;
        float ux = 0, uy = 0;        // top-left in logical units
        float uw = 6, uh = 3;        // footprint in logical units (already rotated by the SDK)
        int   rotation = 0;          // art orientation, 90-degree steps clockwise (0..3)
        int   octaves = 2;           // keybed octaves (keyboards only)
        // Exact keybed range for the legacy RISE/GRAND keybed, so a 49-key board draws all 49 keys
        // (C2..C6 incl. the top C) rather than whole octaves. keyCount == 0 => derive from `octaves`.
        int   lowNote = 48;          // MIDI note of the leftmost key (a C for the C-to-C boards)
        int   keyCount = 0;          // number of keys to draw; 0 = octaves * 12 whole octaves
        int   grid = 15;             // LED grid resolution (lightpad only)
        Control control = Control::generic;   // control-block subtype (Live/Loop/Touch art)
    };

    KeyboardView() { buildLegacyLocked(); computeBoundsLocked(); }

    // --- Legacy single-keybed setters (RISE/GRAND and the dev previews) --------------------------
    // These build a one-tile layout; the real multi-block arrangement comes through setLayout().
    // They always rebuild (not guarded on value) so a call also leaves any prior Block layout mode.
    void setOctaves (int o)   { octaves = juce::jlimit (1, 8, o); legacyKeys = 0; rebuildLegacy(); }
    void setFamily  (Family f){ family = f; rebuildLegacy(); }
    void setBlocks  (int)     { /* retained for call sites; real chaining now comes via setLayout() */ }
    int  getOctaves() const   { return octaves; }

    // Device art V2 toggle (Advanced -> New device art): draw the designer's SVG kit instead of the
    // hand-drawn vector art, where a family has been wired in. Phase 1 covers Control blocks only;
    // families without V2 art fall back to the drawn art automatically.
    void setUseDeviceArtV2 (bool b)
    {
        if (useDeviceArtV2 == b) return;
        useDeviceArtV2 = b;
        if (usingLayout) repaint();   // DNA tiles keep their fixed footprints (already match the art)
        else rebuildLegacy();         // legacy keybed: re-derive the footprint (GRAND aspect changes)
    }

    // Exact keybed range: draw `keyCount` keys from `lowNote` (a C for the C-to-C boards). Use this
    // instead of setOctaves for RISE/GRAND so the real key count is drawn (e.g. a 49-key RISE 2 =
    // C2..C6, with its top C getting its own key) and the touch ring lands on the true key centres.
    void setKeyboardRange (int lowNote, int keyCount)
    {
        legacyLow  = lowNote;
        legacyKeys = juce::jmax (1, keyCount);
        octaves    = juce::jlimit (1, 8, (legacyKeys + 11) / 12);
        rebuildLegacy();
    }

    // nx (0..1 across the keybed's white-key span) for a MIDI note on the current legacy keybed,
    // matching drawKeybed's geometry exactly - so the RISE/GRAND touch ring sits on the pressed key.
    // bendSemitones nudges it along for Glide (approximate: the average white-key spacing).
    float legacyNoteToX (int note, float bendSemitones = 0.0f) const
    {
        const int kc = legacyKeys > 0 ? legacyKeys : octaves * 12;
        const int lo = legacyKeys > 0 ? legacyLow  : defaultLowC (octaves);
        float x = keybedNoteX (note, lo, kc);
        if (bendSemitones != 0.0f)
        {
            const int totalW = keybedWhiteCount (lo, kc);
            x += bendSemitones * (7.0f / 12.0f) / (float) juce::jmax (1, totalW);
        }
        return juce::jlimit (0.0f, 1.0f, x);
    }

    // The real DNA arrangement: tiles from the device topology. Empty falls back to the legacy keybed.
    void setLayout (std::vector<Tile> t)
    {
        {
            const juce::ScopedLock sl (touchLock);
            tiles = std::move (t);
            usingLayout = ! tiles.empty();
            if (! usingLayout) buildLegacyLocked();
            computeBoundsLocked();
            dirty = true;
        }
        repaint();   // called on the message thread (device connect / selection)
    }

    // The block(s) of the currently-selected device, highlighted in a multi-block arrangement (the
    // rest dim, like ROLI Dashboard). No effect when every drawn tile belongs to the selection.
    void setSelected (std::vector<juce::uint64> s)
    {
        const juce::ScopedLock sl (touchLock);
        if (selectedUids != s) { selectedUids = std::move (s); dirty = true; }
    }

    // Active chord-mode intervals (semitones above the played note), empty = Chord Off. Each live
    // touch on a keybed then also draws a ghost marker at these offsets (the auto-played notes).
    void setChord (std::vector<int> offsets)
    {
        const juce::ScopedLock sl (touchLock);
        if (chordOffsets != offsets) { chordOffsets = std::move (offsets); dirty = true; }
    }

    // LUMI / Piano key lighting, mirroring the device's active-mode Color Scheme (semantics from
    // ROLI support): 0 Pro (root in a highlight, scale white), 1 User (root + scale in your Global/
    // Root colours), 2 Piano (plain white/black), 3 Stage (dim rainbow), 4 Rainbow (in-key rainbow).
    // `scale` is 0..18 (device order), `root` 0..11 (C..B). Rainbow hues are an approximation.
    void setColourScheme (int scheme, int scale, int root, juce::uint32 inScale, juce::uint32 rootColour,
                          int brightnessPct = 100)
    {
        if (colourScheme == scheme && scaleId == scale && rootNote == ((root % 12) + 12) % 12
            && inScaleArgb == inScale && rootArgb == rootColour && brightness == brightnessPct)
            return;
        colourScheme = scheme; scaleId = scale; rootNote = ((root % 12) + 12) % 12;
        inScaleArgb = inScale; rootArgb = rootColour; brightness = juce::jlimit (0, 100, brightnessPct);
        repaint();
    }

    // --- Live 5D-touch markers (Equator style): Strike -> solid disc (fixed for the note's life),
    // Press -> ring breathing around it. Glide/Slide are the disc's x/y position; Lift isn't shown.
    // setTouch may be called from the MIDI thread; a 60 Hz timer eases values and coalesces repaints.
    void startTouchDisplay()
    {
        { const juce::ScopedLock sl (touchLock); touches.clear(); }
        if (! isTimerRunning()) startTimerHz (60);
    }
    void stopTouchDisplay()
    {
        stopTimer();
        { const juce::ScopedLock sl (touchLock); touches.clear(); }
        repaint();
    }

    // nx/ny are 0..1 within the tile's own (unrotated) surface. tileUid picks the tile.
    void setTouch (juce::uint64 tileUid, int fingerId, float nx, float ny, float z, float strike, bool end)
    {
        const juce::ScopedLock sl (touchLock);
        const Key key { tileUid, fingerId };
        if (end) { touches.erase (key); dirty = true; return; }
        const double now = juce::Time::getMillisecondCounterHiRes();
        auto it = touches.find (key);
        if (it == touches.end())
            touches[key] = { nx, ny, z, strike, nx, ny, z, now };   // new finger: displayed snaps to first sample
        else
        {
            auto& t = it->second;
            t.x = nx; t.y = ny; t.z = z; t.strike = strike; t.lastMs = now;
        }
        dirty = true;
    }

    int preferredWidth() const { return 600; }   // the view is laid out full-width; content self-centres

    // Clicking a block in the drawn arrangement selects it (uid of the clicked tile), the way you'd
    // click a block in ROLI Dashboard. uid 0 (the single RISE/GRAND keybed) never fires.
    std::function<void (juce::uint64)> onTileClicked;

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto uid = tileUidAt (e.position);
        if (uid != 0 && onTileClicked != nullptr) onTileClicked (uid);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (tileUidAt (e.position) != 0 ? juce::MouseCursor::PointingHandCursor
                                                     : juce::MouseCursor::NormalCursor);
    }

    void paint (juce::Graphics& g) override
    {
        std::vector<Tile> ts;
        std::map<Key, Live> liveTouches;
        std::vector<int> chord;
        std::vector<juce::uint64> sel;
        float bw, bh, bx, by;
        {
            const juce::ScopedLock sl (touchLock);
            ts = tiles; liveTouches = touches; chord = chordOffsets; sel = selectedUids;
            bw = boundsW; bh = boundsH; bx = boundsX; by = boundsY;
        }
        if (ts.empty() || bw <= 0 || bh <= 0) return;

        // Only dim/highlight when the selection is a strict subset of what's drawn (a mixed cluster);
        // a single-group arrangement draws plainly.
        const auto isSel = [&sel] (juce::uint64 u) { return std::find (sel.begin(), sel.end(), u) != sel.end(); };
        bool anyUnsel = false;
        for (auto& t : ts) if (! isSel (t.uid)) { anyUnsel = true; break; }
        const bool highlight = ! sel.empty() && anyUnsel;

        // Fit the whole arrangement into our bounds, preserving square units and centring.
        auto full = getLocalBounds().toFloat().reduced (2.0f);
        const float scale = juce::jmin (full.getWidth() / bw, full.getHeight() / bh);
        const float ox = full.getCentreX() - bw * scale * 0.5f - bx * scale;
        const float oy = full.getCentreY() - bh * scale * 0.5f - by * scale;

        for (auto& t : ts)
        {
            juce::Rectangle<float> foot (ox + t.ux * scale, oy + t.uy * scale, t.uw * scale, t.uh * scale);
            drawTile (g, t, foot.reduced (0.5f), liveTouches, chord, highlight, highlight && isSel (t.uid));
        }
    }

private:
    struct Key
    {
        juce::uint64 uid; int finger;
        bool operator< (const Key& o) const noexcept { return uid != o.uid ? uid < o.uid : finger < o.finger; }
    };

    // Targets from MIDI (strike fixed at touch start) + displayed values eased toward them + last-seen.
    struct Live { float x, y, z, strike; float dx, dy, dz; double lastMs; };

    void timerCallback() override
    {
        {
            const juce::ScopedLock sl (touchLock);
            const double now = juce::Time::getMillisecondCounterHiRes();
            for (auto it = touches.begin(); it != touches.end();)
            {
                if (now - it->second.lastMs > kTouchTimeoutMs) { it = touches.erase (it); dirty = true; }
                else
                {
                    // Ease displayed position/press toward the latest sample so 7-bit MIDI steps and
                    // the report cadence don't look jittery. Strike snaps - it's a one-shot impact.
                    auto& t = it->second;
                    t.dx += (t.x - t.dx) * kTouchSmooth;
                    t.dy += (t.y - t.dy) * kTouchSmooth;
                    t.dz += (t.z - t.dz) * kTouchSmooth;
                    if (std::abs (t.x - t.dx) + std::abs (t.y - t.dy) + std::abs (t.z - t.dz) > 0.001f)
                        dirty = true;
                    ++it;
                }
            }
        }
        if (dirty.exchange (false)) repaint();
    }

    // --- layout bookkeeping ---------------------------------------------------------------------
    void rebuildLegacy()
    {
        {
            const juce::ScopedLock sl (touchLock);
            usingLayout = false;   // the legacy setters are the RISE/GRAND + preview path - leave layout mode
            buildLegacyLocked();
            computeBoundsLocked();
            dirty = true;
        }
        repaint();
    }
    void buildLegacyLocked()
    {
        // One keybed tile, ~3 logical units per octave wide by 3 tall. Width tracks the real white-key
        // count (so a 49-key board is sized as 49 keys, not padded to 5 whole octaves); for whole-octave
        // callers this equals the old octaves*3, so their layout is unchanged.
        Tile t; t.uid = 0; t.family = family; t.octaves = octaves;
        t.lowNote  = legacyKeys > 0 ? legacyLow  : defaultLowC (octaves);
        t.keyCount = legacyKeys;   // 0 => whole octaves (drawKeybed derives from octaves)
        const int whites = legacyKeys > 0 ? keybedWhiteCount (t.lowNote, legacyKeys) : octaves * 7;
        t.ux = 0; t.uy = 0;
        t.uw = juce::jmax (1.0f, (float) whites * (3.0f / 7.0f));   // 3 units / 7 white keys
        t.uh = 3.0f;
        // V2 keybed art: size the footprint to the SVG's real aspect (its key pitch differs from the
        // generic 3-units/octave), so stretchToFit doesn't distort it. No-op when there's no V2 art.
        if (useDeviceArtV2)
            if (const char* f = v2FileFor (t.family, t.keyCount > 0 ? t.keyCount : octaves * 12))
                if (auto* a = loadArt (f))
                    if (a->viewBox.getHeight() > 0.0f)
                        t.uw = t.uh * a->viewBox.getWidth() / a->viewBox.getHeight();
        tiles.assign (1, t);
    }

    // --- keybed note geometry (shared by drawKeybed and the RISE/GRAND touch mapping) ------------
    // The keybed is a strip of `keyCount` keys from `lowNote`. White keys are evenly spaced; black
    // keys sit on the boundary between their white neighbours (drawKeybed draws them there). These
    // helpers give the white-key count and a note's horizontal centre so rings land on real keys.
    static bool keyIsWhite (int pc)        { static const bool w[12] = {1,0,1,0,1,1,0,1,0,1,0,1}; return w[((pc % 12) + 12) % 12]; }
    static int  defaultLowC (int octaves)  { return 60 - 12 * (juce::jlimit (1, 8, octaves) / 2); }  // a C, middle-C centred

    static int keybedWhiteCount (int lowNote, int keyCount)
    {
        int n = 0;
        for (int i = 0; i < keyCount; ++i) if (keyIsWhite (lowNote + i)) ++n;
        return juce::jmax (1, n);
    }

    static float keybedNoteX (int note, int lowNote, int keyCount)
    {
        // White index within an octave (-1 = black), and, for a black key, the white it sits after.
        static const int wIdx[12]   = { 0,-1, 1,-1, 2, 3,-1, 4,-1, 5,-1, 6 };
        static const int bAfter[12] = {-1, 0,-1, 1,-1,-1, 3,-1, 4,-1, 5,-1 };
        const int totalW = keybedWhiteCount (lowNote, keyCount);
        const int rel = note - lowNote;
        const int oct = rel >= 0 ? rel / 12 : -((-rel + 11) / 12);   // floored div (rel may be negative)
        const int pc  = ((rel % 12) + 12) % 12;
        const float whitePos = wIdx[pc] >= 0 ? (float) (oct * 7 + wIdx[pc]) + 0.5f    // white-key centre
                                             : (float) (oct * 7 + bAfter[pc] + 1);     // black on the boundary
        return juce::jlimit (0.0f, 1.0f, whitePos / (float) totalW);
    }
    void computeBoundsLocked()
    {
        if (tiles.empty()) { boundsX = boundsY = 0; boundsW = boundsH = 1; return; }
        float x0 = tiles[0].ux, y0 = tiles[0].uy, x1 = tiles[0].ux + tiles[0].uw, y1 = tiles[0].uy + tiles[0].uh;
        for (auto& t : tiles)
        { x0 = juce::jmin (x0, t.ux); y0 = juce::jmin (y0, t.uy);
          x1 = juce::jmax (x1, t.ux + t.uw); y1 = juce::jmax (y1, t.uy + t.uh); }
        boundsX = x0; boundsY = y0; boundsW = juce::jmax (0.1f, x1 - x0); boundsH = juce::jmax (0.1f, y1 - y0);
    }

    // Which tile's footprint contains p (view pixels), or 0 if none. Same fit transform as paint().
    juce::uint64 tileUidAt (juce::Point<float> p) const
    {
        std::vector<Tile> ts; float bw, bh, bx, by;
        { const juce::ScopedLock sl (touchLock); ts = tiles; bw = boundsW; bh = boundsH; bx = boundsX; by = boundsY; }
        if (ts.empty() || bw <= 0 || bh <= 0) return 0;
        auto full = getLocalBounds().toFloat().reduced (2.0f);
        const float scale = juce::jmin (full.getWidth() / bw, full.getHeight() / bh);
        const float ox = full.getCentreX() - bw * scale * 0.5f - bx * scale;
        const float oy = full.getCentreY() - bh * scale * 0.5f - by * scale;
        for (auto& t : ts)
            if (t.uid != 0
                && juce::Rectangle<float> (ox + t.ux * scale, oy + t.uy * scale, t.uw * scale, t.uh * scale).contains (p))
                return t.uid;
        return 0;
    }

    // --- one tile (art + its touch markers), rotated about its centre if the block is rotated -----
    void drawTile (juce::Graphics& g, const Tile& t, juce::Rectangle<float> foot,
                   const std::map<Key, Live>& liveTouches, const std::vector<int>& chord,
                   bool highlight = false, bool selected = false)
    {
        const float cx = foot.getCentreX(), cy = foot.getCentreY();
        const bool odd = (t.rotation & 1) != 0;
        // Native (unrotated) rect: the footprint with w/h swapped back when rotated 90/270.
        juce::Rectangle<float> nat = odd ? juce::Rectangle<float> (cx - foot.getHeight() * 0.5f, cy - foot.getWidth() * 0.5f,
                                                                   foot.getHeight(), foot.getWidth())
                                         : foot;
        juce::Graphics::ScopedSaveState sss (g);
        if (t.rotation != 0)
            g.addTransform (juce::AffineTransform::rotation (t.rotation * juce::MathConstants<float>::halfPi, cx, cy));

        // Dashboard-style selection: every block stays fully visible; only its accent border changes.
        // The selected device's block(s) get a bright border, the others a dim one. With no selection
        // (a single device, or nothing highlighted) the border is the normal accent.
        const juce::Colour border = selected ? grab::theme::accent.brighter (0.25f)
                                  : highlight ? grab::theme::accent.withAlpha (0.28f)
                                              : grab::theme::accent.withAlpha (0.9f);

        const bool keybed = (t.family == Family::block || t.family == Family::rise || t.family == Family::grand);
        // Resolve the keybed's exact key range (keyCount==0 => whole octaves from `octaves`).
        const int kc = t.keyCount > 0 ? t.keyCount : t.octaves * 12;
        const int lo = t.keyCount > 0 ? t.lowNote  : defaultLowC (t.octaves);
        if      (t.family == Family::lumi)     drawFlatKeybed (g, nat, t.octaves, border);
        else if (t.family == Family::lightpad) drawPad (g, nat, t.grid, border);
        else if (t.family == Family::control)  drawControl (g, nat, t.control, border);
        else                                   drawKeybed (g, nat, t.family, lo, kc, border);

        // Touch markers for this tile.
        std::vector<const Live*> mine;
        for (auto& kv : liveTouches) if (kv.first.uid == t.uid) mine.push_back (&kv.second);
        if (mine.empty()) return;

        auto inset = (t.family == Family::lightpad || t.family == Family::control) ? nat.reduced (kBodyMargin)
                                                                                   : nat.reduced (kBodyMargin, kKeyInsetV);
        // V2 art: land the rings on the SVG's real playing rect (data-grab-active-area).
        if (useDeviceArtV2)
            if (const char* f = v2FileFor (t.family, t.keyCount > 0 ? t.keyCount : t.octaves * 12))
                if (auto* a = loadArt (f))
                    inset = activeInset (*a, nat);
        auto drawMarker = [&g, &inset] (float nx, float ny, float strike, float press, float a)
        {
            const float mx = inset.getX() + juce::jlimit (0.0f, 1.0f, nx) * inset.getWidth();
            const float my = inset.getY() + juce::jlimit (0.0f, 1.0f, ny) * inset.getHeight();

            // Pressure ring (5D "Press" teal) tracks live pressure; strike disc (5D "Strike" blue) is the
            // initial hit. The ring is drawn at true pressure, and the disc is clamped to stay inside it:
            // when pressure falls below the strike size the disc shrinks to match, so the pressure ring is
            // always visible outside the disc instead of being hidden under it.
            const float strikeR = 3.0f + juce::jlimit (0.0f, 1.0f, strike) * 11.0f;
            const float outer   = 6.0f + juce::jlimit (0.0f, 1.0f, press)  * 16.0f;
            const float inner   = juce::jmin (strikeR, outer - 2.0f);
            juce::Rectangle<float> discC (mx - inner, my - inner, inner * 2.0f, inner * 2.0f);
            juce::Rectangle<float> ringC (mx - outer, my - outer, outer * 2.0f, outer * 2.0f);
            g.setColour (grab::theme::press.withAlpha  (0.12f * a)); g.fillEllipse (ringC);   // faint fill inside ring
            g.setColour (grab::theme::strike.withAlpha (0.85f * a)); g.fillEllipse (discC);   // strike disc (blue)
            g.setColour (grab::theme::press.withAlpha  (0.95f * a)); g.drawEllipse (ringC, 1.6f); // pressure ring (teal)
        };

        // Chord ghosts only on keybeds (Seaboard). Approximate: a +k-semitone ghost sits k keys
        // along the strip. On a pad grid the note layout isn't a pitch axis, so no ghosts.
        const float semi = keybed ? 1.0f / (float) juce::jmax (1, kc) : 0.0f;
        for (auto* m : mine)
        {
            if (keybed)
                for (int off : chord)
                    drawMarker (m->dx + (float) off * semi, m->dy, m->strike, m->dz, 0.5f);
            drawMarker (m->dx, m->dy, m->strike, m->dz, 1.0f);
        }
    }

    // 12-bit scale masks (bit n set = semitone n above the root is in the scale), taken verbatim
    // from ROLI Dashboard's own Littlefoot library (Scale.littlefoot) so lit keys match the device.
    static int scaleMask (int scaleId)
    {
        static const int masks[19] = {
            0xab5, 0x5ad, 0x9ad, 0x4a5, 0x295, 0x4a9, 0x4e9, 0x6ad, 0x5ab, 0xad5,
            0x6b5, 0x56b, 0x555, 0xb6d, 0x575, 0x8d1, 0x8b1, 0x57b, 0xfff
        };
        return (scaleId >= 0 && scaleId < 19) ? masks[scaleId] : 0xfff;
    }

    juce::Colour keyColour (int pitchClass, bool white) const
    {
        const int  rel     = (((pitchClass - rootNote) % 12) + 12) % 12;
        const bool inScale = ((scaleMask (scaleId) >> rel) & 1) != 0;
        const bool isRoot  = (rel == 0);
        const juce::Colour off  (0xff0c0d12);
        const juce::Colour dim  (0xff15161f);
        const auto rainbow = [pitchClass] (float v) { return juce::Colour::fromHSV ((float) pitchClass / 12.0f, 0.85f, v, 1.0f); };

        juce::Colour c;
        switch (colourScheme)
        {
            case 2:  c = white ? juce::Colour (0xffe9ebee) : juce::Colour (0xff15161b); break;
            case 3:  c = rainbow (0.35f); break;
            case 4:  c = inScale ? rainbow (1.0f) : off; break;
            case 0:  c = isRoot ? juce::Colour (rootArgb).withAlpha (1.0f) : inScale ? juce::Colour (0xffe9ebee) : dim; break;
            case 1:
            default: c = isRoot ? juce::Colour (rootArgb).withAlpha (1.0f)
                        : inScale ? juce::Colour (inScaleArgb).withAlpha (1.0f) : dim; break;
        }
        // Track the device's LED brightness (id 36, 0..100) so the keybed dims with it, live or preview.
        return brightness >= 100 ? c : c.withMultipliedBrightness (brightness * 0.01f);
    }

    // Flat, per-key coloured keyboard for LUMI / Piano (Dashboard-style), not the Seaboard keywave.
    void drawFlatKeybed (juce::Graphics& g, juce::Rectangle<float> r, int octaveCount,
                         juce::Colour border = grab::theme::accent.withAlpha (0.9f))
    {
        // V2: LUMI / ROLI Piano from the SVG kit, with each key recoloured live by the device's
        // colour scheme (key-w-N / key-b-N). Footprint aspect is fixed in buildLegacyLocked.
        if (useDeviceArtV2)
            if (const char* f = v2FileFor (Family::lumi, octaveCount * 12))
                if (auto* a = loadArt (f))
                {
                    recolourLumiKeys (*a);
                    recolourFrame (*a, border);
                    a->drawable->drawWithin (g, r, juce::RectanglePlacement::stretchToFit, 1.0f);
                    return;
                }

        g.setColour (juce::Colour (0xff0c0d12));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 1.4f);

        auto keys = r.reduced (kBodyMargin, kKeyInsetV);
        const int   totalWhites = juce::jmax (1, octaveCount * 7);
        const float ww     = keys.getWidth() / (float) totalWhites;
        const float blackH = keys.getHeight() * 0.60f;
        const float bw     = ww * 0.60f;

        static const int whitePc[7]    = { 0, 2, 4, 5, 7, 9, 11 };
        static const bool blackAfter[7] = { true, true, false, true, true, true, false };
        static const int blackPc[7]    = { 1, 3, -1, 6, 8, 10, -1 };

        for (int w = 0; w < totalWhites; ++w)
        {
            juce::Rectangle<float> k (keys.getX() + w * ww, keys.getY(), ww, keys.getHeight());
            k = k.reduced (1.0f, 0.0f);
            g.setColour (keyColour (whitePc[w % 7], true));
            g.fillRoundedRectangle (k, 2.5f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (k, 2.5f, 1.0f);
        }
        for (int o = 0; o < octaveCount; ++o)
            for (int i = 0; i < 7; ++i)
                if (blackAfter[i])
                {
                    const float cx = keys.getX() + (float) (o * 7 + i + 1) * ww;
                    juce::Rectangle<float> blk (cx - bw * 0.5f, keys.getY(), bw, blackH);
                    g.setColour (juce::Colours::black.withAlpha (0.45f));
                    g.fillRoundedRectangle (blk.expanded (1.2f, 0.0f), 2.5f);
                    g.setColour (keyColour (blackPc[i], false));
                    g.fillRoundedRectangle (blk, 2.5f);
                }
    }

    // Pseudo-3D moulded keywave for Seaboard / RISE / GRAND. Draws exactly `keyCount` keys from
    // `lowNote` (white keys first, then blacks on top), so a 49-key board shows all 49 keys.
    void drawKeybed (juce::Graphics& g, juce::Rectangle<float> r, Family fam, int lowNote, int keyCount,
                     juce::Colour border = grab::theme::accent.withAlpha (0.9f))
    {
        // V2: Seaboard Block / GRAND keybed from the SVG kit. The tile footprint is set to the SVG's
        // aspect in buildLegacyLocked, so stretchToFit doesn't distort. RISE still draws below (its
        // SVG includes the control panel, handled in a later phase). Border overlaid for selection.
        if (useDeviceArtV2)
            if (const char* f = v2FileFor (fam, keyCount))
                if (auto* a = loadArt (f))
                {
                    recolourFrame (*a, border);   // selection highlight via the SVG's own frame
                    a->drawable->drawWithin (g, r, juce::RectanglePlacement::stretchToFit, 1.0f);
                    return;   // the SVG carries its own chassis + selection-frame (correct corner radius)
                }

        const int   totalWhites = keybedWhiteCount (lowNote, keyCount);
        const float radius   = fam == Family::block ? 2.5f : fam == Family::rise ? 4.0f : 5.5f;
        const float crestLum = fam == Family::block ? 0.00f : fam == Family::rise ? -0.06f : -0.10f;

        const juce::Colour whiteEdge  (0xff111318);
        const juce::Colour whiteCrest = juce::Colour (0xff3a3f4a).brighter (crestLum >= 0 ? crestLum : 0.0f)
                                            .darker  (crestLum < 0 ? -crestLum : 0.0f);
        const juce::Colour blackEdge  (0xff060708);
        const juce::Colour blackCrest (0xff22252c);

        const juce::Colour flatSurface = whiteEdge.interpolatedWith (whiteCrest, 0.5f);
        g.setColour (flatSurface);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 1.6f);

        auto keys = r.reduced (kBodyMargin, kKeyInsetV);
        const float ww       = keys.getWidth() / (float) totalWhites;
        const float blackH   = keys.getHeight() * (fam == Family::block ? 0.42f : 0.50f);
        const float glideTop = keys.getHeight() * 0.11f;

        // Pass 1: white keys, left to right.
        int wc = 0;
        for (int s = 0; s < keyCount; ++s)
        {
            if (! keyIsWhite (lowNote + s)) continue;
            juce::Rectangle<float> col (keys.getX() + wc * ww, keys.getY(), ww, keys.getHeight());
            auto k = col.withTrimmedTop (glideTop).withTrimmedBottom (glideTop).reduced (0.8f, 0.0f);
            juce::ColourGradient grad (whiteEdge, k.getX(), 0.0f, whiteEdge, k.getRight(), 0.0f, false);
            grad.addColour (0.5, whiteCrest);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (k, radius);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (k.removeFromBottom (k.getHeight() * 0.14f), radius);
            ++wc;
        }
        // Pass 2: black keys on top, each on the boundary after its preceding white key.
        wc = 0;
        for (int s = 0; s < keyCount; ++s)
        {
            if (keyIsWhite (lowNote + s)) { ++wc; continue; }
            const float cx = keys.getX() + (float) wc * ww;   // boundary after the last white
            const float bw = ww * 0.58f;
            juce::Rectangle<float> blk (cx - bw * 0.5f, keys.getY() + glideTop, bw, blackH);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (blk.translated (0.0f, 3.0f).expanded (1.5f, 1.0f), radius + 1.0f);
            juce::ColourGradient bg (blackEdge, blk.getX(), 0.0f, blackEdge, blk.getRight(), 0.0f, false);
            bg.addColour (0.5, blackCrest);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (blk, radius);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRoundedRectangle (blk.getCentreX() - 0.7f, blk.getY() + 3.0f, 1.4f, blk.getHeight() - 7.0f, 0.7f);
        }
    }

    // Lightpad: a square body with an NxN grid of dim LED dots (touch rings + ghosts draw over it).
    void drawPad (juce::Graphics& g, juce::Rectangle<float> r, int gridN,
                  juce::Colour border = grab::theme::accent.withAlpha (0.9f))
    {
        // V2: Lightpad body + 15x15 LED grid from the SVG kit (200x200 square == the 2x2 footprint,
        // so it maps cleanly). Touch rings draw over it via the active-area inset in drawTile.
        if (useDeviceArtV2)
            if (auto* a = loadArt ("lightpad-block.svg"))
            {
                recolourFrame (*a, border);
                a->drawable->drawWithin (g, r, juce::RectanglePlacement::stretchToFit, 1.0f);
                return;   // the SVG carries its own chassis + selection-frame (correct corner radius)
            }

        gridN = juce::jlimit (3, 30, gridN);
        g.setColour (juce::Colour (0xff0b0c11));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 1.6f);

        auto pad = r.reduced (kBodyMargin + 2.0f);
        const float cell = juce::jmin (pad.getWidth(), pad.getHeight()) / (float) gridN;
        const float dot  = cell * 0.62f;
        const float gx   = pad.getCentreX() - cell * gridN * 0.5f;
        const float gy   = pad.getCentreY() - cell * gridN * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int yy = 0; yy < gridN; ++yy)
            for (int xx = 0; xx < gridN; ++xx)
                g.fillEllipse (gx + xx * cell + (cell - dot) * 0.5f, gy + yy * cell + (cell - dot) * 0.5f, dot, dot);
    }

    // A parsed device-art SVG: the Drawable plus its viewBox and data-grab-active-area (both in
    // viewBox coords). Declared here so loadArt/activeInset can name it in their signatures.
    struct Art { std::unique_ptr<juce::Drawable> drawable; juce::Rectangle<float> viewBox, active; };

    // Device art V2: an embedded model SVG parsed into a Drawable, with its viewBox and the
    // data-grab-active-area (the real playing rect inside the bezel, in viewBox coords). Returns
    // nullptr if the resource is missing or unparseable (caller falls back to the drawn art).
    // Keyed by filename; a cached failure stays cached (returns nullptr without re-parsing).
    const Art* loadArt (const juce::String& file)
    {
        auto it = artCache.find (file);
        if (it != artCache.end()) return it->second.drawable != nullptr ? &it->second : nullptr;

        Art art;
        for (int i = 0; i < DeviceArtData::namedResourceListSize; ++i)
            if (file == DeviceArtData::originalFilenames[i])
            {
                int size = 0;
                if (const char* data = DeviceArtData::getNamedResource (DeviceArtData::namedResourceList[i], size))
                    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
                    {
                        art.drawable = juce::Drawable::createFromSVG (*xml);
                        const auto rect = [&xml] (juce::StringRef attr, juce::Rectangle<float> fallback)
                        {
                            auto t = juce::StringArray::fromTokens (xml->getStringAttribute (attr), " ", "");
                            return t.size() == 4 ? juce::Rectangle<float> (t[0].getFloatValue(), t[1].getFloatValue(),
                                                                           t[2].getFloatValue(), t[3].getFloatValue())
                                                 : fallback;
                        };
                        art.viewBox = rect ("viewBox", { 0, 0, 100, 100 });
                        art.active  = rect ("data-grab-active-area", art.viewBox);
                    }
                break;
            }
        auto& slot = artCache[file];   // caches the failure too (empty Art) so we don't re-parse
        slot = std::move (art);
        return slot.drawable != nullptr ? &slot : nullptr;
    }

    // The SVG filename for a control-block subtype (Developer and generic share the Developer art).
    static const char* controlArtFile (Control sub)
    {
        return sub == Control::live  ? "control-live.svg"
             : sub == Control::loop  ? "control-loop.svg"
             : sub == Control::touch ? "control-touch.svg"
                                     : "control-developer.svg";
    }

    // The model SVG for a family + key count, or nullptr if that family has no V2 art yet.
    // GRAND/LUMI map to the nearest built size; Block/Lightpad are fixed. Control uses controlArtFile
    // (per subtype); RISE returns nullptr (its SVG bundles the control panel - a later phase).
    static const char* v2FileFor (Family fam, int keyCount)
    {
        switch (fam)
        {
            case Family::block:    return "seaboard-block-24.svg";
            case Family::lightpad: return "lightpad-block.svg";
            case Family::lumi:     return keyCount >= 37 ? "piano-49.svg" : "lumi-24.svg";
            case Family::grand:
            {
                static const int   sz[3] = { 37, 61, 88 };
                static const char* fl[3] = { "grand-studio-37.svg", "grand-stage-61.svg", "grand-88.svg" };
                int best = 0;
                for (int i = 1; i < 3; ++i)
                    if (std::abs (keyCount - sz[i]) < std::abs (keyCount - sz[best])) best = i;
                return fl[best];
            }
            case Family::rise:
            case Family::control:
            default:               return nullptr;
        }
    }

    // Find a sub-drawable by its SVG id (JUCE stores the id as the Drawable's componentID).
    static juce::Drawable* findDrawableById (juce::Drawable* d, juce::StringRef id)
    {
        if (d == nullptr) return nullptr;
        if (d->getComponentID() == id) return d;
        if (auto* comp = dynamic_cast<juce::DrawableComposite*> (d))
            for (int i = 0; i < comp->getNumChildComponents(); ++i)
                if (auto* child = dynamic_cast<juce::Drawable*> (comp->getChildComponent (i)))
                    if (auto* found = findDrawableById (child, id)) return found;
        return nullptr;
    }

    // Recolour the SVG's selection-frame stroke to convey selection state (bright/dim/normal accent).
    void recolourFrame (const Art& a, juce::Colour c)
    {
        if (auto* d = findDrawableById (a.drawable.get(), "selection-frame"))
            if (auto* s = dynamic_cast<juce::DrawableShape*> (d))
                s->setStrokeFill (c);
    }

    // Recolour each LUMI/Piano key (key-w-N / key-b-N) by the live colour scheme, the way the
    // drawn flat keybed does - so the SVG keys light up with the device's scheme, not the demo art.
    void recolourLumiKeys (const Art& a)
    {
        static const int whitePc[7]  = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackSeq[5] = { 1, 3, 6, 8, 10 };
        auto* root = a.drawable.get();
        for (int i = 0; ; ++i)
        {
            auto* d = findDrawableById (root, juce::String ("key-w-") + juce::String (i));
            if (d == nullptr) break;
            if (auto* s = dynamic_cast<juce::DrawableShape*> (d)) s->setFill (keyColour (whitePc[i % 7], true));
        }
        for (int j = 0; ; ++j)
        {
            auto* d = findDrawableById (root, juce::String ("key-b-") + juce::String (j));
            if (d == nullptr) break;
            if (auto* s = dynamic_cast<juce::DrawableShape*> (d)) s->setFill (keyColour (blackSeq[j % 5], false));
        }
    }

    // Map an Art's active-area (viewBox coords) onto the drawn footprint `nat`, so touch rings land
    // where the real playing surface is drawn in the SVG rather than on a guessed inset.
    static juce::Rectangle<float> activeInset (const Art& a, juce::Rectangle<float> nat)
    {
        if (a.viewBox.getWidth() <= 0 || a.viewBox.getHeight() <= 0) return nat;
        return { nat.getX() + (a.active.getX() / a.viewBox.getWidth())  * nat.getWidth(),
                 nat.getY() + (a.active.getY() / a.viewBox.getHeight()) * nat.getHeight(),
                 (a.active.getWidth()  / a.viewBox.getWidth())  * nat.getWidth(),
                 (a.active.getHeight() / a.viewBox.getHeight()) * nat.getHeight() };
    }

    // Control block (Live / Loop / Touch): a short dark body with a row of button discs.
    // A ROLI Control Block: Live / Loop / Touch (and Developer). All are the same 10-button hardware
    // - 5 round buttons on top, 3 round + a wide -/+ on the bottom, with an LED dot strip above. The
    // subtype picks the LED colour and the printed icons, drawn as our own stylised vector glyphs.
    void drawControl (juce::Graphics& g, juce::Rectangle<float> r, Control sub,
                      juce::Colour border = grab::theme::accent.withAlpha (0.7f))
    {
        // V2: draw the designer's SVG, then GRAB's accent border on top so selection highlight still
        // reads. Falls through to the hand-drawn art if the SVG isn't available.
        if (useDeviceArtV2)
            if (auto* a = loadArt (controlArtFile (sub)))
            {
                recolourFrame (*a, border);
                a->drawable->drawWithin (g, r, juce::RectanglePlacement::stretchToFit, 1.0f);
                return;   // the SVG carries its own chassis + selection-frame (correct corner radius)
            }

        g.setColour (juce::Colour (0xff17181e));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.4f);

        const juce::Colour led = sub == Control::touch     ? juce::Colour (0xffb39cff)   // purple
                               : sub == Control::loop      ? juce::Colour (0xff35d0c6)   // teal
                               : sub == Control::live      ? juce::Colour (0xffff9a3c)   // orange
                               : sub == Control::developer ? juce::Colour (0xffe6e6ee)   // developer: neutral white (blank caps)
                                                           : juce::Colour (0xff6a6d7a);  // generic: dim
        auto body = r.reduced (kBodyMargin + 4.0f);

        // LED dot strip across the top (a few lit in the middle, like the hardware).
        const int nLed = 13;
        const float ledY  = body.getY() + body.getHeight() * 0.05f;
        const float ledGap = body.getWidth() / (float) (nLed + 1);
        for (int i = 0; i < nLed; ++i)
        {
            const bool lit = i >= 4 && i <= 7;
            g.setColour (led.withAlpha (lit ? 0.95f : 0.30f));
            g.fillEllipse (body.getX() + ledGap * (i + 1) - 1.5f, ledY - 1.5f, 3.0f, 3.0f);
        }

        // Two button rows below the strip.
        auto btns = body.withTrimmedTop (body.getHeight() * 0.18f);
        const float colGap = btns.getWidth() / 5.0f;
        const float d = juce::jmin (btns.getHeight() * 0.42f, colGap * 0.82f);
        const float topY = btns.getY() + btns.getHeight() * 0.27f;
        const float botY = btns.getY() + btns.getHeight() * 0.74f;

        auto disc = [&] (float cx, float cy, int idx)
        {
            juce::Rectangle<float> bb (cx - d * 0.5f, cy - d * 0.5f, d, d);
            g.setColour (juce::Colour (0xff2a2c34));
            g.fillEllipse (bb);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (bb, 1.0f);
            drawControlIcon (g, sub, idx, bb.reduced (d * 0.24f), led.brighter (0.2f));
        };

        for (int i = 0; i < 5; ++i) disc (btns.getX() + colGap * (i + 0.5f), topY, i);       // top row 0-4
        for (int i = 0; i < 3; ++i) disc (btns.getX() + colGap * (i + 0.5f), botY, 5 + i);   // bottom 5-7

        // Wide -/+ pill (buttons 8 & 9) filling the bottom-right two columns.
        juce::Rectangle<float> pill (btns.getX() + colGap * 3.08f, botY - d * 0.55f, colGap * 1.84f, d * 1.1f);
        g.setColour (juce::Colour (0xff2a2c34));
        g.fillRoundedRectangle (pill, d * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (pill, d * 0.5f, 1.0f);
        const float pr = d * 0.16f, lw = juce::jmax (1.2f, d * 0.08f);
        g.setColour (led.brighter (0.2f));
        const float lx = pill.getX() + pill.getWidth() * 0.27f, rx = pill.getX() + pill.getWidth() * 0.73f, py = pill.getCentreY();
        g.fillRoundedRectangle (lx - pr, py - lw * 0.5f, pr * 2.0f, lw, lw * 0.5f);                 // minus
        g.fillRoundedRectangle (rx - pr, py - lw * 0.5f, pr * 2.0f, lw, lw * 0.5f);                 // plus -
        g.fillRoundedRectangle (rx - lw * 0.5f, py - pr, lw, pr * 2.0f, lw * 0.5f);                 // plus |
    }

    // One control-block button glyph, drawn into box `b`. Simplified but recognisable per subtype.
    void drawControlIcon (juce::Graphics& g, Control sub, int idx, juce::Rectangle<float> b, juce::Colour c)
    {
        const float cx = b.getCentreX(), cy = b.getCentreY(), s = b.getWidth() * 0.5f;
        const float lw = juce::jmax (1.1f, b.getWidth() * 0.11f);
        const juce::PathStrokeType stroke (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (c);
        auto chevron = [&] (float yc, float dir) { juce::Path p;                 // dir +1 down / -1 up
            p.startNewSubPath (cx - s * 0.7f, yc - dir * s * 0.35f); p.lineTo (cx, yc + dir * s * 0.35f);
            p.lineTo (cx + s * 0.7f, yc - dir * s * 0.35f); g.strokePath (p, stroke); };
        auto tri = [&] (float ox, float dir) { juce::Path p;                     // play/tempo triangle
            p.startNewSubPath (cx + ox - dir * s * 0.35f, cy - s * 0.55f);
            p.lineTo (cx + ox + dir * s * 0.45f, cy); p.lineTo (cx + ox - dir * s * 0.35f, cy + s * 0.55f);
            p.closeSubPath(); g.fillPath (p); };
        auto grid = [&] { for (int ry = 0; ry < 3; ++ry) for (int cxi = 0; cxi < 3; ++cxi)
            g.fillEllipse (cx - s * 0.6f + cxi * s * 0.6f - lw * 0.7f, cy - s * 0.6f + ry * s * 0.6f - lw * 0.7f, lw * 1.4f, lw * 1.4f); };
        auto speaker = [&] { juce::Path p; p.addRectangle (cx - s * 0.7f, cy - s * 0.25f, s * 0.35f, s * 0.5f);
            p.startNewSubPath (cx - s * 0.35f, cy - s * 0.25f); p.lineTo (cx, cy - s * 0.6f);
            p.lineTo (cx, cy + s * 0.6f); p.lineTo (cx - s * 0.35f, cy + s * 0.25f); p.closeSubPath(); g.fillPath (p);
            for (int w = 1; w <= 2; ++w) { juce::Path a; a.addCentredArc (cx, cy, s * (0.2f + 0.25f * w), s * (0.2f + 0.25f * w),
                0.0f, -0.7f, 0.7f, true); g.strokePath (a, juce::PathStrokeType (lw * 0.8f)); } };
        auto target = [&] { g.drawEllipse (cx - s * 0.6f, cy - s * 0.6f, s * 1.2f, s * 1.2f, lw * 0.9f);
            g.fillEllipse (cx - s * 0.2f, cy - s * 0.2f, s * 0.4f, s * 0.4f); };
        auto circle = [&] (float rr, bool fill) { juce::Rectangle<float> e (cx - s * rr, cy - s * rr, s * rr * 2.0f, s * rr * 2.0f);
            if (fill) g.fillEllipse (e); else g.drawEllipse (e, lw * 0.9f); };
        auto lock = [&] (float arrowDir) {                                        // padlock + small arrow
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.05f, s * 0.9f, s * 0.7f, lw * 0.6f, lw * 0.8f);
            juce::Path a; a.addCentredArc (cx, cy - s * 0.05f, s * 0.28f, s * 0.3f, 0.0f, -1.6f, 1.6f, true);
            g.strokePath (a, juce::PathStrokeType (lw * 0.8f));
            if (arrowDir != 0.0f) chevron (cy + s * 0.28f, arrowDir); };

        if (sub == Control::touch)
        {
            switch (idx) {
                case 0: chevron (cy, +1); break;                                  // octave/nav down
                case 1: { juce::Path p; p.startNewSubPath (cx - s * 0.1f, cy - s * 0.5f); p.lineTo (cx - s * 0.6f, cy);
                          p.lineTo (cx - s * 0.1f, cy + s * 0.5f); p.startNewSubPath (cx + s * 0.1f, cy - s * 0.5f);
                          p.lineTo (cx + s * 0.6f, cy); p.lineTo (cx + s * 0.1f, cy + s * 0.5f); g.strokePath (p, stroke); } break;
                case 2: chevron (cy - s * 0.3f, -1); chevron (cy + s * 0.3f, +1); break;
                case 3: target(); break;
                case 4: chevron (cy, -1); break;                                  // up
                case 5: lock (+1); break;
                case 6: chevron (cy - s * 0.42f, -1); lock (+1); break;
                case 7: { g.drawRoundedRectangle (cx - s * 0.65f, cy - s * 0.4f, s * 1.3f, s * 0.8f, 1.5f, lw * 0.7f);
                          for (int k = 1; k < 4; ++k) g.drawLine (cx - s * 0.65f + k * s * 0.325f, cy - s * 0.4f,
                                                                  cx - s * 0.65f + k * s * 0.325f, cy + s * 0.4f, lw * 0.6f); } break;
                default: break;
            }
        }
        else if (sub == Control::loop)
        {
            switch (idx) {
                case 0: grid(); break;
                case 1: speaker(); break;
                case 2: { juce::Path p; p.startNewSubPath (cx - s * 0.5f, cy + s * 0.6f); p.lineTo (cx + s * 0.1f, cy - s * 0.6f);
                          p.lineTo (cx + s * 0.5f, cy + s * 0.6f); g.strokePath (p, stroke);
                          g.drawLine (cx - s * 0.35f, cy + s * 0.6f, cx + s * 0.1f, cy - s * 0.1f, lw * 0.8f); } break;  // metronome
                case 3: tri (-s * 0.4f, +1); tri (0.0f, +1); tri (s * 0.4f, +1); break;                                  // tempo >>>
                case 4: { juce::Path a; a.addCentredArc (cx, cy, s * 0.5f, s * 0.5f, 0.0f, -2.6f, 0.9f, true);
                          g.strokePath (a, stroke); } break;                                                             // undo
                case 5: tri (-s * 0.25f, +1); g.fillRoundedRectangle (cx + s * 0.15f, cy - s * 0.45f, lw, s * 0.9f, lw * 0.4f);
                        g.fillRoundedRectangle (cx + s * 0.42f, cy - s * 0.45f, lw, s * 0.9f, lw * 0.4f); break;          // play/pause
                case 6: g.setColour (juce::Colour (0xffff4d58)); circle (0.5f, true); break;                             // record
                case 7: { juce::Path p; p.startNewSubPath (cx - s * 0.6f, cy); p.lineTo (cx, cy - s * 0.4f);             // mortarboard
                          p.lineTo (cx + s * 0.6f, cy); p.lineTo (cx, cy + s * 0.4f); p.closeSubPath(); g.fillPath (p); } break;
                default: break;
            }
        }
        else if (sub == Control::live)
        {
            switch (idx) {
                case 0: grid(); break;
                case 1: speaker(); break;
                case 2: case 3: case 4: { for (int k = 0; k < 2 + idx - 2; ++k) {}   // scene dots rows
                          g.drawEllipse (cx - s * 0.6f, cy - s * 0.6f, s * 1.2f, s * 1.2f, lw * 0.7f);
                          g.fillEllipse (cx - lw, cy - s * 0.35f - lw, lw * 2.0f, lw * 2.0f);
                          g.fillEllipse (cx - lw, cy + s * 0.15f - lw, lw * 2.0f, lw * 2.0f); } break;
                case 5: g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.55f, s * 0.9f, s * 1.1f, 1.5f, lw * 0.8f); break; // clip
                case 6: g.fillRoundedRectangle (cx - s * 0.5f, cy - s * 0.5f, s, s, s * 0.25f); break;                      // highlight
                case 7: { juce::Path p; p.startNewSubPath (cx, cy + s * 0.55f);                                            // heart
                          p.cubicTo (cx - s * 0.9f, cy - s * 0.1f, cx - s * 0.35f, cy - s * 0.7f, cx, cy - s * 0.2f);
                          p.cubicTo (cx + s * 0.35f, cy - s * 0.7f, cx + s * 0.9f, cy - s * 0.1f, cx, cy + s * 0.55f);
                          p.closeSubPath(); g.fillPath (p); } break;
                default: break;
            }
        }
        else if (sub == Control::developer)   // blank dev caps are printed 0-7
        {
            g.setColour (c);
            g.setFont (juce::Font (juce::FontOptions (s * 1.5f)).boldened());
            g.drawText (juce::String (idx), b.expanded (s * 0.5f), juce::Justification::centred, false);
        }
        else   // generic: a plain dot
            g.fillEllipse (cx - lw, cy - lw, lw * 2.0f, lw * 2.0f);
    }

    // --- constants / state ----------------------------------------------------------------------
    static constexpr int kBodyMargin = 3;    // side margin so the accent border frames the content
    static constexpr int kKeyInsetV  = 4;    // vertical inset of keys from the body edge

    int    octaves = 2;                      // legacy keybed size (RISE/GRAND + previews)
    int    legacyLow  = 48;                  // exact-range keybed: leftmost MIDI note (a C)
    int    legacyKeys = 0;                   // exact-range keybed: key count (0 = whole octaves)
    Family family  = Family::rise;

    // Device art V2 (off by default; flipped from Advanced -> New device art). artCache holds each
    // parsed SVG (Drawable + viewBox + active-area) keyed by filename, so it is parsed at most once.
    // (struct Art is declared above, next to loadArt, so its signature can name it.)
    bool   useDeviceArtV2 = false;
    std::map<juce::String, Art> artCache;

    std::vector<Tile> tiles;                 // the drawn arrangement (legacy keybed or real topology)
    bool   usingLayout = false;              // true once setLayout() has supplied real tiles
    float  boundsX = 0, boundsY = 0, boundsW = 1, boundsH = 1;   // unit-space bounding box of `tiles`

    // LUMI / Piano key-colour state (see setColourScheme).
    int           colourScheme = 1;
    int           scaleId     = 0;
    int           rootNote    = 0;
    int           brightness  = 100;         // LUMI/Piano LED brightness (id 36, 0..100)
    juce::uint32  inScaleArgb = 0xff00b0bb;
    juce::uint32  rootArgb    = 0xffffffff;

    static constexpr double kTouchTimeoutMs = 140.0;   // drop a ring this long after its last update
    static constexpr float  kTouchSmooth    = 0.35f;   // per-60Hz-frame easing toward the latest sample
    std::map<Key, Live>   touches;           // active touches, keyed by {tile uid, finger}
    std::vector<int>      chordOffsets;      // chord-mode ghost intervals (semitones), empty = off
    std::vector<juce::uint64> selectedUids;  // blocks of the selected device (highlighted vs dimmed)
    juce::CriticalSection touchLock;         // setTouch/setLayout (any thread) vs paint (message thread)
    std::atomic<bool>     dirty { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardView)
};
