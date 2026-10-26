// FooterBar — a status strip across the bottom that describes whatever control is under the
// mouse. It reuses JUCE's tooltip mechanism: each control carries its help text as a tooltip,
// and the bar polls the component under the mouse (walking up to the nearest one with text).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

class FooterBar  : public juce::Component,
                   private juce::Timer
{
public:
    FooterBar() { startTimerHz (10); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (grab::theme::panelBg);
        g.fillRect (getLocalBounds());
        g.setColour (grab::theme::outline);
        g.fillRect (getLocalBounds().removeFromTop (1));   // divider from the content above

        const bool hasText = text.isNotEmpty();
        g.setColour (hasText ? grab::theme::textPrimary : grab::theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (hasText ? text : hint, getLocalBounds().reduced (12, 0),
                    juce::Justification::centredLeft, true);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
