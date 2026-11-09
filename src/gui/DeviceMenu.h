// DeviceMenu - Fred's rich device switcher, shown in a CallOutBox from the dashboard-tile pill.
// A "Devices" list (device-art icon + name + "transport, fw" + a tick on the selected one), then a
// "Connection type" segmented footer (Auto / USB / Bluetooth). Replaces the plain combo + separate
// Connection Type box: one popup does both jobs, matching design/ui/prototype.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "DeviceArtData.h"   // embedded device-art SVGs (CMake: GRABDeviceArt)
#include "SegmentedControl.h"
#include "Theme.h"

namespace grab
{
    class DeviceMenu  : public juce::Component
    {
    public:
        struct Item
        {
            juce::String name;      // "Seaboard RISE 25"
            juce::String meta;      // "USB, fw 1.1.8"  (may be just the transport, or empty)
            juce::String artFile;   // "rise-25.svg", or "" for no icon
            bool         selected = false;
            int          id = 0;    // the caller's device id (blockBox item id)
        };

        std::function<void (int)> onPick;        // a device row was clicked (its id)
        std::function<void (int)> onTransport;   // connection type changed (0 Auto, 1 USB, 2 BLE)

        DeviceMenu()
        {
            seg.setItems ({ "Auto", "USB", "Bluetooth" });
            seg.onChange = [this] { if (onTransport) onTransport (seg.getSelectedIndex()); };
            addAndMakeVisible (seg);
        }

        // Fill the menu and size it to fit. transportIndex: 0 Auto / 1 USB / 2 BLE.
        void setContent (juce::Array<Item> newItems, int transportIndex)
        {
            items = std::move (newItems);
            seg.setSelectedIndex (juce::jlimit (0, 2, transportIndex), false);
            setSize (width, contentHeight());
            resized();
            repaint();
        }

        int contentHeight() const
        {
            return pad + headerH + (int) items.size() * rowH + pad + dividerGap
                 + headerH + segH + pad;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (pad, 0);
            r.removeFromTop (pad + headerH + (int) items.size() * rowH + pad + dividerGap + headerH);
            seg.setBounds (r.removeFromTop (segH));
        }

        void paint (juce::Graphics& g) override
        {
            using namespace grab::theme;
            g.fillAll (panel);

            auto r = getLocalBounds().reduced (pad, 0);
            r.removeFromTop (pad);

            g.setColour (label);
            g.setFont (type::font (type::Role::caption));
            g.drawText ("Devices", r.removeFromTop (headerH), juce::Justification::centredLeft);

            rowRects.clear();
            for (auto& it : items)
            {
                auto row = r.removeFromTop (rowH);
                rowRects.add (row);
                paintRow (g, row, it);
            }

            r.removeFromTop (pad);
            auto div = r.removeFromTop (dividerGap);
            g.setColour (divider);
            g.fillRect (div.withSizeKeepingCentre (div.getWidth(), 1));

            g.setColour (label);
            g.setFont (type::font (type::Role::caption));
            g.drawText ("Connection type", r.removeFromTop (headerH), juce::Justification::centredLeft);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < rowRects.size(); ++i)
                if (rowRects[i].contains (e.getPosition()) && i < items.size())
                {
                    if (onPick) onPick (items[i].id);
                    if (auto* cb = findParentComponentOfClass<juce::CallOutBox>()) cb->dismiss();
                    return;
                }
        }

        static constexpr int width = 340;

    private:
        void paintRow (juce::Graphics& g, juce::Rectangle<int> row, const Item& it)
        {
            using namespace grab::theme;
            auto r = row.reduced (2, 3);

            // Tick column (selected), then the device-art icon.
            auto tickCol = r.removeFromLeft (20);
            if (it.selected)
            {
                g.setColour (ink);
                juce::Path check;
                auto t = tickCol.withSizeKeepingCentre (12, 12).toFloat();
                check.startNewSubPath (t.getX(), t.getCentreY());
                check.lineTo (t.getX() + t.getWidth() * 0.38f, t.getBottom());
                check.lineTo (t.getRight(), t.getY());
                g.strokePath (check, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            auto iconR = r.removeFromLeft (iconW).reduced (2);
            if (auto* d = artFor (it.artFile))
            {
                // A small drop shadow so the device thumbnail sits above the row (matches the big art).
                const auto db = d->getDrawableBounds();
                if (! db.isEmpty())
                {
                    const auto placed = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                                            .appliedTo (db, iconR.toFloat());
                    juce::Path shape;
                    shape.addRoundedRectangle (placed, juce::jmax (2.0f, placed.getHeight() * 0.12f));
                    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 5, { 0, 2 }).drawForPath (g, shape);
                }
                d->drawWithin (g, iconR.toFloat(), juce::RectanglePlacement::centred, 1.0f);
            }
            r.removeFromLeft (8);

            // Name (left) and metadata (right, dimmer).
            if (it.meta.isNotEmpty())
            {
                g.setColour (label);
                g.setFont (type::font (type::Role::caption));
                g.drawText (it.meta, r.removeFromRight (118), juce::Justification::centredRight);
            }
            g.setColour (it.selected ? ink : ink2);
            g.setFont (type::font (type::Role::body));
            g.drawText (it.name, r, juce::Justification::centredLeft, true);
        }

        // Parse + cache a device-art SVG as a Drawable (cached failures stay cached).
        juce::Drawable* artFor (const juce::String& file)
        {
            if (file.isEmpty()) return nullptr;
            auto it = artCache.find (file);
            if (it != artCache.end()) return it->second.get();

            std::unique_ptr<juce::Drawable> d;
            for (int i = 0; i < DeviceArtData::namedResourceListSize; ++i)
                if (file == DeviceArtData::originalFilenames[i])
                {
                    int size = 0;
                    if (const char* data = DeviceArtData::getNamedResource (DeviceArtData::namedResourceList[i], size))
                        if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
                            d = juce::Drawable::createFromSVG (*xml);
                    break;
                }
            auto* raw = d.get();
            artCache[file] = std::move (d);
            return raw;
        }

        static constexpr int pad = 12, headerH = 18, rowH = 40, segH = 34, dividerGap = 12, iconW = 40;

        juce::Array<Item>               items;
        juce::Array<juce::Rectangle<int>> rowRects;
        std::map<juce::String, std::unique_ptr<juce::Drawable>> artCache;
        grab::SegmentedControl          seg;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceMenu)
    };
}
