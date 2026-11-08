// LiveKeysStrip - Fred's "Live keys" view: a wide keyboard strip that shows where and how hard you
// play. Each live touch is a ring (grows with pressure, follows the finger across = glide and along
// the key = slide), fed from the device's performance MIDI - the same stream that drives the small
// device graphic. It overlays the settings area (the top band and the 5D graph stay visible).
//
// Touches come in as setTouch(fingerId, nx, ny, z): nx 0..1 across the whole keyboard, ny 0..1 along
// the key (slide), z 0..1 pressure. It renders only (no device I/O); mouse press-drag to play is a
// later step.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <map>

class LiveKeysStrip  : public juce::Component,
                       private juce::Timer
{
public:
    LiveKeysStrip() { startTimerHz (60); }

    /** The drawn keyboard span: MIDI note of the lowest key, and how many keys. */
    void setRange (int lowNote, int keyCount)
    {
        if (lowNote == low && keyCount == count) return;
        low = lowNote; count = juce::jmax (1, keyCount);
        repaint();
    }

    /** A live touch. nx across the keyboard (0..1), ny along the key / slide (0..1), z pressure
        (0..1). end removes it. fingerId is any stable per-finger key. */
    void setTouch (int fingerId, float nx, float ny, float z, bool end)
    {
        if (end) { touches.erase (fingerId); return; }
        auto& t = touches[fingerId];
        t.x = juce::jlimit (0.0f, 1.0f, nx);
        t.y = juce::jlimit (0.0f, 1.0f, ny);
        t.z = juce::jlimit (0.0f, 1.0f, z);
        if (! t.seen) { t.dx = t.x; t.dy = t.y; t.dz = t.z; t.seen = true; }
    }

    void clearTouches() { touches.clear(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        auto r = getLocalBounds().toFloat();
        g.setColour (display);
        g.fillRoundedRectangle (r, radius::tile);

        auto keysR = getLocalBounds().reduced (16, 14);
        const int labelH = 18;
        auto labels = keysR.removeFromBottom (labelH);
        const float x0 = (float) keysR.getX(), w = (float) keysR.getWidth();
        const float top = (float) keysR.getY(), h = (float) keysR.getHeight();

        // White-key boundaries as thin verticals, with a C label at each octave.
        const int whites = whiteCount();
        g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
        int wIdx = 0;
        for (int n = low; n < low + count; ++n)
        {
            if (isBlack (n)) continue;
            const float fx = x0 + w * (float) wIdx / (float) juce::jmax (1, whites);
            g.setColour (grid);
            g.drawVerticalLine (juce::roundToInt (fx), top, top + h);
            if (n % 12 == 0)   // C
            {
                g.setColour (label);
                g.drawText ("C" + juce::String (n / 12 - 1),
                            juce::Rectangle<float> (fx + 2.0f, (float) labels.getY(), 34.0f, (float) labelH),
                            juce::Justification::centredLeft);
            }
            ++wIdx;
        }
        g.setColour (grid);
        g.drawVerticalLine (keysR.getRight(), top, top + h);

        // Touch rings: Press-hue ring sized by pressure, following x (glide) and y (slide).
        for (auto& kv : touches)
        {
            auto& t = kv.second;
            const float cx = x0 + t.dx * w;
            const float cy = top + (1.0f - t.dy) * h * 0.78f + h * 0.11f;   // slide runs up the key
            const float rad = 14.0f + t.dz * 46.0f;
            g.setColour (press.withAlpha (0.28f));
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (press);
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 2.0f);
            g.setColour (strike);
            g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        }

        if (touches.empty())
        {
            g.setColour (label);
            g.setFont (grab::theme::type::font (grab::theme::type::Role::body));
            g.drawText ("Play the device, or watch your touches here.",
                        keysR.toFloat(), juce::Justification::centred);
        }
    }

private:
    struct Live { float x = 0, y = 0.5f, z = 0, dx = 0, dy = 0.5f, dz = 0; bool seen = false; };

    static bool isBlack (int note)
    {
        switch (((note % 12) + 12) % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; }
    }
    int whiteCount() const
    {
        int w = 0;
        for (int n = low; n < low + count; ++n) if (! isBlack (n)) ++w;
        return juce::jmax (1, w);
    }

    void timerCallback() override
    {
        bool moved = false;
        for (auto& kv : touches)
        {
            auto& t = kv.second;
            t.dx += (t.x - t.dx) * 0.35f;
            t.dy += (t.y - t.dy) * 0.35f;
            t.dz += (t.z - t.dz) * 0.35f;
            if (std::abs (t.x - t.dx) + std::abs (t.y - t.dy) + std::abs (t.z - t.dz) > 0.001f) moved = true;
        }
        if (moved) repaint();
    }

    int low = 48, count = 25;   // C3, two octaves (sensible default until setRange)
    std::map<int, Live> touches;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveKeysStrip)
};
