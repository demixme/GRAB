// ColourWell: an editable colour config item (e.g. Global / Root Key Color), shown the way
// macOS shows a colour well — a swatch plus its hex code. Clicking it (or Space/Return when
// focused) opens a popover with a colour picker and a row of quick presets; every change is
// reported live, so the keys follow as you drag (the owner's ThrottledWriter coalesces the
// writes). The device stores 32-bit ARGB: the well keeps whatever the device reported (so a
// right-click reset restores it exactly), and edits made in the picker are written opaque.

#pragma once

#include "Theme.h"

#include <functional>

namespace grab
{

class ColourWell  : public juce::Button
{
public:
    std::function<void (juce::Colour)> onColourChanged;   // fired on every edit in the popover

    ColourWell() : juce::Button ("Colour")
    {
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSwatchColour (juce::Colour c)
    {
        colour = c;
        setTitle ("Colour " + hexOf (colour));   // read out by screen readers, not just shown
        repaint();
    }

    juce::Colour getSwatchColour() const { return colour; }

    static juce::String hexOf (juce::Colour c) { return "#" + c.toDisplayString (false); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        using namespace grab::theme;
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        // Swatch: fixed width on the left, hex code beside it so the value never relies on
        // colour alone (and stays readable for very dark or very light colours).
        auto sw = r.removeFromLeft (juce::jmin (52.0f, r.getWidth() * 0.5f)).reduced (0.0f, 3.0f);
        g.setColour (colour.withAlpha (1.0f));
        g.fillRoundedRectangle (sw, 5.0f);
        g.setColour (highlighted || down ? accent : outline.brighter (0.4f));
        g.drawRoundedRectangle (sw, 5.0f, highlighted || down ? 1.5f : 1.0f);

        if (hasKeyboardFocus (true))
        {
            g.setColour (accent);
            g.drawRoundedRectangle (sw.expanded (2.5f), 7.0f, 2.0f);
        }

        r.removeFromLeft (10.0f);
        g.setColour (isEnabled() ? textPrimary : textSecondary);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (hexOf (colour), r, juce::Justification::centredLeft, false);
    }

    void clicked (const juce::ModifierKeys& mods) override
    {
        if (mods.isPopupMenu())   // right-click is the row's "reset to device value", not "open"
            return;

        // Fit the popover on small or scaled displays: the picker shrinks, the presets don't.
        int height = 430;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            height = juce::jlimit (300, 430, d->userBounds.getLargestIntegerWithin().getHeight() - 80);

        auto panel = std::make_unique<Panel> (colour, height, juce::Component::SafePointer<ColourWell> (this));
        juce::CallOutBox::launchAsynchronously (std::move (panel), getScreenBounds(), nullptr);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (isEnabled() && key.isKeyCode (juce::KeyPress::spaceKey))   // Button only handles Return
        {
            triggerClick();
            return true;
        }
        return juce::Button::keyPressed (key);
    }

private:
    // A preset in the popover: painted as its own colour (the app LookAndFeel paints every
    // TextButton in the panel colour), with a hover outline and a keyboard focus ring.
    class PresetSwatch  : public juce::Button
    {
    public:
        PresetSwatch (const char* name, juce::Colour c) : juce::Button (name), colour (c)
        {
            setTooltip (name);
            setTitle (name);
            setWantsKeyboardFocus (true);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            using namespace grab::theme;
            auto r = getLocalBounds().toFloat().reduced (2.5f);
            g.setColour (colour);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (highlighted || down ? accent : outline.brighter (0.4f));
            g.drawRoundedRectangle (r, 4.0f, highlighted || down ? 1.5f : 1.0f);
            if (hasKeyboardFocus (true))
            {
                g.setColour (accent);
                g.drawRoundedRectangle (r.expanded (1.5f), 5.0f, 2.0f);
            }
        }

        const juce::Colour colour;
    };

    // Popover content: HSV picker + RGB sliders, and quick presets underneath.
    class Panel  : public juce::Component,
                   private juce::ChangeListener
    {
    public:
        Panel (juce::Colour start, int height, juce::Component::SafePointer<ColourWell> ownerToUse)
            : owner (ownerToUse),
              selector (juce::ColourSelector::showColourAtTop
                        | juce::ColourSelector::showSliders
                        | juce::ColourSelector::showColourspace)
        {
            using namespace grab::theme;
            selector.setColour (juce::ColourSelector::backgroundColourId, panelBg);
            selector.setColour (juce::ColourSelector::labelTextColourId, textSecondary);
            selector.setCurrentColour (start, juce::dontSendNotification);
            selector.addChangeListener (this);
            addAndMakeVisible (selector);

            for (const auto& p : presets())
            {
                auto b = std::make_unique<PresetSwatch> (p.name, p.colour);
                // Set the selector silently, then report once: a notifying set updates the
                // R/G/B sliders one at a time, and each re-reads the others while they still
                // hold the old colour (White -> Blue came out cyan).
                b->onClick = [this, c = p.colour]
                {
                    selector.setCurrentColour (c, juce::dontSendNotification);
                    report();
                };
                addAndMakeVisible (*b);
                presetButtons.push_back (std::move (b));
            }

            presetLabel.setText ("Presets", juce::dontSendNotification);
            presetLabel.setColour (juce::Label::textColourId, textSecondary);
            addAndMakeVisible (presetLabel);

            hint.setText (grab::u8 ("Visible when the active mode's Color Scheme is User."),
                          juce::dontSendNotification);
            hint.setColour (juce::Label::textColourId, textSecondary);
            hint.setFont (juce::FontOptions (13.0f));
            addAndMakeVisible (hint);

            setSize (300, height);
        }

        ~Panel() override { selector.removeChangeListener (this); }

        void paint (juce::Graphics& g) override { g.fillAll (grab::theme::panelBg); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10);
            hint.setBounds (r.removeFromBottom (20));
            r.removeFromBottom (6);

            auto row = r.removeFromBottom (32);
            presetLabel.setBounds (r.removeFromBottom (20));
            r.removeFromBottom (2);

            const int n = (int) presetButtons.size();
            const int gap = 6;
            const int w = (row.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
            for (auto& b : presetButtons)
            {
                b->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (gap);
            }

            r.removeFromBottom (8);
            selector.setBounds (r);
        }

    private:
        struct Preset { const char* name; juce::Colour colour; };

        static const std::vector<Preset>& presets()
        {
            static const std::vector<Preset> p = {
                { "White",   juce::Colour (0xffffffff) }, { "Red",     juce::Colour (0xffff0000) },
                { "Orange",  juce::Colour (0xffff8000) }, { "Yellow",  juce::Colour (0xffffff00) },
                { "Green",   juce::Colour (0xff00ff00) }, { "Cyan",    juce::Colour (0xff00ffff) },
                { "Blue",    juce::Colour (0xff0000ff) }, { "Magenta", juce::Colour (0xffff00ff) } };
            return p;
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override { report(); }

        void report()
        {
            const auto c = selector.getCurrentColour().withAlpha (1.0f);
            if (auto* w = owner.getComponent())
            {
                w->setSwatchColour (c);
                if (w->onColourChanged)
                    w->onColourChanged (c);
            }
        }

        juce::Component::SafePointer<ColourWell> owner;   // the row may be rebuilt while open
        juce::ColourSelector selector;
        juce::Label presetLabel, hint;
        std::vector<std::unique_ptr<PresetSwatch>> presetButtons;
    };

    juce::Colour colour { juce::Colours::black };
};

} // namespace grab
