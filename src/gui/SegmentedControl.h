// SegmentedControl - a row of pill segments, one selected (FabFilter / iOS style). Used by ConfigRow
// for small option sets (MIDI mode, MPE zone, ...) in place of a dropdown, per Fred's design.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace grab
{
    class SegmentedControl  : public juce::Component
    {
    public:
        SegmentedControl() = default;

        std::function<void()> onChange;

        void setItems (juce::StringArray items)
        {
            labels = std::move (items);
            if (selected >= labels.size()) selected = 0;
            repaint();
        }
        int  count() const { return labels.size(); }
        int  getSelectedIndex() const { return selected; }
        void setSelectedIndex (int i, bool notify)
        {
            i = juce::jlimit (0, juce::jmax (0, labels.size() - 1), i);
            if (i == selected) return;
            selected = i;
            repaint();
            if (notify && onChange) onChange();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace grab::theme;
            auto r = getLocalBounds().toFloat();
            g.setColour (well);
            g.fillRoundedRectangle (r, radius::control);

            const int n = juce::jmax (1, labels.size());
            const float segW = r.getWidth() / (float) n;
            g.setFont (grab::theme::type::font (grab::theme::type::Role::control));
            for (int i = 0; i < labels.size(); ++i)
            {
                auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight());
                if (i == selected)
                {
                    g.setColour (segmentOn);
                    g.fillRoundedRectangle (seg.reduced (2.0f), radius::segmentInner);
                }
                g.setColour (i == selected ? ink : ink2);
                g.drawText (labels[i], seg, juce::Justification::centred, true);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int n = juce::jmax (1, labels.size());
            setSelectedIndex (juce::jlimit (0, n - 1, (int) (e.position.x / (getWidth() / (float) n))), true);
        }

    private:
        juce::StringArray labels;
        int selected = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
    };
}
