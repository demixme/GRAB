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

class KeyboardView  : public juce::Component
{
public:
    enum class Family { block, rise, grand };

    KeyboardView() = default;

    void setOctaves (int o)   { if (octaves != o) { octaves = juce::jlimit (1, 7, o); repaint(); } }
    void setFamily  (Family f){ if (family  != f) { family  = f; repaint(); } }
    int  getOctaves() const   { return octaves; }

    // Natural width at the target key size. The layout centres the view at min(this, available)
    // so keys stay a fixed scale on a wide window (narrow-range devices don't stretch), while
    // wide-range devices (RISE 2) still shrink to fit a narrow one.
    int  preferredWidth() const { return octaves * 7 * kWhiteKeyW + 2 * kBodyMargin; }

    void paint (juce::Graphics& g) override
    {
        const int whitesPerOct = 7;
        const int totalWhites   = octaves * whitesPerOct;
        if (totalWhites <= 0) return;

        // Fixed key size: the whole keyboard is drawn at a constant scale and centred. If it's
        // wider than the window it simply clips (resizing reveals more/less — the keys never
        // change size); if narrower, it sits with margins.
        const float ww     = (float) kWhiteKeyW;
        const float bodyW  = totalWhites * ww + 2.0f * kBodyMargin;
        auto full = getLocalBounds().toFloat();
        juce::Rectangle<float> r (full.getCentreX() - bodyW * 0.5f, full.getY() + 1.0f, bodyW, full.getHeight() - 2.0f);

        // Body + accent border.
        g.setColour (grab::theme::windowBg.darker (0.5f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (grab::theme::accent.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 1.6f);

        auto keys = r.reduced (kBodyMargin, 14.0f);

        // Per-family feel: corner radius, how bright the crest catches light, black-key height.
        const float radius  = family == Family::block ? 2.5f : family == Family::rise ? 4.0f : 5.5f;
        const float crestLum = family == Family::block ? 0.00f : family == Family::rise ? -0.06f : -0.10f;
        const float blackH   = keys.getHeight() * (family == Family::block ? 0.60f : 0.56f);

        const juce::Colour whiteEdge  (0xff111318);
        const juce::Colour whiteCrest = juce::Colour (0xff3a3f4a).brighter (crestLum >= 0 ? crestLum : 0.0f)
                                            .darker  (crestLum < 0 ? -crestLum : 0.0f);
        const juce::Colour blackEdge  (0xff060708);
        const juce::Colour blackCrest (0xff22252c);

        // A black key follows C, D, F, G, A (white-key indices 0,1,3,4,5).
        static const bool blackAfter[7] = { true, true, false, true, true, true, false };

        // White keys — rounded vertical cylinders (dark edge -> bright crest -> dark edge),
        // with a thin specular keyline and a soft base shadow for depth.
        for (int w = 0; w < totalWhites; ++w)
        {
            juce::Rectangle<float> key (keys.getX() + w * ww, keys.getY(), ww, keys.getHeight());
            auto k = key.reduced (0.8f, 0.0f);

            juce::ColourGradient grad (whiteEdge, k.getX(), 0.0f, whiteEdge, k.getRight(), 0.0f, false);
            grad.addColour (0.5, whiteCrest);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (k, radius);

            // base shadow (keys tilt away toward the bottom)
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (k.removeFromBottom (k.getHeight() * 0.18f), radius);

            // specular keyline at the crest
            g.setColour (juce::Colours::white.withAlpha (family == Family::block ? 0.40f : 0.30f));
            g.fillRoundedRectangle (key.getCentreX() - 0.8f, key.getY() + key.getHeight() * 0.08f,
                                    1.6f, key.getHeight() * 0.62f, 0.8f);
        }

        // Black keys — narrower raised ridges on top, with a soft drop shadow beneath.
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < whitesPerOct; ++i)
                if (blackAfter[i])
                {
                    const int w = o * whitesPerOct + i;
                    const float cx = keys.getX() + (w + 1) * ww;       // boundary between this white and the next
                    const float bw = ww * 0.60f;
                    juce::Rectangle<float> blk (cx - bw * 0.5f, keys.getY(), bw, blackH);

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
    static constexpr int kWhiteKeyW = 26;   // fixed white-key width (px) — Dashboard-ish proportion; never scales
    static constexpr int kBodyMargin = 18;

    int    octaves = 2;
    Family family  = Family::rise;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardView)
};
