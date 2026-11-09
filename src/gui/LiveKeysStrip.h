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
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    LiveKeysStrip() { startTimerHz (60); }

    std::function<void()> onClick;   // the compact map uses this to open the big Live-keys view

    /** compact = the small keystroke map (shorter ticks, tighter labels, no readout row). */
    void setCompact (bool c) { if (compact != c) { compact = c; repaint(); } }

    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

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

    /** The device keybed's width/height, so the big Live-keys view holds the device's proportions
        instead of stretching (Seaboard Block art is 600x300 -> 2.0). */
    void setKeybedAspect (float widthOverHeight)
    {
        widthOverHeight = juce::jlimit (1.0f, 6.0f, widthOverHeight);
        if (std::abs (widthOverHeight - keybedAspect) < 0.001f) return;
        keybedAspect = widthOverHeight;
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
        if (! t.seen) { t.dx = t.x; t.dy = t.y; t.dz = t.z; t.seen = true; latestId = fingerId; }
        t.strike = strike;   // held for the note's life (captured at contact upstream)
    }

    void clearTouches() { touches.clear(); repaint(); }

    /** nx (0..1) for a MIDI note, matching the drawn key layout - for feeding note-based touches
        (RISE/GRAND have no surface x, only a note). Clamped to the displayed range. */
    float nxForNote (int midiNote) const
    {
        return juce::jlimit (0.0f, 1.0f, pianoX (juce::jlimit (0, count - 1, midiNote - low)));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        auto full = getLocalBounds();
        g.setColour (well);
        g.fillRoundedRectangle (full.toFloat(), radius::tile);

        // A fixed readout row at the bottom-left (Fred's layout), below the keybed.
        auto content = full;
        auto readoutRow = compact ? juce::Rectangle<int>() : content.removeFromBottom (34);
        auto area = content.reduced (compact ? 12 : 18, compact ? 10 : 14);

        // Keep the device's wide-and-short keybed proportions instead of stretching to fill the tall
        // area: fit a keybedAspect:1 rectangle (centred), so the slide range matches the real keywave
        // depth and the glide strips sit at the keybed's top/bottom edges.
        auto keysR = area;
        if (! compact)
        {
            int kw = area.getWidth(), kh = juce::roundToInt ((float) kw / keybedAspect);
            if (kh > area.getHeight()) { kh = area.getHeight(); kw = juce::roundToInt ((float) kh * keybedAspect); }
            keysR = juce::Rectangle<int> (kw, kh).withCentre (area.getCentre());
        }
        const float x0 = (float) keysR.getX(), w = (float) keysR.getWidth();
        const float top = (float) keysR.getY(), h = (float) keysR.getHeight();

        // Piano layout, matching the device graphic: a touch's nx maps linearly across the (piano-
        // proportioned) keybed, so the ring lands exactly where the device shows it. White keys are
        // faint dividers, black keys short ticks, and each key that *sounds* as a C (physical + octave
        // + transpose) is labelled - so labels, the ring and the readout all agree and track octave.
        // The Seaboard's keys sit in a band below the top glide strip; the black keys are shorter.
        const float ky0 = top + kKeyBandTop  * h;
        const float ky1 = top + kBlackKeyBot * h;   // black-key divider lines are short (raised notes)

        g.setFont (grab::theme::type::font (compact ? grab::theme::type::Role::caption
                                                    : grab::theme::type::Role::body));
        for (int s = 0; s < count; ++s)
        {
            const int   n  = low + s;
            const float fx = x0 + w * pianoX (s);
            if (isBlack (n))
            {
                g.setColour (grid);
                g.fillRect (juce::Rectangle<float> (fx - (compact ? 0.5f : 1.0f), ky0, compact ? 1.0f : 2.0f, ky1 - ky0));
                continue;
            }
            if (posMod (n + shift, 12) == 0)   // a key that sounds as a C
            {
                g.setColour (label);
                g.drawText (noteName (n + shift),      // centred on the key line, so it aligns with the ring
                            juce::Rectangle<float> (fx - 21.0f, top + h - (compact ? 14.0f : 18.0f), 42.0f, 18.0f),
                            juce::Justification::centred);
            }
        }

        // Touch markers: identical in style/behaviour to the device graphic (KeyboardView). A Press-hue
        // ring grows with pressure (outer), a Strike-hue disc grows with the initial strike and is
        // clamped inside the ring so the ring is always visible around it. x follows glide, y follows
        // slide (same orientation as the device graphic). Scaled up for the big view.
        for (auto& kv : touches)
        {
            auto& t = kv.second;
            const float cx = x0 + t.dx * w;
            // Slide maps across the keywave band (between the glide strips), so the ring tracks the
            // keys the same way the device graphic does.
            const float cy = top + (kKeyBandTop + juce::jlimit (0.0f, 1.0f, t.dy) * (kKeyBandBot - kKeyBandTop)) * h;
            const float a  = t.releasing ? juce::jlimit (0.0f, 1.0f, t.fade) : 1.0f;

            const float strikeR = (compact ? 2.0f : 4.0f)  + juce::jlimit (0.0f, 1.0f, t.strike) * (compact ? 7.0f : 24.0f);
            const float outer   = (compact ? 5.0f : 12.0f) + juce::jlimit (0.0f, 1.0f, t.dz)     * (compact ? 16.0f : 48.0f);
            const float inner   = juce::jmin (strikeR, outer - 2.0f);
            g.setColour (press.withAlpha  (0.12f * a)); g.fillEllipse (juce::Rectangle<float> (outer * 2, outer * 2).withCentre ({ cx, cy }));
            g.setColour (strike.withAlpha (0.90f * a)); g.fillEllipse (juce::Rectangle<float> (inner * 2, inner * 2).withCentre ({ cx, cy }));
            g.setColour (press.withAlpha  (0.95f * a)); g.drawEllipse (juce::Rectangle<float> (outer * 2, outer * 2).withCentre ({ cx, cy }), compact ? 1.5f : 2.5f);
        }

        // One readout only - the active/latest key (several sets at once is overwhelming). Prefer the
        // last-pressed finger; fall back to any still-held one. Fixed at the bottom-left (Fred's layout).
        if (! compact)
        {
            const Live* sel = nullptr;
            if (auto it = touches.find (latestId); it != touches.end() && ! it->second.releasing) sel = &it->second;
            if (sel == nullptr)
                for (auto& kv : touches) if (! kv.second.releasing) { sel = &kv.second; break; }
            if (sel != nullptr)
                paintReadoutPill (g, *sel, readoutRow.reduced (0, 4));
        }
    }

private:
    struct Live { float x = 0, y = 0.5f, z = 0, dx = 0, dy = 0.5f, dz = 0, strike = 0, glide = 0, fade = 1.0f;
                  bool seen = false, releasing = false; };

    // The readout pill at the bottom-left: the note (octave + transpose), then Strike/Press/Glide/Slide
    // each in its 5D hue. Slide rises toward the back of the key (up = more), so it's 1 - y.
    void paintReadoutPill (juce::Graphics& g, const Live& t, juce::Rectangle<int> row)
    {
        using namespace grab::theme;
        const int note = noteAtX (t.dx) + shift;
        const int vals[4] = { juce::roundToInt (t.strike * 127), juce::roundToInt (t.dz * 127),
                              juce::roundToInt (t.glide * 127),  juce::roundToInt ((1.0f - t.dy) * 127) };
        const juce::Colour cols[4] = { strike, press, glide, slide };

        const float chipH = (float) row.getHeight();
        auto pill = juce::Rectangle<float> ((float) row.getX(), (float) row.getY(), 44.0f + 4.0f * 34.0f, chipH);
        g.setColour (raised);
        g.fillRoundedRectangle (pill, radius::control);

        g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
        float x = pill.getX() + 10.0f;
        g.setColour (ink);
        g.drawText (noteName (note), juce::Rectangle<float> (x, pill.getY(), 36.0f, chipH), juce::Justification::centredLeft);
        x += 36.0f;
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (cols[i]);
            g.fillEllipse (x, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
            x += 9.0f;
            g.setColour (cols[i].withAlpha (0.92f));
            g.drawText (juce::String (vals[i]), juce::Rectangle<float> (x, pill.getY(), 25.0f, chipH), juce::Justification::centredLeft);
            x += 25.0f;
        }
    }

    static int posMod (int a, int m) { return ((a % m) + m) % m; }
    static bool isBlack (int note)
    {
        switch (posMod (note, 12)) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; }
    }
    int whiteCount() const
    {
        int c = 0;
        for (int i = 0; i < count; ++i) if (! isBlack (low + i)) ++c;
        return juce::jmax (2, c);
    }
    // x fraction (0..1) of semitone s (0-based from low), matching the device art's cell layout: the
    // keybed is W equal white cells, each white key centred in its cell at (wBelow+0.5)/W and each
    // black key on the cell boundary at wBelow/W. The device reports a touch's nx the same way (it
    // lands the ring on the art's keys), so the ring, dividers, labels and readout all agree.
    float pianoX (int s) const
    {
        const int W = whiteCount();
        int wBelow = 0;
        for (int i = 0; i < s; ++i) if (! isBlack (low + i)) ++wBelow;
        const float x = isBlack (low + s) ? (float) wBelow : (float) wBelow + 0.5f;
        return x / (float) W;
    }
    // The note under an nx (0..1 across the keybed): the semitone whose piano-x is nearest.
    int noteAtX (float f) const
    {
        int best = 0; float bd = 1.0e9f;
        for (int s = 0; s < count; ++s) { const float d = std::abs (pianoX (s) - f); if (d < bd) { bd = d; best = s; } }
        return low + best;
    }

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
    float keybedAspect = 2.0f;   // device keybed width/height (Seaboard Block art is 600x300)
    // The key band within the keybed, calibrated from a finger at the top/bottom of a key. The slide
    // ring spans the white-key length [kKeyBandTop, kKeyBandBot]; the black keys are shorter raised
    // notes, so their divider lines stop at kBlackKeyBot (~half the white-key length).
    static constexpr float kKeyBandTop = 0.12f, kKeyBandBot = 0.78f, kBlackKeyBot = 0.45f;
    int  low = 48, count = 25;   // C3, two octaves (sensible default until setRange)
    int  shift = 0;              // octave*12 + transpose, added to displayed note names
    int  latestId = -1;          // the most recently pressed finger (its readout is the one shown)
    std::map<int, Live> touches;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveKeysStrip)
};
