// Stepper — Fred's `.st`: a minus button, a centred value (+ optional muted unit annotation),
// and a plus button, in a sunken `well` rounded control. The value also scrubs with a vertical
// drag. Used for the small integer fields (member channels, octave, transpose, slide CC, ...).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>
#include <utility>

namespace grab
{

class Stepper  : public juce::Component
{
public:
    Stepper() { setWantsKeyboardFocus (true); }

    std::function<void()>                 onChange;   // fired on any value change (step / scrub / keys)
    std::function<juce::String (int)>     unitFn;     // optional annotation from the value (e.g. "ch 2-16")

    void setRange (int lo, int hi)        { min = lo; max = hi; setValue (value, false); }
    void setValue (int v, bool notify)
    {
        v = juce::jlimit (min, max, v);
        if (v == value) { repaint(); return; }
        value = v;
        repaint();
        if (notify && onChange) onChange();
    }
    int  getValue() const                 { return value; }
    void setUnitFn (std::function<juce::String (int)> f) { unitFn = std::move (f); repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        auto rf = getLocalBounds().toFloat();
        g.setColour (hover ? wellHover : well);
        g.fillRoundedRectangle (rf, radius::control);

        auto full  = getLocalBounds();
        auto left  = full.removeFromLeft (btnW);
        auto right = full.removeFromRight (btnW);
        drawSign (g, left,  false, value > min);
        drawSign (g, right, true,  value < max);

        const juce::String val  = juce::String (value);
        const juce::String unit = unitFn ? unitFn (value) : juce::String();
        const juce::Font vf = type::font (type::Role::control);
        const juce::Font uf = type::font (type::Role::caption);
        const int vw = juce::GlyphArrangement::getStringWidthInt (vf, val);
        const int uw = unit.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (uf, unit);
        const int gap = unit.isEmpty() ? 0 : 6;
        int x = full.getCentreX() - (vw + gap + uw) / 2;

        g.setFont (vf);
        g.setColour (ink);
        g.drawText (val, x, full.getY(), vw + 2, full.getHeight(), juce::Justification::centredLeft);
        if (! unit.isEmpty())
        {
            g.setFont (uf);
            g.setColour (label);
            g.drawText (unit, x + vw + gap, full.getY(), uw + 2, full.getHeight(), juce::Justification::centredLeft);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;   // let ConfigRow handle right-click reset
        const int w = getWidth();
        if (e.x < btnW)              { setValue (value - 1, true); scrubbing = false; }
        else if (e.x > w - btnW)     { setValue (value + 1, true); scrubbing = false; }
        else                         { scrubbing = true; scrubStart = value; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! scrubbing) return;
        const float rate = e.mods.isCommandDown() || e.mods.isCtrlDown() ? 0.15f : 0.5f;
        setValue (scrubStart - juce::roundToInt ((float) e.getDistanceFromDragStartY() * rate), true);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::upKey   || k == juce::KeyPress::rightKey) { setValue (value + 1, true); return true; }
        if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey)  { setValue (value - 1, true); return true; }
        return false;
    }

private:
    void drawSign (juce::Graphics& g, juce::Rectangle<int> cell, bool plus, bool enabled)
    {
        using namespace grab::theme;
        g.setColour (enabled ? ink2 : disabled);
        const auto c = cell.getCentre().toFloat();
        const float r = 4.0f;
        g.drawLine (c.x - r, c.y, c.x + r, c.y, 1.5f);
        if (plus) g.drawLine (c.x, c.y - r, c.x, c.y + r, 1.5f);
    }

    int min = 0, max = 127, value = 0;
    int btnW = 32;
    bool hover = false, scrubbing = false;
    int scrubStart = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stepper)
};

} // namespace grab
