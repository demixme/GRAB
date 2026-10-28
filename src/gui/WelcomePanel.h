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

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Got it");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (640, 560);
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
        closeBtn.setBounds (r.removeFromBottom (32).removeFromRight (120));
        r.removeFromBottom (8);
        body.setBounds (r);
    }

private:
    static juce::String content()
    {
        // Lines are word-wrapped by the editor, so each paragraph is one line (no mid-sentence breaks).
        const char* utf8 =
            "GRAB reads and writes ROLI device settings over MIDI \xe2\x80\x94 no ROLI software needed. It grows from "
            "what people with real hardware send in, so your reports are how new devices and features get added. "
            "Everything is anonymised, and nothing is sent unless you click.\n"
            "\n"
            "HOW YOU CAN HELP\n"
            "\n"
            "1.  One click \xe2\x80\x94 \"Contribute...\"\n"
            "     If GRAB shows your device and its settings, click Contribute to send an anonymised profile. That's it.\n"
            "\n"
            "2.  The one that matters most \xe2\x80\x94 help us ADD a device\n"
            "     GRAB can't yet see the special settings on some instruments: LUMI scales & lighting, the Lightpad's "
            "pads & modes, the Seaboard GRAND's full settings. The only way we can add them is to see what ROLI "
            "Dashboard does when you change them.\n"
            "     So if you own one, a short ROLI Dashboard capture is the most useful thing you can send \xe2\x80\x94 it's "
            "exactly how we added the GRAND. It's quick (about 10 minutes), passive, and read-only: it never touches "
            "your device or its firmware. Steps are in tools/CAPTURE-DASHBOARD.md (a free MIDI monitor on Mac, "
            "Wireshark on Windows) \xe2\x80\x94 record it while you change a few settings, then attach it to a GitHub issue "
            "and we do the rest.\n"
            "\n"
            "Can't see your device?  Click \"Diagnose...\" for connection help, or \"Help add device...\" to send a "
            "one-click report.\n"
            "\n"
            "WHERE THINGS STAND\n"
            "     Working now:  Seaboard Block / Block M, Seaboard RISE (25 / 49 / 2), ROLI Piano.\n"
            "     Needs your help:  LUMI Keys, Lightpad (and Note Grid / Live / Loop / Touch), Seaboard GRAND \xe2\x80\x94 "
            "they connect, but their special settings need a Dashboard capture (above).\n"
            "\n"
            "GOOD TO KNOW\n"
            "     One app at a time:  quit ROLI Dashboard and your DAW before opening GRAB \xe2\x80\x94 unless you run the new "
            "Windows MIDI Services, which lets them share (Mac and Linux already share).\n"
            "     Archive firmware:  Advanced \xe2\x96\xb8 Download firmware for archiving grabs a copy of your Block / RISE "
            "firmware while ROLI's servers are still up.\n"
            "     Private & safe:  serials are hashed, serial & MAC are stripped from captures, and nothing leaves "
            "your machine until you click. GRAB never flashes firmware.\n"
            "\n"
            "Thanks for helping!  Reopen this any time from Advanced \xe2\x96\xb8 How you can help.";
        return juce::String::fromUTF8 (utf8);
    }

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePanel)
};
