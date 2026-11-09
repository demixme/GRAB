// IconButton - Fred's .ibtn: a borderless 16px icon in a square hit area, ink2 that brightens to ink
// on hover with a rounded raisedHover pill. Used for the controls-card action row (live view, read
// from device, tools).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace grab
{

class IconButton  : public juce::Button
{
public:
    enum class Icon { liveView, reload, tools };

    explicit IconButton (Icon ic) : juce::Button ({}), icon (ic) {}

    void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
    {
        using namespace grab::theme;
        auto r = getLocalBounds().toFloat();
        const bool hot = isOver || isDown || isMouseButtonDown();
        if (hot)
        {
            g.setColour (raisedHover);
            g.fillRoundedRectangle (r.reduced (2.0f), 8.0f);
        }

        const juce::Colour c = ! isEnabled() ? disabled : (hot ? ink : ink2);
        auto box = r.withSizeKeepingCentre (16.0f, 16.0f);
        const auto t = juce::AffineTransform::translation (box.getX(), box.getY());
        const juce::PathStrokeType stroke (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path p;
        if      (icon == Icon::liveView) p = juce::Drawable::parseSVGPath ("M1.5 8h2.5l2-4.5 3.5 9 2-4.5h3");
        else if (icon == Icon::reload)   p = juce::Drawable::parseSVGPath ("M13 8a5 5 0 11-1.5-3.6M13 2.5v3h-3");
        else                             p = juce::Drawable::parseSVGPath ("M2.5 4.5h11M2.5 11.5h11");

        g.setColour (c);
        g.strokePath (p, stroke, t);

        if (icon == Icon::tools)   // two knobs sitting on the slider lines
            for (auto knob : { juce::Point<float> (6.0f, 4.5f), juce::Point<float> (10.0f, 11.5f) })
            {
                auto e = juce::Rectangle<float> (3.6f, 3.6f).withCentre (knob).transformedBy (t);
                g.setColour (chrome);  g.fillEllipse (e);
                g.setColour (c);       g.drawEllipse (e, 1.5f);
            }
    }

private:
    Icon icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

} // namespace grab
