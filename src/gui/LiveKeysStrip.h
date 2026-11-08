// LiveKeysStrip - Fred's keystroke strip: a keyboard span that shows where and how hard you play.
// Black keys are short ticks, each C is labelled (shifted by the device's octave + transpose so the
// labels read C3/C4/C5 correctly), and every live touch is a Press-hue ring that grows with pressure
// and follows the finger across (glide) and along the key (slide). A readout row names each held note
// with its Press and Slide values. The same component serves both the big Live-keys view and - in
// compact mode - the small keystroke map that can replace the device drawing. Render-only; mouse
// press-drag to play is a later step.
//
// Touches: setTouch(fingerId, nx, ny, z) - nx 0..1 across the keyboard, ny 0..1 along the key (slide),
// z 0..1 pressure. end removes it. fingerId is any stable per-finger key.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <map>

class LiveKeysStrip  : public juce::Component,
                       private juce::Timer
{
public:
    LiveKeysStrip() { startTimerHz (60); }

    /** compact = the small keystroke map (shorter ticks, tighter labels, no readout row). */
    void setCompact (bool c) { if (compact != c) { compact = c; repaint(); } }

    /** The drawn keyboard span: MIDI note of the lowest key, and how many keys. */
    void setRange (int lowNote, int keyCount)
    {
        if (lowNote == low && keyCount == count) return;
        low = lowNote; count = juce::jmax (1, keyCount);
        repaint();
    }

    /** Semitones to add to the displayed note names (octave*12 + transpose), so C labels track the
        device. Key positions don't move; only the labels. */
    void setLabelShift (int semitones)
    {
        if (semitones == shift) return;
        shift = semitones;
        repaint();
    }

    /** A live touch. nx across the keyboard (0..1), ny along the key / slide (0..1), z pressure
        (0..1), strike 0..1 (initial contact), glide -1..1 (signed pitch bend). end removes it.
        fingerId is any stable per-finger key. */
    void setTouch (int fingerId, float nx, float ny, float z, float strike, float glide, bool end)
    {
        if (end) { if (auto it = touches.find (fingerId); it != touches.end()) it->second.releasing = true; return; }
        auto& t = touches[fingerId];
        t.x = juce::jlimit (0.0f, 1.0f, nx);
        t.y = juce::jlimit (0.0f, 1.0f, ny);
        t.z = juce::jlimit (0.0f, 1.0f, z);
        t.glide = juce::jlimit (-1.0f, 1.0f, glide);
        t.releasing = false;
        if (! t.seen) { t.dx = t.x; t.dy = t.y; t.dz = t.z; t.seen = true; }
        t.strike = strike;   // held for the note's life (captured at contact upstream)
    }

    void clearTouches() { touches.clear(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        auto full = getLocalBounds();
        g.setColour (well);
        g.fillRoundedRectangle (full.toFloat(), radius::tile);

        const int readoutH = compact ? 0 : 30;   // always reserve the 5D key row in the big view
        auto readoutR = full.removeFromBottom (readoutH);

        auto keysR = full.reduced (compact ? 12 : 18, compact ? 10 : 14);
        const float x0 = (float) keysR.getX(), w = (float) keysR.getWidth();
        const float top = (float) keysR.getY(), h = (float) keysR.getHeight();
        const float tickH = h * (compact ? 0.42f : 0.5f);
        const int   whites = whiteCount();

        // Black keys as short ticks from the top; a label under each physical C (name shifted).
        g.setFont (grab::theme::type::font (compact ? grab::theme::type::Role::caption
                                                    : grab::theme::type::Role::body));
        int wIdx = 0;
        for (int n = low; n < low + count; ++n)
        {
            if (isBlack (n))
            {
                const float fx = x0 + w * whiteFraction (wIdx, whites);
                g.setColour (grid);
                g.fillRect (juce::Rectangle<float> (fx - (compact ? 0.5f : 1.0f), top, compact ? 1.0f : 2.0f, tickH));
                continue;
            }
            if (n % 12 == 0)   // physical C: a fixed C-grid reference (C3/C4/C5); octave/transpose show
            {                   // in the readout note, not here, so the grid stays stable.
                const float fx = x0 + w * (float) wIdx / (float) whites;
                g.setColour (label);
                g.drawText (noteName (n),
                            juce::Rectangle<float> (fx + 2.0f, top + h - (compact ? 14.0f : 18.0f),
                                                    42.0f, 18.0f),
                            juce::Justification::centredLeft);
            }
            ++wIdx;
        }

        // Touch rings: a Press-hue ring (grows with pressure), a Strike-red centre dot (its initial
        // impact) - matching the device graphic's markers. x follows glide, y follows slide up the key.
        const float ringMax = compact ? 20.0f : 46.0f, ringMin = compact ? 6.0f : 14.0f;
        for (auto& kv : touches)
        {
            auto& t = kv.second;
            const float cx = x0 + t.dx * w;
            const float cy = top + (1.0f - t.dy) * h * 0.78f + h * 0.11f;   // slide runs up the key
            const float rad = ringMin + t.dz * ringMax;
            const float a = t.releasing ? juce::jlimit (0.0f, 1.0f, t.fade) : 1.0f;
            g.setColour (press.withAlpha (0.14f * a));
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (press.withAlpha (a));
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, compact ? 1.5f : 2.5f);
            g.setColour (strike.withAlpha (a));
            const float dot = (compact ? 2.0f : 5.0f) + (compact ? 2.0f : 6.0f) * t.strike;
            g.fillEllipse (cx - dot, cy - dot, dot * 2.0f, dot * 2.0f);
        }

        // Readout row: a 5D colour key, then one chip per held note (name + Strike/Press/Glide/Slide,
        // each in its dimension's hue). The note name carries the octave + transpose (the real note).
        if (readoutH > 0)
        {
            auto row = readoutR.reduced (18, 4);
            g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
            float x = (float) row.getX();
            const float cy = (float) row.getCentreY();

            // Key: dimension colour swatches with names.
            for (auto& d : dims())
            {
                g.setColour (d.colour);
                g.fillEllipse (x, cy - 4.0f, 8.0f, 8.0f);
                x += 12.0f;
                g.setColour (label);
                const float lw = (float) d.name.length() * 7.0f + 4.0f;
                g.drawText (d.name, juce::Rectangle<float> (x, cy - 8.0f, lw, 16.0f), juce::Justification::centredLeft);
                x += lw + 8.0f;
            }
            x += 10.0f;

            int shown = 0;
            for (auto& kv : touches)
            {
                if (shown >= 4 || x > (float) row.getRight() - 150.0f) break;
                auto& t = kv.second;
                if (t.releasing) continue;
                const int note = juce::roundToInt (low + t.dx * (count - 1)) + shift;
                g.setColour (ink);
                g.drawText (noteName (note), juce::Rectangle<float> (x, cy - 8.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
                x += 40.0f;
                const int vals[4] = { juce::roundToInt (t.strike * 127), juce::roundToInt (t.dz * 127),
                                      juce::roundToInt (t.glide * 127),  juce::roundToInt (t.dy * 127) };
                const juce::Colour cols[4] = { strike, press, glide, slide };
                for (int i = 0; i < 4; ++i)
                {
                    g.setColour (cols[i]);
                    g.fillEllipse (x, cy - 3.0f, 6.0f, 6.0f);
                    x += 9.0f;
                    g.setColour (ink2);
                    g.drawText (juce::String (vals[i]), juce::Rectangle<float> (x, cy - 8.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
                    x += 30.0f;
                }
                x += 14.0f;
                ++shown;
            }
        }
    }

    struct DimKey { juce::Colour colour; juce::String name; };
    static juce::Array<DimKey> dims()
    {
        using namespace grab::theme;
        juce::Array<DimKey> a;
        a.add ({ strike, "Strike" }); a.add ({ press, "Press" });
        a.add ({ glide,  "Glide"  }); a.add ({ slide, "Slide" });
        return a;
    }

private:
    struct Live { float x = 0, y = 0.5f, z = 0, dx = 0, dy = 0.5f, dz = 0, strike = 0, glide = 0, fade = 1.0f;
                  bool seen = false, releasing = false; };

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
    // The x fraction of a black key: midway between the surrounding white indices.
    static float whiteFraction (int wIdx, int whites) { return (wIdx - 0.5f) / (float) whites; }

    static juce::String noteName (int midi)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int n = ((midi % 12) + 12) % 12;
        return juce::String (names[n]) + juce::String (midi / 12 - 1);
    }

    void timerCallback() override
    {
        bool anim = false;
        for (auto it = touches.begin(); it != touches.end(); )
        {
            auto& t = it->second;
            t.dx += (t.x - t.dx) * 0.35f;
            t.dy += (t.y - t.dy) * 0.35f;
            t.dz += (t.z - t.dz) * 0.35f;
            if (t.releasing) { t.fade -= 0.10f; anim = true; if (t.fade <= 0.0f) { it = touches.erase (it); continue; } }
            else if (std::abs (t.x - t.dx) + std::abs (t.y - t.dy) + std::abs (t.z - t.dz) > 0.001f) anim = true;
            ++it;
        }
        if (anim) repaint();
    }

    bool compact = false;
    int  low = 48, count = 25;   // C3, two octaves (sensible default until setRange)
    int  shift = 0;              // octave*12 + transpose, added to displayed note names
    std::map<int, Live> touches;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveKeysStrip)
};
