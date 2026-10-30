// AirwaveTesterPanel - an EXPERIMENTAL Airwave Expression emulator.
//
// A real Airwave sends its six Dimensions of Air as a fixed set of MIDI CCs (CC21-31) on a device
// called "ROLI Airwave Expression" - see AirwavePanel / the decoded manual. This panel reproduces
// exactly those CCs from sliders, so anyone can map and test Airwave gestures in a DAW or plug-in
// WITHOUT owning an Airwave. It sends only standard CC messages to a MIDI output you pick; it reads
// no hardware and needs no SDK. (The real camera->CC path needs the Ultraleap SDK + a device; this
// is the output half, which we can ship today.)

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>

#include "Theme.h"

class AirwaveTesterPanel  : public juce::Component
{
public:
    // CC -> gesture, exactly as ROLI Airwave Expression sends them (decoded from the Airwave manual).
    struct Row { int cc; const char* name; };
    static constexpr std::array<Row, 11> kRows { {
        { 21, "Grasp  (both hands summed)" },
        { 22, "Slide  Left" },  { 23, "Slide  Right" },
        { 24, "Glide  Left" },  { 25, "Glide  Right" },
        { 26, "Raise  Left" },  { 27, "Raise  Right" },
        { 28, "Tilt   Left" },  { 29, "Tilt   Right" },
        { 30, "Flex   Left" },  { 31, "Flex   Right" } } };

    AirwaveTesterPanel()
    {
        addAndMakeVisible (title);
        title.setText ("Airwave Expression emulator " + grab::u8 ("-") + " experimental",
                       juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (18.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (intro);
        intro.setJustificationType (juce::Justification::topLeft);
        intro.setColour (juce::Label::textColourId, grab::theme::textPrimary.withAlpha (0.8f));
        intro.setFont (juce::Font (juce::FontOptions (13.0f)));
        intro.setText ("Sends the exact CC21-31 a real ROLI Airwave emits, so you can MIDI-map and test "
                       "Airwave gestures in your DAW or plug-in without the hardware. Pick an output, then move "
                       "the sliders. On Windows, route into a DAW via a virtual port (e.g. loopMIDI); on macOS "
                       "use the IAC Driver.", juce::dontSendNotification);

        addAndMakeVisible (outputLabel);
        outputLabel.setText ("MIDI output:", juce::dontSendNotification);
        outputLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (outputBox);
        refreshOutputs();
        outputBox.onChange = [this] { openSelectedOutput(); };

        addAndMakeVisible (refreshBtn);
        refreshBtn.setButtonText ("Refresh");
        refreshBtn.onClick = [this] { refreshOutputs(); };

        addAndMakeVisible (channelLabel);
        channelLabel.setText ("Channel:", juce::dontSendNotification);
        channelLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (channelBox);
        for (int ch = 1; ch <= 16; ++ch) channelBox.addItem (juce::String (ch), ch);
        channelBox.setSelectedId (1, juce::dontSendNotification);

        for (size_t i = 0; i < kRows.size(); ++i)
        {
            auto& nl = names[i];
            addAndMakeVisible (nl);
            nl.setText ("CC" + juce::String (kRows[i].cc) + "   " + kRows[i].name, juce::dontSendNotification);
            nl.setFont (juce::Font (juce::FontOptions (13.0f)));

            auto& s = sliders[i];
            addAndMakeVisible (s);
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setRange (0.0, 127.0, 1.0);
            s.setValue (0.0, juce::dontSendNotification);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 22);
            const int cc = kRows[i].cc;
            s.onValueChange = [this, cc, idx = i] { sendCc (cc, (int) sliders[idx].getValue()); };
        }

        addAndMakeVisible (resetBtn);
        resetBtn.setButtonText ("Reset to 0");
        resetBtn.onClick = [this]
        {
            for (auto& s : sliders) s.setValue (0.0, juce::dontSendNotification);
            for (auto& r : kRows)   sendCc (r.cc, 0);
        };

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (520, 560);
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (26));
        r.removeFromTop (4);
        intro.setBounds (r.removeFromTop (58));
        r.removeFromTop (6);

        auto outRow = r.removeFromTop (26);
        outputLabel.setBounds (outRow.removeFromLeft (84));
        refreshBtn.setBounds  (outRow.removeFromRight (74));
        outRow.removeFromRight (6);
        outputBox.setBounds   (outRow);
        r.removeFromTop (6);

        auto chRow = r.removeFromTop (26);
        channelLabel.setBounds (chRow.removeFromLeft (84));
        channelBox.setBounds   (chRow.removeFromLeft (70));
        r.removeFromTop (8);

        auto bottom = r.removeFromBottom (30);
        closeBtn.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        resetBtn.setBounds (bottom.removeFromRight (100));
        r.removeFromBottom (8);

        for (size_t i = 0; i < kRows.size(); ++i)
        {
            auto row = r.removeFromTop (28);
            names[i].setBounds (row.removeFromLeft (170));
            sliders[i].setBounds (row);
            r.removeFromTop (2);
        }
    }

private:
    void refreshOutputs()
    {
        const auto wanted = outputBox.getText();
        outputBox.clear (juce::dontSendNotification);
        outputs = juce::MidiOutput::getAvailableDevices();
        outputBox.addItem ("(none)", 1);
        for (int i = 0; i < outputs.size(); ++i)
            outputBox.addItem (outputs[i].name, i + 2);

        // Keep the previous selection if it's still present, else default to (none).
        int sel = 1;
        for (int i = 0; i < outputs.size(); ++i)
            if (outputs[i].name == wanted) sel = i + 2;
        outputBox.setSelectedId (sel, juce::dontSendNotification);
        openSelectedOutput();
    }

    void openSelectedOutput()
    {
        out.reset();
        const int id = outputBox.getSelectedId();
        if (id >= 2 && id - 2 < outputs.size())
            out = juce::MidiOutput::openDevice (outputs[id - 2].identifier);
    }

    void sendCc (int cc, int value)
    {
        if (out == nullptr) return;
        const int ch = juce::jlimit (1, 16, channelBox.getSelectedId());
        out->sendMessageNow (juce::MidiMessage::controllerEvent (ch, cc, juce::jlimit (0, 127, value)));
    }

    juce::Label      title, intro, outputLabel, channelLabel;
    juce::ComboBox   outputBox, channelBox;
    juce::TextButton refreshBtn, resetBtn, closeBtn;
    juce::Array<juce::MidiDeviceInfo> outputs;
    std::unique_ptr<juce::MidiOutput> out;
    std::array<juce::Label, 11>  names;
    std::array<juce::Slider, 11> sliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirwaveTesterPanel)
};
