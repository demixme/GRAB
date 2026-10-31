// DeviceInfoBar - the one persistent device-state strip, shared by the BLOCKS and RISE/GRAND pages
// so device info reads the same everywhere. Shows a battery icon + exact %, then the device name,
// firmware and transport. setInfo(...) fills it; clear() blanks it when nothing is connected.
//
// This replaces two inconsistent displays: the BLOCKS "battery 6%" baked into the status line, and
// the RISE "battery 6/7" in its own info label. Battery is now always an icon + % on both.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

class DeviceInfoBar  : public juce::Component
{
public:
    DeviceInfoBar() { setInterceptsMouseClicks (false, false); }

    void clear()
    {
        hasDevice = false;
        text = {};
        battery = -1.0f;
        repaint();
    }

    /** battery01: 0..1, or < 0 when unknown (the icon is then hidden). transport e.g. "USB" / "BLE".
        suffix is shown faintly at the end (e.g. "18 settings"). */
    void setInfo (const juce::String& name, const juce::String& fw, float battery01,
                  const juce::String& transport, const juce::String& suffix = {})
    {
        if (name.isEmpty() && fw.isEmpty() && battery01 < 0.0f) { clear(); return; }

        hasDevice = true;
        battery   = battery01;

        juce::StringArray parts;
        if (name.isNotEmpty())      parts.add (name);
        if (fw.isNotEmpty())        parts.add ("fw " + fw);
        if (transport.isNotEmpty()) parts.add (transport);
        if (suffix.isNotEmpty())    parts.add (suffix);
        text = parts.joinIntoString (juce::String::fromUTF8 ("   \xc2\xb7   "));   // " · "
        repaint();
    }

    bool isShowingDevice() const { return hasDevice; }

    void paint (juce::Graphics& g) override
    {
        if (! hasDevice) return;
        auto r = getLocalBounds().toFloat();
        const float h = r.getHeight();
        float x = r.getX();

        if (battery >= 0.0f)
        {
            const float bw = 25.0f, bh = juce::jmin (13.0f, h - 4.0f);
            juce::Rectangle<float> body (x, r.getCentreY() - bh * 0.5f, bw, bh);
            const float lvl = juce::jlimit (0.0f, 1.0f, battery);
            const juce::Colour col = lvl > 0.5f ? juce::Colour (0xff35c759)    // green
                                   : lvl > 0.2f ? juce::Colour (0xffff9f0a)    // amber
                                                : juce::Colour (0xffff3b30);   // red

            g.setColour (grab::theme::textPrimary.withAlpha (0.5f));
            g.drawRoundedRectangle (body, 2.5f, 1.3f);
            g.fillRoundedRectangle (body.getRight() + 1.0f, r.getCentreY() - 2.8f, 2.2f, 5.6f, 1.0f); // nub

            auto fill = body.reduced (2.2f);
            fill = fill.withWidth (juce::jmax (1.0f, fill.getWidth() * lvl));
            g.setColour (col);
            g.fillRoundedRectangle (fill, 1.3f);
            x += bw + 9.0f;

            g.setFont (juce::Font (juce::FontOptions (13.5f)).boldened());
            g.setColour (grab::theme::textPrimary);
            const auto pct = juce::String (juce::roundToInt (lvl * 100.0f)) + "%";
            g.drawText (pct, juce::Rectangle<float> (x, r.getY(), 42.0f, h), juce::Justification::centredLeft);
            x += 48.0f;
        }

        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.setColour (grab::theme::textPrimary.withAlpha (0.88f));
        g.drawText (text, juce::Rectangle<float> (x, r.getY(), r.getRight() - x, h),
                    juce::Justification::centredLeft, true);
    }

private:
    bool hasDevice = false;
    float battery = -1.0f;
    juce::String text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceInfoBar)
};
