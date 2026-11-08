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

        // Index-based (segment i == value i). Zone, pedal type, ...
        void setItems (juce::StringArray items)
        {
            labels = std::move (items);
            values.clear();
            if (selected >= labels.size()) selected = 0;
            repaint();
        }
        // Value-mapped: segment i shows labels[i] and carries vals[i] as the device value, so the
        // display order can differ from the device's value order (MIDI mode: show MPE first, whatever
        // value the firmware assigns it). Keeps the selection on the same value when it exists.
        void setItems (juce::StringArray items, juce::Array<int> vals)
        {
            const int cur = getSelectedValue();
            labels = std::move (items);
            values = std::move (vals);
            setSelectedValue (cur, false);
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
        // The device value of the selected segment (== index when no value map was given).
        int  getSelectedValue() const
        {
            return (values.isEmpty() || selected >= values.size()) ? selected : values[selected];
        }
        void setSelectedValue (int v, bool notify)
        {
            if (values.isEmpty()) { setSelectedIndex (v, notify); return; }
            const int i = values.indexOf (v);
            setSelectedIndex (i >= 0 ? i : 0, notify);
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
        juce::Array<int>  values;   // empty => segment index is the value
        int selected = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
    };
}
