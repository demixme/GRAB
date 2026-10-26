// FaderPanel — a row of vertical faders for the RISE "MIDI CC Assignments" section, echoing
// the hardware (three touch faders + XY pad axes). Each fader sets which MIDI CC that control
// sends (0..127); drag to change, double-click to revert to the device value.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

class Fader  : public juce::Component
{
public:
    using ChangeFn = std::function<void (juce::uint32, juce::int32)>;

    Fader (juce::uint32 idToUse, juce::String label, int value, int lo, int hi, ChangeFn onChange)
        : id (idToUse), change (std::move (onChange))
    {
        nameLabel.setText (label, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        nameLabel.setColour (juce::Label::textColourId, grab::theme::textSecondary);
        addAndMakeVisible (nameLabel);

        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 20);
        slider.setRange ((double) lo, (double) hi, 1.0);
        slider.setValue ((double) value, juce::dontSendNotification);
        slider.setDoubleClickReturnValue (true, (double) value);
        slider.onValueChange = [this]
        {
            if (change != nullptr)
                change (id, (juce::int32) juce::roundToInt (slider.getValue()));
        };
        addAndMakeVisible (slider);
    }

    juce::uint32 getId() const { return id; }
    void setValue (int v) { slider.setValue ((double) v, juce::dontSendNotification); }
    void setChanged (bool c) { if (changed != c) { changed = c; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        if (changed)
        {
            g.setColour (juce::Colours::orange.withAlpha (0.14f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        nameLabel.setBounds (b.removeFromTop (16));
        slider.setBounds (b.reduced (4, 2));
    }

private:
    juce::uint32 id;
    juce::Label  nameLabel;
    juce::Slider slider;
    ChangeFn     change;
    bool         changed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fader)
};

//==============================================================================
class FaderPanel  : public juce::Component
{
public:
    FaderPanel() = default;

    Fader* add (std::unique_ptr<Fader> f)
    {
        auto* p = f.get();
        addAndMakeVisible (*f);
        faders.push_back (std::move (f));
        return p;
    }

    void resized() override
    {
        const int n = (int) faders.size();
        if (n == 0) return;
        const int w = getWidth() / n;
        for (int i = 0; i < n; ++i)
            faders[(size_t) i]->setBounds (i * w, 0, w, getHeight());
    }

    std::vector<std::unique_ptr<Fader>> faders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderPanel)
};
