// SectionCard — a titled, bordered panel that stacks a section's rows. The config page
// lays these out in a responsive grid (Sensitivity spans the top, the rest flow into
// columns), so the UI reads as grouped cards rather than one long list.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Changeable.h"

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

    /** Change a child's reserved height (e.g. the sensitivity graph growing when the inspector
        stacks below it at narrow widths). Returns true if the height actually changed. */
    bool setItemHeight (juce::Component* c, int h)
    {
        for (auto& it : items)
            if (it.first == c && it.second != h) { it.second = h; return true; }
        return false;
    }

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

        auto titleRow = getLocalBounds().reduced (kPadX, 0).withTop (kPadTop).withHeight (kTitleH);

        // Fred's tile header .cnt: "N changed", right-aligned in the label colour, when anything here
        // differs from the device.
        int n = 0;
        for (auto& it : items)
            if (auto* ch = dynamic_cast<grab::Changeable*> (it.first))
                n += ch->changedCount();
        if (n > 0)
        {
            g.setColour (grab::theme::label);
            g.setFont (grab::theme::type::font (grab::theme::type::Role::caption));
            g.drawText (juce::String (n) + " changed", titleRow, juce::Justification::centredRight);
        }

        g.setColour (grab::theme::ink);
        g.setFont (grab::theme::type::font (grab::theme::type::Role::title));
        g.drawText (title, titleRow, juce::Justification::centredLeft);
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
/** Three weighted columns: `fullWidthTitle` spans the top, then `col0Title` (38%) and `col1Title`
    (38%) each get their own column and everything else stacks in the third (24%). Falls back to the
    responsive grid when the window is too narrow for three columns. Used by the Seaboard keyboard
    page so the wide MIDI output / Expression cards get the room their controls need. */
inline void layoutWeightedCards (juce::Viewport& viewport, juce::Component& rowHolder,
                                 const std::vector<std::unique_ptr<SectionCard>>& cards,
                                 const juce::String& fullWidthTitle,
                                 const juce::String& col0Title, const juce::String& col1Title);

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

inline void layoutWeightedCards (juce::Viewport& viewport, juce::Component& rowHolder,
                                 const std::vector<std::unique_ptr<SectionCard>>& cards,
                                 const juce::String& fullWidthTitle,
                                 const juce::String& col0Title, const juce::String& col1Title)
{
    const int gap    = grab::theme::space::gutter;
    const int availW = juce::jmax (260, viewport.getWidth() - 14);

    if (availW < 760)   // too narrow for three columns: use the responsive grid
    {
        layoutCardGrid (viewport, rowHolder, cards, fullWidthTitle);
        return;
    }

    const int inner = availW - 2 * gap;
    const int w0 = juce::roundToInt (inner * 0.38f);
    const int w1 = juce::roundToInt (inner * 0.38f);
    const int w2 = inner - w0 - w1;
    const int x[3] = { 0, w0 + gap, w0 + gap + w1 + gap };
    const int w[3] = { w0, w1, w2 };

    int topY = 0;
    SectionCard* full = nullptr;
    for (auto& c : cards)
        if (c->getTitle() == fullWidthTitle) { full = c.get(); break; }
    if (full != nullptr)
    {
        full->setBounds (0, 0, availW, full->contentHeight());
        topY = full->getBottom() + gap;
    }

    int colBottom[3] = { topY, topY, topY };
    for (auto& c : cards)
    {
        if (c.get() == full) continue;
        const int col = c->getTitle() == col0Title ? 0 : c->getTitle() == col1Title ? 1 : 2;
        c->setBounds (x[col], colBottom[col], w[col], c->contentHeight());
        colBottom[col] = c->getBottom() + gap;
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
