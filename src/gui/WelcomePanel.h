// WelcomePanel — the "how you can help" dialog shown on launch. GRAB is community-driven, so it
// explains, briefly and plainly, the single most useful thing a user can contribute (a Dashboard
// capture) plus the quick one-click option. Reopenable from Advanced ▸ How you can help.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Theme.h"

class WelcomePanel  : public juce::Component
{
public:
    WelcomePanel()
    {
        addAndMakeVisible (title);
        title.setText ("GRAB " + grab::u8 ("\xe2\x80\x94") + " how you can help", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (body);
        body.setMultiLine (true);
        body.setReadOnly (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setFont (juce::Font (juce::FontOptions (15.0f)));
        body.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        body.setColour (juce::TextEditor::textColourId,       grab::theme::textPrimary);
        body.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);
        body.setText (content(), false);

        addAndMakeVisible (guideBtn);
        guideBtn.setButtonText ("Open capture guide");
        guideBtn.setTooltip ("Opens the step-by-step Dashboard capture guide on GitHub in your browser.");
        guideBtn.onClick = [] { juce::URL (kGuideUrl).launchInDefaultBrowser(); };

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Got it");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (660, 600);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (grab::theme::windowBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (32);
        closeBtn.setBounds (bottom.removeFromRight (120));
        bottom.removeFromRight (8);
        guideBtn.setBounds (bottom.removeFromRight (170));
        r.removeFromBottom (8);
        body.setBounds (r);
    }

private:
    static juce::String content()
    {
        // Lines are word-wrapped by the editor, so each paragraph is one line (no mid-sentence breaks).
        const char* utf8 =
            "GRAB reads and writes ROLI device settings over MIDI \xe2\x80\x94 no ROLI software needed. It grows from "
            "what people with real hardware send in. Everything is anonymised, and nothing is sent unless you click.\n"
            "\n"
            "HOW YOU CAN HELP\n"
            "\n"
            "1.  Easy, one click \xe2\x80\x94 \"Contribute...\"\n"
            "     If GRAB shows your device and its settings, click Contribute to send an anonymised profile. We now "
            "have these for every ROLI family, so this is quick and always welcome for confirming control names and "
            "ranges.\n"
            "\n"
            "2.  The big one, a little technical \xe2\x80\x94 capture ROLI Dashboard\n"
            "     GRAB can read what a device reports, but it cannot see what ROLI Dashboard SENDS to the device "
            "when you change a setting \xe2\x80\x94 and that \"write\" traffic is how we unlock the features GRAB can't reach "
            "yet: the Lightpad's pads & modes, Note Grid / Live / Loop / Touch settings, and the exact name of every "
            "control. (It's exactly how the Seaboard GRAND went from a mystery to configurable.)\n"
            "     To capture it, run a USB/MIDI monitor next to Dashboard and change a few settings:\n"
            "       \xe2\x80\xa2  Windows:  Wireshark + USBPcap (both free) \xe2\x80\x94 record the Seaboard's USB while you edit "
            "in Dashboard.\n"
            "       \xe2\x80\xa2  macOS:  MIDI Monitor (Snoize, free) with Spy enabled on the output \xe2\x80\x94 record the "
            "\"to device\" stream.\n"
            "     Then save it and attach it to a GitHub issue. For the full step-by-step, click \"Open capture "
            "guide\" below (tools/CAPTURE-DASHBOARD.md). This is the single most valuable thing a technical user can "
            "send.\n"
            "     (GRAB's own Advanced \xe2\x96\xb8 Capture MIDI records and decodes the device side, which helps too, but "
            "Dashboard's writes only appear in a USB/MIDI monitor as above.)\n"
            "\n"
            "WHAT WE STILL NEED  (by device)\n"
            "     Seaboard Block / Block M, Seaboard RISE 25 / 49 / 2, ROLI Piano, LUMI Keys / Piano M "
            "\xe2\x80\x94 fully supported (read + live edit). Just Contribute a profile to confirm names and ranges.\n"
            "     Seaboard GRAND  \xe2\x80\x94 pedals, glide and transpose work. A GRAND owner could confirm the rest: in "
            "Dashboard, change the Pitch Bend Range, Octave, MIDI mode / channel, and each pedal's Type, then send a "
            "capture (or just test the latest build and tell us what's right / wrong).\n"
            "     Original Seaboard RISE 25 / 49  \xe2\x80\x94 one gap: set a Glide/Slide or Press Tracking to \"Disabled\" in "
            "Dashboard and capture it, so we can add that option.\n"
            "     Lightpad / Note Grid, and Live / Loop / Touch  \xe2\x80\x94 detection only. Their pad / mode settings need a "
            "Dashboard capture (part 2 above).\n"
            "     Airwave  \xe2\x80\x94 out of scope: a host-side gesture controller, no on-device settings to change.\n"
            "\n"
            "GOOD TO KNOW\n"
            "     Sharing the device:  on Windows 11 25H2+, MIDI is multi-client by default, so GRAB, ROLI Dashboard "
            "and your DAW can all use the device at once. On older Windows (the legacy stack) quit Dashboard and your "
            "DAW before opening GRAB. Mac and Linux already share. For the Dashboard capture above you WANT both open "
            "at once.\n"
            "     Archive firmware:  Advanced \xe2\x96\xb8 Download firmware for archiving grabs a copy of your Block / RISE "
            "firmware while ROLI's servers are still up.\n"
            "     Private & safe:  serials are hashed, serial & MAC are stripped from captures, and nothing leaves "
            "your machine until you click. GRAB never flashes firmware.\n"
            "\n"
            "Thanks for helping!  Reopen this any time from Advanced \xe2\x96\xb8 How you can help.";
        return juce::String::fromUTF8 (utf8);
    }

    static constexpr const char* kGuideUrl =
        "https://github.com/demixme/GRAB/blob/main/tools/CAPTURE-DASHBOARD.md";

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton guideBtn, closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePanel)
};
