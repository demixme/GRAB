// FooterBar — a status strip across the bottom that describes whatever control is under the
// mouse. It reuses JUCE's tooltip mechanism: each control carries its help text as a tooltip,
// and the bar polls the component under the mouse (walking up to the nearest one with text).
//
// The right-hand corner shows the GRAB build version. When a newer release is found on GitHub it
// turns into a clickable "update available" link that opens the releases page.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "TextMarquee.h"

class FooterBar  : public juce::Component,
                   private juce::Timer
{
public:
    FooterBar() { startTimerHz (grab::TextMarquee::kHz); }   // 30 Hz: drives both the poll and the scroll

    /** Show the running build version in the right corner, e.g. "v0.1.30". */
    void setVersion (const juce::String& v)
    {
        versionText = v.startsWithChar ('v') ? v : "v" + v;
        repaint();
    }

    /** Mark that a newer release exists: the corner becomes a clickable link to `url`. */
    void setUpdateAvailable (const juce::String& latest, const juce::String& url)
    {
        updateText = "Update " + (latest.startsWithChar ('v') ? latest : "v" + latest) + " available";
        releasesUrl = url;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (grab::theme::panelBg);
        g.fillRect (getLocalBounds());
        g.setColour (grab::theme::outline);
        g.fillRect (getLocalBounds().removeFromTop (1));   // divider from the content above

        const juce::Font font       = grab::theme::type::fontAt (14.5f);   // bumped up; Red Hat, app font
        const juce::Font cornerFont = grab::theme::type::fontAt (12.5f);

        // Right corner: version, or an "update available" link in the accent colour.
        auto area = getLocalBounds().reduced (12, 0);
        g.setFont (cornerFont);
        const auto& corner = updateText.isNotEmpty() ? updateText : versionText;
        if (corner.isNotEmpty())
        {
            const int w = (int) juce::GlyphArrangement::getStringWidth (cornerFont, corner) + 8;
            auto right = area.removeFromRight (w);
            cornerBounds = right;
            g.setColour (updateText.isNotEmpty() ? grab::theme::accent : grab::theme::textSecondary);
            g.drawText (corner, right, juce::Justification::centredRight, true);
            area.removeFromRight (12);   // gap before the hint text
        }

        // Left: a transient status message if any, else the hovered control's help, else the hint.
        const juce::String left = leftText();
        const bool strong = status.isNotEmpty() || text.isNotEmpty();
        const auto colour  = strong ? grab::theme::textPrimary : grab::theme::textSecondary;

        // Reassess overflow when the text or the available width changed, then either scroll it (so a
        // long description can be read in full) or draw it normally.
        if (left != marqueeText || area.getWidth() != marqueeWidth)
        {
            marqueeText  = left;
            marqueeWidth = area.getWidth();
            marquee.configure (grab::TextMarquee::textWidth (font, left), (float) area.getWidth());
        }
        g.setFont (font);
        if (marquee.scrolling())
            marquee.drawScrolled (g, area, font, left, colour);
        else
        {
            g.setColour (colour);
            g.drawText (left, area, juce::Justification::centredLeft, false);
        }
    }

    void setStatus (const juce::String& s) { if (status != s) { status = s; repaint(); } }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (updateText.isNotEmpty() && cornerBounds.contains (e.getPosition()))
            juce::URL (releasesUrl).launchInDefaultBrowser();
    }

private:
    void timerCallback() override
    {
        auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        juce::String tip;
        for (auto* c = under; c != nullptr && tip.isEmpty(); c = c->getParentComponent())
            if (auto* ttc = dynamic_cast<juce::TooltipClient*> (c))
                tip = ttc->getTooltip();

        if (tip != text) { text = tip; repaint(); return; }   // paint reassesses the scroll on a change
        marquee.tick();
        if (marquee.scrolling()) repaint();
    }

    // The line shown on the left, with any newline in a tooltip (e.g. write-only notes) flattened so
    // it scrolls as one line.
    juce::String leftText() const
    {
        const juce::String raw = status.isNotEmpty() ? status : text.isNotEmpty() ? text : hint;
        return raw.replace ("\n", "   \xe2\x80\xa2   ");   // " • "
    }

    grab::TextMarquee marquee;
    juce::String marqueeText;
    int          marqueeWidth = -1;

    juce::String text, status;
    const juce::String hint { "Hover a control for what it does.  Double-click or right-click to reset it." };
    juce::String versionText, updateText, releasesUrl;
    juce::Rectangle<int> cornerBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
