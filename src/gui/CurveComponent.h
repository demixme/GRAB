// CurveComponent — a response-curve graph for one 5D dimension (Strike/Glide/
// Slide/Press/Lift), in the ROLI-ish colour for that dimension.
//
// Honest scope: the device config exposes a single *sensitivity* value (0-127)
// plus optional min/max — NOT multi-point curve data. The shapes are drawn the
// way ROLI Dashboard 4.7.2 draws them, measured from its RISE page at sensitivities
// 0 to 127 (s = sensitivity / 127 below):
//   Strike / Press / Lift  quadratic from (0, min) to (s, max), flat after that
//   Slide                  S-curve around x = 0.5, about s wide
//   Glide                  the same turned on its side: steep at both edges
// Towards 127 every curve straightens into the diagonal. At 0 Strike is a line at
// max, Lift a line at min, Press and Slide draw nothing and Glide shows a keyboard.
// Drag vertically to set the sensitivity.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

class CurveComponent  : public juce::Component
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    CurveComponent (juce::String nameToUse, juce::Colour colourToUse, juce::uint32 sensId,
                    juce::int32 sensitivity, juce::int32 minVal, juce::int32 maxVal, ChangeFn onChange,
                    bool showNodes = true)
        : name (std::move (nameToUse)), colour (colourToUse), id (sensId),
          sens (sensitivity), mn (minVal), mx (maxVal), change (std::move (onChange)), nodes (showNodes)
    {
    }

    juce::uint32 getId()    const { return id; }
    juce::int32  getValue() const { return sens; }
    void setSensitivity (juce::int32 v) { sens = juce::jlimit (0, 127, v); repaint(); }
    void setChanged (bool c) { if (changed != c) { changed = c; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced (4);
        auto header = b.removeFromTop (16);
        b.removeFromTop (4);
        auto bar = b.removeFromLeft (10).toFloat();
        const auto plot = plotArea();

        // panel
        g.setColour (juce::Colour (0xff20242a));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        if (changed)
        {
            g.setColour (juce::Colours::orange);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.5f);
        }

        // sensitivity bar
        g.setColour (juce::Colour (0x14ffffff));
        g.fillRoundedRectangle (bar, 5.0f);
        auto fill = bar.reduced (2.0f);
        fill = fill.removeFromBottom (fill.getHeight() * (float) sens / 127.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 3.0f);

        // grid
        g.setColour (juce::Colour (0x18ffffff));
        for (int i = 1; i < 8; ++i)
        {
            const float fx = plot.getX() + plot.getWidth()  * (i / 8.0f);
            const float fy = plot.getY() + plot.getHeight() * (i / 8.0f);
            g.drawVerticalLine   ((int) fx, plot.getY(), plot.getBottom());
            g.drawHorizontalLine ((int) fy, plot.getX(), plot.getRight());
        }

        // curve
        const auto toPlot = [&plot] (juce::Point<float> p)
        {
            return juce::Point<float> (plot.getX() + p.x * plot.getWidth(), plot.getBottom() - p.y * plot.getHeight());
        };
        const auto path = curvePath ((float) mn / 127.0f, (float) mx / 127.0f);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f),
                      juce::AffineTransform::scale (plot.getWidth(), -plot.getHeight())
                          .translated (plot.getX(), plot.getBottom()));

        if (nodes && sens > 0)
            for (auto p : { toPlot ({ 0.0f, (float) mn / 127.0f }), toPlot ({ 1.0f, (float) mx / 127.0f }) })
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));

        // Glide end stops: keyboard (no glide) at 0, notes (free pitch) at 127.
        if (kind == Kind::glide && (sens == 0 || sens == 127))
        {
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawText (sens == 0 ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb9"))
                                  : juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xab")),
                        plot.withSize (24.0f, 18.0f).translated (4.0f, 2.0f), juce::Justification::centredLeft);
        }

        // label + value
        g.setColour (colour);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (name, header, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (juce::String (sens), header, juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override { setFromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }

private:
    enum class Kind { velocity, slide, glide };   // velocity: Strike, Press, Lift

    static Kind kindFor (const juce::String& n)
    {
        return n == "Slide" ? Kind::slide : n == "Glide" ? Kind::glide : Kind::velocity;
    }

    juce::Rectangle<float> plotArea() const
    {
        auto b = getLocalBounds().reduced (4);
        b.removeFromTop (20);
        b.removeFromLeft (18);
        return b.reduced (5).toFloat();
    }

    // Path in unit coordinates (x right, y up), see the header comment.
    juce::Path curvePath (float lo, float hi) const
    {
        using P = juce::Point<float>;
        juce::Path path;
        const float s = (float) sens / 127.0f;
        const float straight = juce::jlimit (0.0f, 1.0f, ((float) sens - 104.0f) / 17.0f);
        const auto lerp = [] (P a, P b, float f) { return a + (b - a) * f; };

        if (kind == Kind::velocity)
        {
            if (sens == 0)
            {
                if (name == "Press") return path;
                const float y = name == "Lift" ? lo : hi;
                path.startNewSubPath ({ 0.0f, y });
                path.lineTo ({ 1.0f, y });
                return path;
            }
            const float end = juce::jmax (0.01f, s);
            path.startNewSubPath ({ 0.0f, lo });
            path.quadraticTo ({ end * (0.55f + 0.45f * straight), hi }, { end, hi });
            path.lineTo ({ 1.0f, hi });
            return path;
        }

        if (kind == Kind::slide && sens == 0)
            return path;

        // Slide: S-curve from (0.5 - w, 0) to (0.5 + w, 1) with flat ends.
        // Glide: from (0, 0.5 - w) to (1, 0.5 + w) with vertical ends.
        const bool slide = kind == Kind::slide;
        const float v = (float) juce::jmax (1, (int) sens);
        const float w = slide ? 0.47f * s : juce::jmin (0.5f, 0.02f + v / 160.0f);
        const float a = slide ? 0.89f * w : w * (1.7f - 0.006f * v);

        const P p0 = slide ? P (0.5f - w, 0.0f) : P (0.0f, 0.5f - w);
        const P p3 = slide ? P (0.5f + w, 1.0f) : P (1.0f, 0.5f + w);
        const P p1 = lerp (slide ? P (p0.x + a, 0.0f) : P (0.0f, p0.y + a), lerp (p0, p3, 1.0f / 3.0f), straight);
        const P p2 = lerp (slide ? P (p3.x - a, 1.0f) : P (1.0f, p3.y - a), lerp (p0, p3, 2.0f / 3.0f), straight);

        path.startNewSubPath ({ 0.0f, 0.0f });
        path.lineTo (p0);
        path.cubicTo (p1, p2, p3);
        path.lineTo ({ 1.0f, 1.0f });
        return path;
    }

    void setFromMouse (const juce::MouseEvent& e)
    {
        const auto plot = plotArea();
        const float f = (plot.getBottom() - e.position.y) / juce::jmax (1.0f, plot.getHeight());
        const int v = juce::jlimit (0, 127, (int) std::lround (f * 127.0f));
        if (v != sens)
        {
            sens = v;
            repaint();
            if (change != nullptr)
                change (id, sens);
        }
    }

    juce::String name;
    juce::Colour colour;
    juce::uint32 id;
    juce::int32 sens, mn, mx;
    ChangeFn change;
    bool nodes;
    Kind kind = kindFor (name);
    bool changed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveComponent)
};

//==============================================================================
/** A horizontal row of CurveComponents (the 5D dimensions across the top). */
class CurveRow  : public juce::Component
{
public:
    CurveRow() = default;

    CurveComponent* add (std::unique_ptr<CurveComponent> c)
    {
        auto* p = c.get();
        addAndMakeVisible (*c);
        curves.push_back (std::move (c));
        return p;
    }

    void resized() override
    {
        const int n = (int) curves.size();
        if (n == 0) return;
        const int w = getWidth() / n;
        for (int i = 0; i < n; ++i)
            curves[(size_t) i]->setBounds (i * w, 0, w - 6, getHeight());
    }

    std::vector<std::unique_ptr<CurveComponent>> curves;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveRow)
};
