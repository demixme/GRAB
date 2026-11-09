// DeviceNameButton — Fred's .devname: the connected device's name shown as the dashboard tile's
// heading, doubling as the dropdown that opens the rich device menu. Borderless; the name is the
// title weight in ink, followed by a small down chevron; hovering lights a rounded pill behind it.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace grab
{

class DeviceNameButton  : public juce::Button
{
public:
    DeviceNameButton() : juce::Button ({}) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace grab::theme;
        g.setFont (type::font (type::Role::title));
        const juce::String name = getButtonText();
        const int nameW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name);

        const int padL = 2, gap = 8, chevW = 10, padR = 8;
        auto pill = getLocalBounds();
        pill = pill.withWidth (juce::jmin (pill.getWidth(), padL + nameW + gap + chevW + padR));

        if (over || down || isMouseButtonDown())
        {
            g.setColour (raisedHover);
            g.fillRoundedRectangle (pill.toFloat(), 8.0f);
        }

        auto a = pill;
        a.removeFromLeft (padL);
        g.setColour (ink);
        g.drawText (name, a.removeFromLeft (nameW + 2), juce::Justification::centredLeft);
        a.removeFromLeft (gap);

        auto cv = a.removeFromLeft (chevW).toFloat();
        const float cx = cv.getCentreX(), cy = cv.getCentreY() + 1.0f, w = 3.0f, h = 2.0f;
        juce::Path chev;
        chev.startNewSubPath (cx - w, cy - h);
        chev.lineTo          (cx,     cy + h);
        chev.lineTo          (cx + w, cy - h);
        g.setColour (label);
        g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceNameButton)
};

} // namespace grab
