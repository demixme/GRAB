// CcCardGrid - Fred's Controller CCs: a 3-up grid of small CC cards. Each card is a `well` tile with
// the control's name on top and, below, its CC number (drag to scrub, - / + to step, double- or
// right-click to reset). Replaces the vertical-fader panel on the RISE/GRAND page.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Changeable.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace grab
{

class CcCard  : public juce::Component,
                public juce::SettableTooltipClient,
                public grab::Changeable
{
public:
    using ChangeFn = std::function<void (juce::uint32, juce::int32)>;

    CcCard (juce::uint32 id_, juce::String name_, int value_, int lo_, int hi_, ChangeFn onChange_)
        : id (id_), name (std::move (name_)), value (value_), initial (value_),
          lo (lo_), hi (hi_), onChange (std::move (onChange_)) {}

    juce::uint32 getId()   const { return id; }
    int          getValue() const { return value; }
    void setValue (int v, bool notify = false)
    {
        v = juce::jlimit (lo, hi, v);
        if (v != value) { value = v; if (notify && onChange) onChange (id, v); }
        repaint();
        // keep the enclosing card's "N changed" header count live
        if (auto* grid = getParentComponent()) if (auto* sc = grid->getParentComponent()) sc->repaint();
    }
    void setDeviceValue (int v) { initial = v; repaint(); }   // the baseline the changed dot compares to
    int  changedCount() const override { return value != initial ? 1 : 0; }

    void resized() override
    {
        auto r = getLocalBounds();
        auto row = r.removeFromBottom (24);
        plusR  = row.removeFromRight (22);
        minusR = row.removeFromRight (22);
        valueR = row;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace grab::theme;
        const bool chg = value != initial;
        g.setColour (hover ? wellHover : well);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), (float) radius::card);

        g.setFont (type::font (type::Role::caption));
        g.setColour (chg ? ink2 : label);
        g.drawText (name, 10, 6, getWidth() - 16, 14, juce::Justification::centredLeft, true);
        if (chg)
        {
            const int nw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name);
            const float d = (float) size::changedDot;
            g.setColour (ink);
            g.fillEllipse ((float) juce::jmin (10 + nw + 6, getWidth() - 12), 10.0f - d * 0.5f, d, d);
        }

        g.setFont (type::fontMedium (type::Role::control));
        g.setColour (ink);
        g.drawText (juce::String (value), valueR.withTrimmedLeft (10), juce::Justification::centredLeft);

        drawStep (g, minusR, false, value > lo);
        drawStep (g, plusR,  true,  value < hi);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) { setValue (initial, true); return; }
        if      (minusR.contains (e.getPosition())) { setValue (value - 1, true); scrubbing = false; }
        else if (plusR.contains (e.getPosition()))  { setValue (value + 1, true); scrubbing = false; }
        else { scrubbing = true; scrubStart = value; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! scrubbing) return;
        const float rate = e.mods.isCommandDown() || e.mods.isCtrlDown() ? 0.15f : 0.5f;
        setValue (scrubStart - juce::roundToInt ((float) e.getDistanceFromDragStartY() * rate), true);
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (initial, true); }

private:
    void drawStep (juce::Graphics& g, juce::Rectangle<int> z, bool plus, bool en)
    {
        using namespace grab::theme;
        g.setColour (en ? label : disabled);
        const auto c = z.getCentre().toFloat();
        const float r = 4.0f;
        g.drawLine (c.x - r, c.y, c.x + r, c.y, 1.5f);
        if (plus) g.drawLine (c.x, c.y - r, c.x, c.y + r, 1.5f);
    }

    juce::uint32 id;
    juce::String name;
    int value, initial, lo, hi;
    ChangeFn onChange;
    juce::Rectangle<int> valueR, minusR, plusR;
    bool hover = false, scrubbing = false;
    int scrubStart = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CcCard)
};

//==============================================================================
class CcGrid  : public juce::Component,
                public grab::Changeable
{
public:
    CcGrid() = default;

    CcCard* add (juce::uint32 id, juce::String name, int value, int lo, int hi, CcCard::ChangeFn onChange)
    {
        auto card = std::make_unique<CcCard> (id, std::move (name), value, lo, hi, std::move (onChange));
        auto* p = card.get();
        addAndMakeVisible (*p);
        cards.push_back (std::move (card));
        return p;
    }
    bool isEmpty() const { return cards.empty(); }
    int  changedCount() const override { int n = 0; for (auto& c : cards) n += c->changedCount(); return n; }

    static constexpr int cols = 3, gap = 8, cardH = 52;
    int preferredHeight() const
    {
        const int rows = juce::jmax (1, ((int) cards.size() + cols - 1) / cols);
        return rows * cardH + (rows - 1) * gap;
    }

    void resized() override
    {
        const int n = (int) cards.size();
        if (n == 0) return;
        const int cw = (getWidth() - gap * (cols - 1)) / cols;
        for (int i = 0; i < n; ++i)
            cards[(size_t) i]->setBounds ((i % cols) * (cw + gap), (i / cols) * (cardH + gap), cw, cardH);
    }

    std::vector<std::unique_ptr<CcCard>> cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CcGrid)
};

} // namespace grab
