// SectionCard — a titled, bordered panel that stacks a section's rows. The config page
// lays these out in a responsive grid (Sensitivity spans the top, the rest flow into
// columns), so the UI reads as grouped cards rather than one long list.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <memory>
#include <utility>
#include <vector>

class SectionCard  : public juce::Component
{
public:
    explicit SectionCard (juce::String titleToUse) : title (std::move (titleToUse)) {}

    /** Add a child row/panel with a fixed height, stacked under the title. */
    void addItem (juce::Component& c, int h) { addAndMakeVisible (c); items.push_back ({ &c, h }); }
    bool isEmpty() const { return items.empty(); }

    /** Height this card needs to show its title and all its rows. */
    int contentHeight() const
    {
        int h = graphRow ? 0 : (kPadTop + kTitleH + kGap);
        for (auto& it : items) h += it.second + kGap;
        return h + (graphRow ? 0 : kPadBottom) - kGap;
    }

    const juce::String& getTitle() const { return title; }

    /** The Sensitivity "graph row": a transparent holder whose child paints its own two tiles
        (dark graph + lighter inspector) on the window gutter, full-width and with no card title -
        so the inspector reads as a sibling tile, not something embedded in the big dark panel. */
    void setGraphRow (bool r) { graphRow = r; }

    void paint (juce::Graphics& g) override
    {
        if (graphRow) return;   // transparent: sits on the window bg, child paints its own tiles

        auto r = getLocalBounds().toFloat();
        // Borderless tile on the window gutters (the "bento" look): a filled rounded panel.
        // No outline - the tone step off the window does the work.
        g.setColour (grab::theme::panel);
        g.fillRoundedRectangle (r, grab::theme::radius::tile);

        g.setColour (grab::theme::ink);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::title));
        g.drawText (title, getLocalBounds().reduced (kPadX, 0).withTop (kPadTop).withHeight (kTitleH),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (! graphRow)
        {
            b = b.reduced (kPadX, 0);
            b.removeFromTop (kPadTop + kTitleH + kGap);
        }
        for (auto& it : items)
        {
            it.first->setBounds (b.removeFromTop (it.second));
            b.removeFromTop (kGap);
        }
    }

private:
    juce::String title;
    bool graphRow = false;
    std::vector<std::pair<juce::Component*, int>> items;
    static constexpr int kTitleH = 18, kPadTop = 12, kPadBottom = 14, kPadX = 16, kGap = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionCard)
};

//==============================================================================
/** Lay cards out in `rowHolder` (the viewed component of `viewport`): the card whose title
    is `fullWidthTitle` spans the full width at the top, the rest flow into up to three
    responsive columns, each card dropped into the shortest column. Sizes rowHolder so the
    viewport scrolls. Shared by the BLOCKS and RISE config pages. */
inline void layoutCardGrid (juce::Viewport& viewport, juce::Component& rowHolder,
                            const std::vector<std::unique_ptr<SectionCard>>& cards,
                            const juce::String& fullWidthTitle)
{
    const int gap    = grab::theme::space::gutter;   // 12
    const int availW = juce::jmax (260, viewport.getWidth() - 14);   // room for the scrollbar
    const int cols   = juce::jlimit (1, 3, (availW + gap) / (280 + gap));
    const int colW   = cols > 1 ? (availW - gap * (cols - 1)) / cols : availW;

    int topY = 0;
    SectionCard* full = nullptr;
    for (auto& c : cards)
        if (c->getTitle() == fullWidthTitle) { full = c.get(); break; }

    if (full != nullptr)
    {
        full->setBounds (0, 0, availW, full->contentHeight());
        topY = full->getBottom() + gap;
    }

    std::vector<int> colBottom ((size_t) cols, topY);
    for (auto& c : cards)
    {
        if (c.get() == full)
            continue;
        int col = 0;
        for (int i = 1; i < cols; ++i)
            if (colBottom[(size_t) i] < colBottom[(size_t) col]) col = i;
        c->setBounds (col * (colW + gap), colBottom[(size_t) col], colW, c->contentHeight());
        colBottom[(size_t) col] = c->getBottom() + gap;
    }

    int total = topY;
    for (int b : colBottom) total = juce::jmax (total, b);
    rowHolder.setSize (availW, juce::jmax (total, viewport.getHeight()));
}

//==============================================================================
/** Control-block layout: a slim left column for the short "MIDI Settings" (and any other)
    cards, and the tall consolidated `wideTitle` card ("Buttons") filling the rest — so its
    per-button rows (CC + behaviour + default/min/max, with interleaved colour rows) fit
    without the window having to be dragged very wide. Falls back to the normal grid when the
    window is too narrow for two columns or the wide card isn't present. */
inline void layoutControlCards (juce::Viewport& viewport, juce::Component& rowHolder,
                                const std::vector<std::unique_ptr<SectionCard>>& cards,
                                const juce::String& wideTitle)
{
    const int gap    = grab::theme::space::gutter;   // 12
    const int availW = juce::jmax (260, viewport.getWidth() - 14);
    const int leftW  = juce::jlimit (240, 420, availW * 2 / 5);
    const int rightW = availW - leftW - gap;

    SectionCard* wide = nullptr;
    for (auto& c : cards)
        if (c->getTitle() == wideTitle) { wide = c.get(); break; }

    if (wide == nullptr || rightW < 300)   // too narrow for two columns: use the responsive grid
    {
        layoutCardGrid (viewport, rowHolder, cards, "Sensitivity");
        return;
    }

    int leftY = 0;
    for (auto& c : cards)
    {
        if (c.get() == wide)
            continue;
        c->setBounds (0, leftY, leftW, c->contentHeight());
        leftY = c->getBottom() + gap;
    }
    wide->setBounds (leftW + gap, 0, rightW, wide->contentHeight());

    const int total = juce::jmax (leftY, wide->getBottom() + gap);
    rowHolder.setSize (availW, juce::jmax (total, viewport.getHeight()));
}
