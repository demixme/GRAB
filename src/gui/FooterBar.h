// FooterBar — a status strip across the bottom that describes whatever control is under the
// mouse. It reuses JUCE's tooltip mechanism: each control carries its help text as a tooltip,
// and the bar polls the component under the mouse (walking up to the nearest one with text).
//
// The right-hand corner shows the GRAB build version. When a newer release is found on GitHub it
// turns into a clickable "update available" link that opens the releases page.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

class FooterBar  : public juce::Component,
                   private juce::Timer
{
public:
    FooterBar() { startTimerHz (10); }

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

        const juce::Font font (juce::FontOptions (13.0f));
        g.setFont (font);

        // Right corner: version, or an "update available" link in the accent colour.
        auto area = getLocalBounds().reduced (12, 0);
        const auto& corner = updateText.isNotEmpty() ? updateText : versionText;
        if (corner.isNotEmpty())
        {
            const int w = (int) juce::GlyphArrangement::getStringWidth (font, corner) + 8;
            auto right = area.removeFromRight (w);
            cornerBounds = right;
            g.setColour (updateText.isNotEmpty() ? grab::theme::accent : grab::theme::textSecondary);
            g.drawText (corner, right, juce::Justification::centredRight, true);
            area.removeFromRight (12);   // gap before the hint text
        }

        const bool hasText = text.isNotEmpty();
        g.setColour (hasText ? grab::theme::textPrimary : grab::theme::textSecondary);
        g.drawText (hasText ? text : hint, area, juce::Justification::centredLeft, true);
    }

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

        if (tip != text) { text = tip; repaint(); }
    }

    juce::String text;
    const juce::String hint { "Hover a control for what it does.  Double-click or right-click to reset it." };
    juce::String versionText, updateText, releasesUrl;
    juce::Rectangle<int> cornerBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
