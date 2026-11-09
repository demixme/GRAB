// PresetBar — Fred's .pbar: a rounded panel pill holding a prev chevron, the current preset name
// (which opens the full list) and a next chevron, with an "Edited" badge when the loaded preset has
// unsaved changes. A stepper around a dropdown: the arrows step through presets, the name opens the
// list.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>

namespace grab
{

class PresetBar  : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    PresetBar() = default;

    std::function<void()> onPrev, onNext, onOpen;

    void setName (const juce::String& n, bool placeholder)
    {
        if (name == n && placeholderName == placeholder) return;
        name = n; placeholderName = placeholder; repaint();
    }
    void setEdited (bool e) { if (edited != e) { edited = e; repaint(); } }

    void resized() override
    {
        auto in = getLocalBounds().reduced (2);
        prevR   = in.removeFromLeft (30);
        nextR   = in.removeFromRight (30);
        centreR = in;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), (float) radius::control);

        auto hoverPill = [&] (juce::Rectangle<int> z) { g.setColour (raisedHover); g.fillRoundedRectangle (z.toFloat(), 8.0f); };
        if (hoverZone == 0) hoverPill (prevR);
        if (hoverZone == 1) hoverPill (centreR);
        if (hoverZone == 2) hoverPill (nextR);

        drawArrow (g, prevR, -1);
        drawArrow (g, nextR, +1);

        auto c = centreR.reduced (10, 0);

        // down chevron on the right of the name
        auto chevZone = c.removeFromRight (12);
        { const float cx = chevZone.getCentreX(), cy = chevZone.getCentreY() + 1.0f, w = 3.0f, h = 2.0f;
          juce::Path p; p.startNewSubPath (cx - w, cy - h); p.lineTo (cx, cy + h); p.lineTo (cx + w, cy - h);
          g.setColour (label); g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        c.removeFromRight (6);

        if (edited)
        {
            const juce::Font cap = type::font (type::Role::caption);
            const int pw = juce::GlyphArrangement::getStringWidthInt (cap, "Edited") + 14;
            auto ep = c.removeFromRight (pw).withSizeKeepingCentre (pw, 18);
            g.setColour (segmentOn); g.fillRoundedRectangle (ep.toFloat(), 9.0f);
            g.setColour (ink2); g.setFont (cap);
            g.drawText ("Edited", ep, juce::Justification::centred);
            c.removeFromRight (6);
        }

        g.setColour (placeholderName ? label : ink);
        g.setFont (type::font (type::Role::body));
        g.drawText (name, c, juce::Justification::centredLeft, true);
    }

    void mouseMove  (const juce::MouseEvent& e) override { updateHover (e.getPosition()); }
    void mouseEnter (const juce::MouseEvent& e) override { updateHover (e.getPosition()); }
    void mouseExit  (const juce::MouseEvent&) override   { if (hoverZone != -1) { hoverZone = -1; repaint(); } }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto p = e.getPosition();
        if      (prevR.contains (p))   { if (onPrev) onPrev(); }
        else if (nextR.contains (p))   { if (onNext) onNext(); }
        else if (centreR.contains (p)) { if (onOpen) onOpen(); }
    }

private:
    void updateHover (juce::Point<int> p)
    {
        const int z = prevR.contains (p) ? 0 : nextR.contains (p) ? 2 : centreR.contains (p) ? 1 : -1;
        if (z != hoverZone) { hoverZone = z; repaint(); }
    }

    void drawArrow (juce::Graphics& g, juce::Rectangle<int> z, int dir)
    {
        const float cx = z.getCentreX(), cy = z.getCentreY(), w = 2.5f, h = 3.5f;
        juce::Path p;
        if (dir < 0) { p.startNewSubPath (cx + w, cy - h); p.lineTo (cx - w, cy); p.lineTo (cx + w, cy + h); }
        else         { p.startNewSubPath (cx - w, cy - h); p.lineTo (cx + w, cy); p.lineTo (cx - w, cy + h); }
        g.setColour (grab::theme::ink2);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::String name { "Preset..." };
    bool placeholderName = true, edited = false;
    juce::Rectangle<int> prevR, nextR, centreR;
    int hoverZone = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace grab
