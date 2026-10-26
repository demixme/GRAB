// SectionCard — a titled, bordered panel that stacks a section's rows. The config page
// lays these out in a responsive grid (Sensitivity spans the top, the rest flow into
// columns), so the UI reads as grouped cards rather than one long list.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <utility>
#include <vector>

class SectionCard  : public juce::Component
{
public:
    explicit SectionCard (juce::String titleToUse) : title (std::move (titleToUse)) {}

    /** Add a child row/panel with a fixed height, stacked under the title. */
    void addItem (juce::Component& c, int h) { addAndMakeVisible (c); items.push_back ({ &c, h }); }

    /** Height this card needs to show its title and all its rows. */
    int contentHeight() const
    {
        int h = kPadTop + kTitleH + kGap;
        for (auto& it : items) h += it.second + kGap;
        return h + kPadBottom - kGap;
    }

    const juce::String& getTitle() const { return title; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (grab::theme::panelBg);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (grab::theme::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (grab::theme::header);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (title, getLocalBounds().reduced (kPadX, 0).withTop (kPadTop).withHeight (kTitleH),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (kPadX, 0);
        b.removeFromTop (kPadTop + kTitleH + kGap);
        for (auto& it : items)
        {
            it.first->setBounds (b.removeFromTop (it.second));
            b.removeFromTop (kGap);
        }
    }

private:
    juce::String title;
    std::vector<std::pair<juce::Component*, int>> items;
    static constexpr int kTitleH = 22, kPadTop = 10, kPadBottom = 12, kPadX = 12, kGap = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionCard)
};
