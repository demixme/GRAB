// TextMarquee — horizontal auto-scroll for a single line of text wider than its space. Ported from
// demix.me core UI (demixme-plugins/core/include/demixme/ui/TextMarquee.h) so GRAB's footer can scroll
// a long tooltip in full instead of truncating it with an ellipsis.
//
// The helper owns no component and no timer: the owner (FooterBar) runs a juce::Timer at kHz while
// scrolling() is true, calling tick() + repaint() each frame, and draws with drawScrolled().
// configure() (re)assesses overflow whenever the text or available width changes.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace grab
{

class TextMarquee
{
public:
    // (Re)assess whether `textWidthPx` overflows `availWidthPx`, resetting the scroll to the start with
    // the initial hold. Returns true if scrolling is needed (the owner then runs a timer at kHz).
    bool configure (float textWidthPx, float availWidthPx) noexcept
    {
        scrollX_   = 0.0f;
        hold_      = kHoldTicks;
        textW_     = textWidthPx;
        scrolling_ = (availWidthPx > 0.0f && textWidthPx > availWidthPx);
        return scrolling_;
    }

    bool scrolling() const noexcept { return scrolling_; }

    // Advance one frame (owner calls this then repaint() from its kHz timer). Holds at the start for
    // ~kHoldTicks frames, then scrolls one strip and loops back to the start (with another hold).
    void tick() noexcept
    {
        if (! scrolling_) return;
        if (hold_ > 0) { --hold_; return; }
        scrollX_ += kPxPerTick;
        if (scrollX_ >= textW_ + kGapPx) { scrollX_ = 0.0f; hold_ = kHoldTicks; }
    }

    // Draw `text` scrolling within `region` (clipped), as two copies kGapPx apart so it reads as a
    // seamless loop. Call only while scrolling(); otherwise draw the text normally (it fits).
    void drawScrolled (juce::Graphics& g, juce::Rectangle<int> region,
                       const juce::Font& font, const juce::String& text, juce::Colour colour) const
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (region);
        g.setFont (font);
        g.setColour (colour);
        const int w     = (int) std::ceil (textW_);
        const int x     = region.getX() - (int) scrollX_;
        const int strip = w + (int) kGapPx;
        g.drawText (text, x,         region.getY(), w, region.getHeight(), juce::Justification::centredLeft, false);
        g.drawText (text, x + strip, region.getY(), w, region.getHeight(), juce::Justification::centredLeft, false);
    }

    static float textWidth (const juce::Font& font, const juce::String& text)
    {
        if (text.isEmpty()) return 0.0f;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, text.length(), true).getWidth();
    }

    static constexpr int   kHz        = 30;     // timer rate the owner should use
    static constexpr float kGapPx     = 48.0f;  // blank between the two scrolling copies
    static constexpr int   kHoldTicks = 45;     // ~1.5 s pause at the start (at kHz)
    static constexpr float kPxPerTick = 1.0f;   // scroll speed

private:
    bool  scrolling_ = false;
    float scrollX_   = 0.0f;
    float textW_     = 0.0f;
    int   hold_      = 0;
};

} // namespace grab
