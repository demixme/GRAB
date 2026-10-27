// FaderPanel — the RISE "MIDI CC Assignments" section, echoing the hardware: three touch faders
// and the XY pad. Each control shows a LIVE meter (its assigned CC's current value, animated as
// you play, like ROLI Dashboard) next to an editable assignment. The faders set which CC they
// send (drag; double-click reverts); the XY pad shows a live 2D dot with editable X/Y CC numbers.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

//==============================================================================
/** One touch fader: a thin live meter (finger position) beside a vertical slider that sets the
    assigned CC number. */
class Fader  : public juce::Component,
               public juce::SettableTooltipClient
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

    int  assignedCc() const { return juce::jlimit (0, 127, (int) juce::roundToInt (slider.getValue())); }
    void updateLive (const int* live)   // live[0..127]; -1 = no data yet
    {
        const int v = live[(size_t) assignedCc()];
        if (v != liveVal) { liveVal = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        if (changed)
        {
            g.setColour (juce::Colours::orange.withAlpha (0.14f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }
        // Live meter: a thin bar on the left that fills to the assigned CC's current value.
        auto bar = meterBounds().toFloat();
        g.setColour (grab::theme::panelBgAlt);
        g.fillRoundedRectangle (bar, 3.0f);
        if (liveVal >= 0)
        {
            auto fill = bar.reduced (1.5f);
            fill = fill.removeFromBottom (fill.getHeight() * (float) liveVal / 127.0f);
            g.setColour (grab::theme::accent);
            g.fillRoundedRectangle (fill, 2.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        nameLabel.setBounds (b.removeFromTop (16));
        meter = b.removeFromLeft (8).reduced (1, 2);
        slider.setBounds (b.reduced (2, 2));
    }

private:
    juce::Rectangle<int> meterBounds() const { return meter; }

    juce::uint32 id;
    juce::Label  nameLabel;
    juce::Slider slider;
    juce::Rectangle<int> meter;
    ChangeFn     change;
    bool         changed = false;
    int          liveVal = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fader)
};

//==============================================================================
/** The XY pad: a square with a live dot at (X CC value, Y CC value), plus two editable CC-number
    fields for the X and Y axes. */
class XYPad  : public juce::Component,
               public juce::SettableTooltipClient
{
public:
    using ChangeFn = std::function<void (juce::uint32, juce::int32)>;

    XYPad (juce::uint32 xIdToUse, juce::uint32 yIdToUse, int xCc, int yCc, ChangeFn onChange)
        : xId (xIdToUse), yId (yIdToUse), change (std::move (onChange))
    {
        title.setText ("XY Pad", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (juce::Font (juce::FontOptions (11.0f)));
        title.setColour (juce::Label::textColourId, grab::theme::textSecondary);
        addAndMakeVisible (title);

        auto setupField = [this] (juce::Label& lab, const juce::String& prefix, int cc, juce::uint32 cid)
        {
            lab.setJustificationType (juce::Justification::centred);
            lab.setFont (juce::Font (juce::FontOptions (12.0f)));
            lab.setColour (juce::Label::backgroundColourId, grab::theme::panelBgAlt);
            lab.setColour (juce::Label::textColourId, grab::theme::textPrimary);
            lab.setText (prefix + " " + juce::String (cc), juce::dontSendNotification);
            lab.setEditable (true);
            lab.onTextChange = [this, &lab, prefix, cid]
            {
                const int v = juce::jlimit (0, 127, lab.getText().retainCharacters ("0123456789").getIntValue());
                lab.setText (prefix + " " + juce::String (v), juce::dontSendNotification);
                if (change != nullptr) change (cid, v);
            };
            addAndMakeVisible (lab);
        };
        setupField (xField, "X", xCc, xId);
        setupField (yField, "Y", yCc, yId);
    }

    void setXy (int xCc, int yCc)
    {
        xField.setText ("X " + juce::String (xCc), juce::dontSendNotification);
        yField.setText ("Y " + juce::String (yCc), juce::dontSendNotification);
        xAssign = xCc; yAssign = yCc;
    }

    void updateLive (const int* live)
    {
        const int nx = live[(size_t) juce::jlimit (0, 127, xAssign)];
        const int ny = live[(size_t) juce::jlimit (0, 127, yAssign)];
        if (nx != liveX || ny != liveY) { liveX = nx; liveY = ny; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto pr = padBounds().toFloat();
        g.setColour (grab::theme::curveBg);
        g.fillRoundedRectangle (pr, 4.0f);
        g.setColour (grab::theme::curveGrid);
        for (int i = 1; i < 4; ++i)
        {
            const float fx = pr.getX() + pr.getWidth()  * (i / 4.0f);
            const float fy = pr.getY() + pr.getHeight() * (i / 4.0f);
            g.drawLine (fx, pr.getY(), fx, pr.getBottom(), 0.5f);
            g.drawLine (pr.getX(), fy, pr.getRight(), fy, 0.5f);
        }
        if (liveX >= 0 && liveY >= 0)
        {
            const float px = pr.getX() + pr.getWidth()  * (float) liveX / 127.0f;
            const float py = pr.getBottom() - pr.getHeight() * (float) liveY / 127.0f;
            g.setColour (grab::theme::accent);
            g.fillEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds (b.removeFromTop (16));
        auto fields = b.removeFromBottom (22);
        xField.setBounds (fields.removeFromLeft (fields.getWidth() / 2).reduced (2, 1));
        yField.setBounds (fields.reduced (2, 1));
        pad = b.reduced (4, 2);
    }

private:
    juce::Rectangle<int> padBounds() const { return pad; }

    juce::uint32 xId, yId;
    juce::Label  title, xField, yField;
    juce::Rectangle<int> pad;
    ChangeFn     change;
    int          xAssign = 0, yAssign = 0, liveX = -1, liveY = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XYPad)
};

//==============================================================================
/** Holds the three faders and the XY pad, and polls the live-CC array (~30 fps) to animate them. */
class FaderPanel  : public juce::Component,
                    private juce::Timer
{
public:
    FaderPanel() = default;
    ~FaderPanel() override { stopTimer(); }

    Fader* add (std::unique_ptr<Fader> f)
    {
        auto* p = f.get();
        addAndMakeVisible (*f);
        faders.push_back (std::move (f));
        return p;
    }

    XYPad* setXYPad (std::unique_ptr<XYPad> p)
    {
        xy = std::move (p);
        addAndMakeVisible (*xy);
        return xy.get();
    }

    /** Point the panel at the live-CC array owned by the page; starts the animation timer. */
    void setLiveSource (const int* liveCcArray)
    {
        live = liveCcArray;
        if (live != nullptr) startTimerHz (30);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int n = (int) faders.size();
        // XY pad takes the right ~40%; the faders share the left.
        auto padArea = (xy != nullptr) ? b.removeFromRight (juce::jmax (90, b.getWidth() * 2 / 5)) : juce::Rectangle<int>();
        if (xy != nullptr) { b.removeFromRight (6); xy->setBounds (padArea); }
        if (n > 0)
        {
            const int w = b.getWidth() / n;
            for (int i = 0; i < n; ++i)
                faders[(size_t) i]->setBounds (b.getX() + i * w, b.getY(), w, b.getHeight());
        }
    }

    std::vector<std::unique_ptr<Fader>> faders;
    std::unique_ptr<XYPad> xy;

private:
    void timerCallback() override
    {
        if (live == nullptr) return;
        for (auto& f : faders) f->updateLive (live);
        if (xy != nullptr)    xy->updateLive (live);
    }

    const int* live = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaderPanel)
};
