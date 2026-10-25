// CurveComponent — a response-curve graph for one 5D dimension (Strike/Glide/
// Slide/Press/Lift), in the ROLI-ish colour for that dimension.
//
// Honest scope: the device config exposes a single *sensitivity* value (0-127)
// plus optional min/max — NOT multi-point curve data. So this draws a
// representative response curve from (sensitivity, min, max) and lets you drag
// vertically to set the sensitivity. It resembles ROLI's curve panels; it is not
// a full bezier editor (that data isn't in the config protocol).

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
                    juce::int32 sensitivity, juce::int32 minVal, juce::int32 maxVal, ChangeFn onChange)
        : name (std::move (nameToUse)), colour (colourToUse), id (sensId),
          sens (sensitivity), mn (minVal), mx (maxVal), change (std::move (onChange))
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
        auto plot = b.reduced (2).toFloat();

        // panel
        g.setColour (juce::Colour (0xff20242a));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        if (changed)
        {
            g.setColour (juce::Colours::orange);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.5f);
        }

        // grid
        g.setColour (juce::Colour (0x18ffffff));
        for (int i = 1; i < 4; ++i)
        {
            const float fx = plot.getX() + plot.getWidth()  * (i / 4.0f);
            const float fy = plot.getY() + plot.getHeight() * (i / 4.0f);
            g.drawVerticalLine   ((int) fx, plot.getY(), plot.getBottom());
            g.drawHorizontalLine ((int) fy, plot.getX(), plot.getRight());
        }

        // curve
        juce::Path path;
        const int N = 48;
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / (float) N;
            const float out = outputFraction (t);
            const float px = plot.getX() + t * plot.getWidth();
            const float py = plot.getBottom() - out * plot.getHeight();
            if (i == 0) path.startNewSubPath (px, py);
            else        path.lineTo (px, py);
        }
        g.setColour (colour.withAlpha (0.18f));
        {
            juce::Path fill (path);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.fillPath (fill);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));

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
    float outputFraction (float t) const
    {
        // exponent: higher sensitivity => more convex (reaches max sooner).
        const double e = std::pow (2.0, (64.0 - (double) sens) / 40.0);
        const double shaped = std::pow ((double) juce::jlimit (0.0f, 1.0f, t), e);
        const double out = ((double) mn + ((double) mx - (double) mn) * shaped) / 127.0;
        return (float) juce::jlimit (0.0, 1.0, out);
    }

    void setFromMouse (const juce::MouseEvent& e)
    {
        const float h = (float) juce::jmax (1, getHeight());
        const int v = juce::jlimit (0, 127, (int) std::lround ((1.0f - e.position.y / h) * 127.0f));
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
