// WelcomePanel — the "how you can help" dialog shown on launch. GRAB is community-driven, so it
// explains, plainly and device-first, exactly what each kind of device needs and WHERE to click
// in GRAB to do it. Styled (headings / emphasis / body) for skimming. Reopenable from
// Advanced ▸ How you can help.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Theme.h"

class WelcomePanel  : public juce::Component
{
public:
    WelcomePanel()
    {
        addAndMakeVisible (title);
        title.setText ("GRAB " + grab::u8 ("-") + " how you can help", juce::dontSendNotification);
        title.setFont (grab::theme::type::fontAt (20.0f, 2));
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (body);
        body.setMultiLine (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        body.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);
        populate();

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

        setSize (680, 620);
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
    // Text styles for the run-based layout (so the dialog has real headings and is skimmable).
    enum Style { Body, H1, H2, Click, Note };

    void add (Style s, const char* utf8)
    {
        juce::Font   f = grab::theme::type::fontAt (15.5f);
        juce::Colour col = grab::theme::textPrimary;
        switch (s)
        {
            case H1:    f = grab::theme::type::fontAt (18.0f, 2); col = grab::theme::accent;        break;
            case H2:    f = grab::theme::type::fontAt (15.5f, 2);                                   break;
            case Click: col = grab::theme::accent;                                                 break;
            case Note:  f = grab::theme::type::fontAt (15.5f); col = grab::theme::textPrimary.withAlpha (0.72f); break;
            case Body:  default: break;
        }
        body.setFont (f);
        body.setColour (juce::TextEditor::textColourId, col);
        body.insertTextAtCaret (juce::String::fromUTF8 (utf8));
    }

    void populate()
    {
        add (Body,  "GRAB reads and writes ROLI device settings over MIDI - no ROLI software needed. It grows "
                    "from what people with real hardware send in, all anonymised; nothing is sent until you click.\n\n");

        // --- device-first: what's supported, what's still wanted --------------------------------
        add (H1,    "What does your device need?\n");
        add (H2,    "Fully supported:");
        add (Body,  "  Seaboard Block / Block M, RISE 25 / 49 / 2, GRAND, ROLI Piano, Piano M / LUMI Keys, and "
                    "Control Blocks (Live / Loop / Touch). They read + live-edit - send a profile (way 1) to "
                    "confirm names & ranges.\n\n");

        add (H2,    "  >> Most wanted: a first-edition Seaboard GRAND <<\n");
        add (Body,  "    GRAB's GRAND support is built from one contributor's capture of a later board. If you have "
                    "an original (first-edition) Seaboard GRAND, a ");
        add (Click, "Capture MIDI");
        add (Body,  " or Dashboard capture (ways 2 & 3) confirms its settings on the earlier hardware.\n\n");

        add (H2,    "Work in progress:");
        add (Body,  "  Lightpad Block / Pad - settings read + live-edit; its apps (Note Grid, Fader, Drum...) "
                    "preview inside GRAB and can be loaded onto a connected Lightpad (experimental, in Advanced).\n");
        add (Body,  "  Airwave is out of scope - it only sends MIDI CCs, so there's nothing on the device to "
                    "configure (notes + a CC tester live under Advanced).\n\n");

        // --- the three ways, easiest first ------------------------------------------------------
        add (H1,    "Three ways to help (easiest first)\n");

        add (H2,    "1.  Send a profile - one click.  ");
        add (Click, "Contribute...");
        add (Body,  "  (top row) sends the ids, names, ranges and your values, anonymised, so controls get labelled "
                    "for everyone. Any supported device, ~30 seconds.\n\n");

        add (H2,    "2.  Capture MIDI - built in.  ");
        add (Click, "Advanced... > Capture MIDI");
        add (Body,  "  records what your device SENDS, decoded to plain English, then Submit / Save / open an issue. "
                    "Shows what a not-yet-mapped device does: play it, press its buttons, switch its modes. ");
        add (Click, "Advanced... > Help add device");
        add (Body,  " is a one-click version for a device GRAB doesn't recognise at all.\n\n");

        add (H2,    "3.  Dashboard capture - a little technical, unlocks writing.  ");
        add (Body,  "records what ROLI Dashboard SENDS to the device - the only way to add WRITING for a new setting "
                    "or device (it's how the GRAND became configurable). Run a free USB/MIDI monitor beside Dashboard "
                    "(Wireshark + USBPcap on Windows, MIDI Monitor on macOS); full steps in \"Open capture guide\" "
                    "below.\n\n");

        // --- good to know -----------------------------------------------------------------------
        add (H1,    "Good to know\n");
        add (Body,  "    Multiple devices: plug in several and the ");
        add (Click, "ROLI device");
        add (Body,  " dropdown (top row) switches between them - no restart or power-cycle.\n");
        add (Body,  "    Windows Bluetooth: ROLI BLE-MIDI needs Windows MIDI Services (Windows 11 25H2+); otherwise "
                    "use USB, which works on every Windows version.\n");
        add (Click, "    Won't connect?  Diagnose...");
        add (Body,  " (top row) lists the MIDI ports GRAB sees and flags one held by another app.\n");
        add (Body,  "    Archiving: Advanced can download your Block / RISE firmware and the ROLI software your "
                    "account owns while ROLI's servers are up - download-only, never flashes or uploads.\n");
        add (Body,  "    Private & safe: serials hashed, serial & MAC stripped from captures, nothing sent until you "
                    "click, and GRAB never flashes firmware.\n\n");

        // --- credits ----------------------------------------------------------------------------
        add (H1,    "With thanks to our contributors\n");
        add (Body,  "    GRAB's device support is built from captures and testing that people with real "
                    "hardware sent in:\n");
        add (Body,  "       sbialy - Seaboard RISE 2 protocol, 5D sensitivity curves, anonymisation\n");
        add (Body,  "       fictionfred - UI design; original RISE 25 write protocol\n");
        add (Body,  "       BabeleDunnit - Seaboard GRAND configuration\n");
        add (Body,  "       lluisgerard - Piano M / LUMI Keys (incl. a chained pair)\n");
        add (Body,  "       gofer723 - Seaboard RISE 2 Multi / Single Channel\n");
        add (Body,  "       LucidCZ - Seaboard RISE 2 settings confirmation (octave, transpose, tracking, 5D)\n");
        add (Body,  "       Harry Triendl - Seaboard RISE 25 captures\n");
        add (Body,  "       WillEarlDE - Seaboard RISE 49 testing\n");
        add (Body,  "       Roger - confirmed BLOCKS settings travel over USB-MIDI; app-loader testing\n");
        add (Body,  "       moofy - Lightpad Block app-load capture; Control Block read+write confirmation (Live/Loop)\n");
        add (Body,  "       Pete Brown (Microsoft) - Windows 11 multi-client MIDI\n");
        add (Body,  "       Legitimate_Horror_72 - ROLI Piano scale names & firmware scale lighting\n");
        add (Body,  "    ...and everyone who sent an anonymous profile or capture - GRAB's device support "
                    "grows from your data.  Missed from this list? Let us know and we'll add you.\n\n");

        add (Body,  "Thanks for helping!  Reopen any time from Advanced > How you can help.");

        body.setReadOnly (true);
        body.setCaretPosition (0);   // scroll back to the top after building
    }

    static constexpr const char* kGuideUrl =
        "https://github.com/demixme/GRAB/blob/main/tools/CAPTURE-DASHBOARD.md";

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton guideBtn, closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePanel)
};
