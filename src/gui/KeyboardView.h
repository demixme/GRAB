// KeyboardView — a pseudo-3D ROLI keywave drawn at the top of the window, sized to the
// connected keyboard's range. Our own vector art (not ROLI's device renders).
//
// Each key is shaded as a rounded vertical cylinder (dark edges, bright crest keyline) to
// mimic the moulded-rubber keywave; black keys are raised ridges with a soft drop shadow.
// A per-family "smoothness" matches the hardware: Block keys are crisper, RISE softer, GRAND
// softest. Read-only for now; the plan is live 5D-touch rings and a DNA-chain join.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <atomic>
#include <map>

class KeyboardView  : public juce::Component,
                      private juce::Timer
{
public:
    enum class Family { block, rise, grand };

    KeyboardView() = default;

    void setOctaves (int o)   { if (octaves != o) { octaves = juce::jlimit (1, 7, o); repaint(); } }
    void setFamily  (Family f){ if (family  != f) { family  = f; repaint(); } }
    void setBlocks  (int n)   { n = juce::jlimit (1, 6, n); if (blocks != n) { blocks = n; repaint(); } }
    int  getOctaves() const   { return octaves; }

    // Live 5D-touch markers — white rings that grow with pressure. Positions are normalised
    // across the whole (chained) keyboard: x 0..1 left->right, y 0..1 top->bottom, z = pressure.
    // setTouch may be called from the MIDI thread; a 60 Hz timer coalesces the repaints.
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
    void setTouch (juce::uint64 id, float x, float y, float z, bool end)
    {
        const juce::ScopedLock sl (touchLock);
        if (end) touches.erase (id);
        else     touches[id] = { x, y, z, juce::Time::getMillisecondCounterHiRes() };
        dirty = true;
    }

    // Natural width at the target key size. The layout centres the view at min(this, available)
    // so keys stay a fixed scale on a wide window (narrow-range devices don't stretch), while
    // wide-range devices (RISE 2) still shrink to fit a narrow one. A DNA chain draws one body
    // per block, touching with a thin seam.
    int  preferredWidth() const { return blocks * octaves * 7 * kWhiteKeyW + 2 * kBodyMargin; }

    void paint (juce::Graphics& g) override
    {
        // Chained Seaboard Blocks snap edge-to-edge into one continuous keyboard, so they draw
        // as a single board of (blocks x octaves) octaves — keys run seamlessly across the join.
        const int octs = blocks * octaves;
        const int totalWhites = octs * 7;
        if (totalWhites <= 0) return;

        // Fixed key size, drawn at a constant scale and centred. Wider than the window simply
        // clips (resizing reveals more/less — the keys never change size).
        const float ww    = (float) kWhiteKeyW;
        const float bodyW = totalWhites * ww + 2.0f * kBodyMargin;
        auto full = getLocalBounds().toFloat();
        juce::Rectangle<float> r (full.getCentreX() - bodyW * 0.5f, full.getY() + 1.0f, bodyW, full.getHeight() - 2.0f);

        drawBoard (g, r, octs);

        // Mark each block boundary with only a thin seam line — the B of one block sits flush to
        // the C of the next (no extra gap), the way the hardware snaps together.
        if (blocks > 1)
        {
            auto keys = r.reduced (kBodyMargin, kKeyInsetV);
            g.setColour (juce::Colours::black.withAlpha (0.40f));
            for (int b = 1; b < blocks; ++b)
            {
                const float jx = keys.getX() + (float) (b * octaves * 7) * ww;
                g.fillRect (jx - 0.75f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f);
            }
        }

        // Live touch markers: a white ring + soft fill that grows with pressure, over the keys.
        std::vector<Live> snapshot;
        { const juce::ScopedLock sl (touchLock); snapshot.reserve (touches.size());
          for (auto& kv : touches) snapshot.push_back (kv.second); }
        if (! snapshot.empty())
        {
            auto keys = r.reduced (kBodyMargin, kKeyInsetV);
            for (auto& t : snapshot)
            {
                const float cx  = keys.getX() + juce::jlimit (0.0f, 1.0f, t.x) * keys.getWidth();
                const float cy  = keys.getY() + juce::jlimit (0.0f, 1.0f, t.y) * keys.getHeight();
                const float rad = 7.0f + juce::jlimit (0.0f, 1.0f, t.z) * 15.0f;
                juce::Rectangle<float> circ (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillEllipse (circ);
                g.setColour (juce::Colours::white.withAlpha (0.90f));
                g.drawEllipse (circ, 1.0f);
            }
        }
    }

private:
    void timerCallback() override
    {
        // Drop rings whose finger has stopped reporting — the touch-end can arrive late when many
        // fingers share the device's bandwidth, so a short staleness timeout keeps release snappy.
        {
            const juce::ScopedLock sl (touchLock);
            const double now = juce::Time::getMillisecondCounterHiRes();
            for (auto it = touches.begin(); it != touches.end();)
            {
                if (now - it->second.lastMs > kTouchTimeoutMs) { it = touches.erase (it); dirty = true; }
                else ++it;
            }
        }
        if (dirty.exchange (false)) repaint();
    }

    void drawBoard (juce::Graphics& g, juce::Rectangle<float> r, int octaveCount)
    {
        const int whitesPerOct = 7;
        const int totalWhites   = octaveCount * whitesPerOct;
        const float ww = (float) kWhiteKeyW;

        // Per-family feel: corner radius, how bright the crest catches light.
        const float radius  = family == Family::block ? 2.5f : family == Family::rise ? 4.0f : 5.5f;
        const float crestLum = family == Family::block ? 0.00f : family == Family::rise ? -0.06f : -0.10f;

        const juce::Colour whiteEdge  (0xff111318);
        const juce::Colour whiteCrest = juce::Colour (0xff3a3f4a).brighter (crestLum >= 0 ? crestLum : 0.0f)
                                            .darker  (crestLum < 0 ? -crestLum : 0.0f);
        const juce::Colour blackEdge  (0xff060708);
        const juce::Colour blackCrest (0xff22252c);

        // The keywave surface is a mid-grey — halfway between the cylinder's dark edge and light
        // crest. It fills the whole body and bleeds to the top/bottom edges; the raised keys rise
        // out of it, leaving flat glide strips above and below (both still sense glide / x-y).
        const juce::Colour flatSurface = whiteEdge.interpolatedWith (whiteCrest, 0.5f);
        g.setColour (flatSurface);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (grab::theme::accent.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 1.6f);

        auto keys = r.reduced (kBodyMargin, kKeyInsetV);
        const float blackH   = keys.getHeight() * (family == Family::block ? 0.42f : 0.50f);
        const float glideTop = keys.getHeight() * 0.11f;

        // A black key follows C, D, F, G, A (white-key indices 0,1,3,4,5).
        static const bool blackAfter[7] = { true, true, false, true, true, true, false };

        // White keys — rounded vertical cylinders (dark edge -> bright crest -> dark edge),
        // rising from the flat surface, with a soft base shadow for depth.
        for (int w = 0; w < totalWhites; ++w)
        {
            juce::Rectangle<float> col (keys.getX() + w * ww, keys.getY(), ww, keys.getHeight());
            auto k = col.withTrimmedTop (glideTop).withTrimmedBottom (glideTop).reduced (0.8f, 0.0f);

            juce::ColourGradient grad (whiteEdge, k.getX(), 0.0f, whiteEdge, k.getRight(), 0.0f, false);
            grad.addColour (0.5, whiteCrest);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (k, radius);

            // base shadow where the raised bump meets the lower flat strip
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (k.removeFromBottom (k.getHeight() * 0.14f), radius);
            // (no specular keyline on white keys — ROLI's render doesn't show one)
        }

        // Black keys — narrower raised ridges on top, with a soft drop shadow beneath.
        for (int o = 0; o < octaveCount; ++o)
            for (int i = 0; i < whitesPerOct; ++i)
                if (blackAfter[i])
                {
                    const int w = o * whitesPerOct + i;
                    const float cx = keys.getX() + (w + 1) * ww;       // boundary between this white and the next
                    const float bw = ww * 0.58f;
                    juce::Rectangle<float> blk (cx - bw * 0.5f, keys.getY() + glideTop, bw, blackH);

                    // drop shadow
                    g.setColour (juce::Colours::black.withAlpha (0.45f));
                    g.fillRoundedRectangle (blk.translated (0.0f, 3.0f).expanded (1.5f, 1.0f), radius + 1.0f);

                    // the ridge (cylinder shading)
                    juce::ColourGradient bg (blackEdge, blk.getX(), 0.0f, blackEdge, blk.getRight(), 0.0f, false);
                    bg.addColour (0.5, blackCrest);
                    g.setGradientFill (bg);
                    g.fillRoundedRectangle (blk, radius);

                    // keyline on the black key
                    g.setColour (juce::Colours::white.withAlpha (0.22f));
                    g.fillRoundedRectangle (blk.getCentreX() - 0.7f, blk.getY() + 3.0f, 1.4f, blk.getHeight() - 7.0f, 0.7f);
                }
    }

private:
    // Proportions taken from ROLI Dashboard's Seaboard Block render (2 octaves ~ 2.6:1 body,
    // slim ~1:5 white keys). Fixed scale: never stretches with the window.
    static constexpr int kWhiteKeyW  = 18;   // white-key width (px)
    static constexpr int kBodyMargin = 3;    // small side margin so the accent border frames the keys (not over them)
    static constexpr int kKeyInsetV  = 4;    // vertical inset of the keys from the body edge (keys run tall)

    int    octaves = 2;
    int    blocks  = 1;     // DNA-chained bodies drawn side-by-side (identical-type cluster)
    Family family  = Family::rise;

    static constexpr double kTouchTimeoutMs = 140.0;   // drop a ring this long after its last update
    struct Live { float x, y, z; double lastMs; };     // normalised position + pressure + last-seen
    std::map<juce::uint64, Live> touches;    // active touches, keyed by block+finger id
    juce::CriticalSection        touchLock;  // setTouch (MIDI thread) vs paint (message thread)
    std::atomic<bool>            dirty { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardView)
};
