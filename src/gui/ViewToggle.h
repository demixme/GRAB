// ViewToggle - Fred's drawing-tile view switch: two stacked icon cells, Device drawing and Live keys.
// Sits in the top-right of the device-drawing tile; the selected cell is lit. onChange(index) fires
// with 0 = device, 1 = keys.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace grab
{
    class ViewToggle  : public juce::Component
    {
    public:
        ViewToggle() { setInterceptsMouseClicks (true, false); }

        std::function<void (int)> onChange;   // 0 device, 1 keys

        int  getIndex() const { return index; }
        void setIndex (int i, bool notify)
        {
            i = juce::jlimit (0, 1, i);
            if (i == index) return;
            index = i;
            repaint();
            if (notify && onChange) onChange (index);
        }

        static constexpr int cellW = 30, cellH = 28;
        int preferredWidth()  const { return cellW + 6; }
        int preferredHeight() const { return cellH * 2 + 9; }

        void paint (juce::Graphics& g) override
        {
            using namespace grab::theme;
            auto r = getLocalBounds().toFloat();
            g.setColour (well);
            g.fillRoundedRectangle (r, radius::control);

            for (int i = 0; i < 2; ++i)
            {
                auto cell = cellBounds (i);
                const bool on = (i == index);
                if (on)
                {
                    g.setColour (segmentOn);
                    g.fillRoundedRectangle (cell.reduced (2.0f), radius::segmentInner);
                }
                g.setColour (on ? ink : ink2);
                if (i == 0) drawDeviceIcon (g, cell);
                else        drawKeysIcon   (g, cell);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            setIndex (e.position.y < (float) getHeight() * 0.5f ? 0 : 1, true);
        }

    private:
        juce::Rectangle<float> cellBounds (int i) const
        {
            auto r = getLocalBounds().reduced (3);
            return r.removeFromTop (cellH).translated (0, i * (cellH + 3)).toFloat();
        }

        // A keyboard: a rounded body with a few key dividers.
        static void drawDeviceIcon (juce::Graphics& g, juce::Rectangle<float> cell)
        {
            auto b = cell.withSizeKeepingCentre (16.0f, 11.0f);
            g.drawRoundedRectangle (b, 2.0f, 1.4f);
            for (float fx : { 0.33f, 0.5f, 0.66f })
                g.drawLine (b.getX() + b.getWidth() * fx, b.getY(),
                            b.getX() + b.getWidth() * fx, b.getY() + b.getHeight() * 0.62f, 1.3f);
        }

        // Live keys: vertical key lines with a touch dot (Fred's vmap glyph).
        static void drawKeysIcon (juce::Graphics& g, juce::Rectangle<float> cell)
        {
            auto b = cell.withSizeKeepingCentre (16.0f, 13.0f);
            for (float fx : { 0.0f, 0.33f, 0.66f, 1.0f })
                g.drawLine (b.getX() + b.getWidth() * fx, b.getY(),
                            b.getX() + b.getWidth() * fx, b.getY() + b.getHeight() * 0.62f, 1.3f);
            const float cx = b.getCentreX(), cy = b.getBottom() - 3.0f;
            g.drawEllipse (cx - 2.6f, cy - 2.6f, 5.2f, 5.2f, 1.3f);
        }

        int index = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViewToggle)
    };
}
