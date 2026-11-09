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

    /** strip = the single-line top-bar form; dashboard = the vertical top-row tile (name heading,
        then Connection / Battery / Firmware / Settings rows). */
    enum class Style { strip, dashboard };
    void setStyle (Style s) { if (style != s) { style = s; repaint(); } }

    /** battery01: 0..1, or < 0 when unknown (the icon is then hidden). transport e.g. "USB" / "BLE".
        suffix is shown faintly at the end (e.g. "18 settings"). */
    void setInfo (const juce::String& name, const juce::String& fw, float battery01,
                  const juce::String& transport, const juce::String& suffix = {})
    {
        if (name.isEmpty() && fw.isEmpty() && battery01 < 0.0f) { clear(); return; }

        hasDevice   = true;
        battery     = battery01;
        fName       = name;
        fFirmware   = fw;
        fTransport  = transport;
        fSuffix     = suffix;

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
        if (style == Style::dashboard) { paintDashboard (g); return; }

        auto r = getLocalBounds().toFloat();
        const float h = r.getHeight();
        float x = r.getX();

        if (battery >= 0.0f)
        {
            const float bw = 25.0f, bh = juce::jmin (13.0f, h - 4.0f);
            juce::Rectangle<float> body (x, r.getCentreY() - bh * 0.5f, bw, bh);
            x += drawBattery (g, body) + 9.0f;

            g.setFont (grab::theme::type::font (grab::theme::type::Role::title));
            g.setColour (grab::theme::ink);
            const auto pct = juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, battery) * 100.0f)) + "%";
            g.drawText (pct, juce::Rectangle<float> (x, r.getY(), 42.0f, h), juce::Justification::centredLeft);
            x += 48.0f;
        }

        g.setFont (grab::theme::type::font (grab::theme::type::Role::body));
        g.setColour (grab::theme::ink.withAlpha (0.88f));
        g.drawText (text, juce::Rectangle<float> (x, r.getY(), r.getRight() - x, h),
                    juce::Justification::centredLeft, true);
    }

private:
    // The vertical dashboard tile: device name as a heading, then labelled rows.
    void paintDashboard (juce::Graphics& g)
    {
        using namespace grab::theme;
        auto r = getLocalBounds();

        // The device name is the dropdown heading (DeviceNameButton), placed above this tile by the
        // layout, so the dashboard body is just the labelled facts.
        // Fred's .dr: key on the left (label colour), value right-aligned to the card edge (ink2),
        // both at caption size - and on the battery row the pill sits just left of the %.
        const int rowH = 20;
        const juce::Font capFont = type::font (type::Role::caption);
        auto row = [&] (const juce::String& key, const juce::String& val, bool batteryRow = false)
        {
            auto line = r.removeFromTop (rowH);
            g.setColour (label);
            g.setFont (capFont);
            g.drawText (key, line.removeFromLeft (84), juce::Justification::centredLeft);

            const int valW = juce::GlyphArrangement::getStringWidthInt (capFont, val) + 2;
            auto valR = line.removeFromRight (valW);
            g.setColour (ink2);
            g.setFont (capFont);
            g.drawText (val, valR, juce::Justification::centredRight, true);

            if (batteryRow && battery >= 0.0f)
            {
                line.removeFromRight (6);
                auto b = line.removeFromRight (25).toFloat().withSizeKeepingCentre (25.0f, 12.0f);
                drawBattery (g, b);
            }
        };

        if (fTransport.isNotEmpty()) row ("Connection", fTransport);
        if (battery >= 0.0f)
            row ("Battery", juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, battery) * 100.0f)) + "%", true);
        if (fFirmware.isNotEmpty()) row ("Firmware", fFirmware);
        if (fSuffix.isNotEmpty())   row ("Settings", fSuffix.upToFirstOccurrenceOf (" ", false, false));
    }

    // Draws the battery pill+nub in `body`; returns its drawn width (for the strip layout).
    float drawBattery (juce::Graphics& g, juce::Rectangle<float> body)
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, battery);
        const juce::Colour col = lvl > 0.5f ? juce::Colour (0xff35c759)    // green
                               : lvl > 0.2f ? juce::Colour (0xffff9f0a)    // amber
                                            : juce::Colour (0xffff3b30);   // red
        g.setColour (grab::theme::ink.withAlpha (0.5f));
        g.drawRoundedRectangle (body, 2.5f, 1.3f);
        g.fillRoundedRectangle (body.getRight() + 1.0f, body.getCentreY() - 2.8f, 2.2f, 5.6f, 1.0f); // nub
        auto fill = body.reduced (2.2f);
        fill = fill.withWidth (juce::jmax (1.0f, fill.getWidth() * lvl));
        g.setColour (col);
        g.fillRoundedRectangle (fill, 1.3f);
        return body.getWidth();
    }

    Style style = Style::strip;
    bool hasDevice = false;
    float battery = -1.0f;
    juce::String text;
    juce::String fName, fFirmware, fTransport, fSuffix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceInfoBar)
};
