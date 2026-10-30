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
        title.setText ("GRAB " + grab::u8 ("\xe2\x80\x94") + " how you can help", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
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
        juce::Font   f (juce::FontOptions (15.0f));
        juce::Colour col = grab::theme::textPrimary;
        switch (s)
        {
            case H1:    f = juce::Font (juce::FontOptions (17.0f)).boldened(); col = grab::theme::accent;        break;
            case H2:    f = juce::Font (juce::FontOptions (15.0f)).boldened();                                   break;
            case Click: col = grab::theme::accent;                                                               break;
            case Note:  f = juce::Font (juce::FontOptions (13.5f)); col = grab::theme::textPrimary.withAlpha (0.72f); break;
            case Body:  default: break;
        }
        body.setFont (f);
        body.setColour (juce::TextEditor::textColourId, col);
        body.insertTextAtCaret (juce::String::fromUTF8 (utf8));
    }

    void populate()
    {
        add (Body,  "GRAB reads and writes ROLI device settings over MIDI \xe2\x80\x94 no ROLI software needed. It "
                    "grows from what people with real hardware send in. Everything is anonymised; nothing is sent "
                    "until you click.\n\n");

        // --- device-first: find yourself, see which "way" (below) helps -------------------------
        add (H1,    "What does your device need?\n");
        add (H2,    "Fully supported:");
        add (Body,  "  Seaboard Block / Block M, RISE 25 / 49 / 2, GRAND, ROLI Piano, Piano M / LUMI Keys.\n");
        add (Body,  "    They read + live-edit fully \xe2\x80\x94 just send a profile (way 1) to confirm names & ranges.\n\n");

        add (H2,    "Work in progress:");
        add (Body,  "  Lightpad Block / Pad \xe2\x80\x94 its settings work, but its apps (Note Grid, Fader, Drum...) don't.\n");
        add (Body,  "    Live / Loop / Touch \xe2\x80\x94 basic settings only.\n");
        add (Body,  "    These grow from a Capture MIDI (way 2) and/or a Dashboard capture (way 3).\n\n");

        add (H2,    "Not recognised / out of scope:");
        add (Body,  "  an unknown device \xe2\x80\x94 use Help add device (way 2). Airwave \xe2\x80\x94 host-side, nothing to "
                    "configure.\n\n");

        // --- the three ways, easiest first ------------------------------------------------------
        add (H1,    "Three ways to help (easiest first)\n");

        add (H2,    "1.  Send a profile \xe2\x80\x94 one click\n");
        add (Click, "    Contribute...  (top row, on the right)\n");
        add (Body,  "    Sends the settings your device reports (ids, names, ranges, your values), anonymised, so "
                    "controls get labelled correctly for everyone. Any supported device. ~30 seconds.\n\n");

        add (H2,    "2.  Capture MIDI \xe2\x80\x94 built into GRAB, no extra software\n");
        add (Click, "    Advanced... \xe2\x96\xb8 Capture MIDI\n");
        add (Body,  "    Records everything your device SENDS and decodes it to plain English (Time / Kind / Message "
                    "/ Value / Raw), then Submit, Save, or open an issue. Use it to show what a not-yet-supported "
                    "device actually does: play it, touch it, press its buttons, switch its modes. ");
        add (Note,  "(This is the device side \xe2\x80\x94 it can't see what Dashboard sends TO the device; that's way 3.)\n");
        add (Body,  "    For a device GRAB doesn't recognise at all, ");
        add (Click, "Advanced... \xe2\x96\xb8 Help add device ");
        add (Body,  "does an automatic, one-click version of this.\n\n");

        add (H2,    "3.  Dashboard capture \xe2\x80\x94 ~10 min, a little technical, unlocks the most\n");
        add (Body,  "    Records what ROLI Dashboard SENDS to the device when you change a setting \xe2\x80\x94 the write "
                    "side, and the only way to add WRITING for a new setting, app or device. (It's how the Seaboard "
                    "GRAND became configurable.) GRAB can't record this itself, so run a free USB/MIDI monitor next "
                    "to Dashboard:\n");
        add (Body,  "       Windows:  Wireshark + USBPcap \xe2\x80\x94 record the device's USB while you edit in Dashboard.\n");
        add (Body,  "       macOS:  MIDI Monitor (Snoize) with \"Spy on output\" on \xe2\x80\x94 record the \"to device\" "
                    "stream.\n");
        add (Click, "    Then attach the saved file to a GitHub issue \xe2\x80\x94 full step-by-step: \"Open capture guide\" "
                    "below.\n\n");

        // --- good to know -----------------------------------------------------------------------
        add (H1,    "Good to know\n");
        add (Body,  "    Sharing the device: on Windows 11 25H2+ MIDI is multi-client, so GRAB, Dashboard and your "
                    "DAW share the device. On older Windows, quit other MIDI apps before opening GRAB; Mac/Linux "
                    "already share. (For a Dashboard capture you want Dashboard + the monitor open \xe2\x80\x94 not GRAB.)\n");
        add (Click, "    Won't connect?  Diagnose...  ");
        add (Body,  "(top row) lists the MIDI ports GRAB sees and flags a port held by another app.\n");
        add (Body,  "    Archive firmware: Advanced \xe2\x96\xb8 Download firmware for archiving saves your Block / RISE "
                    "firmware while ROLI's servers are up.\n");
        add (Body,  "    Private & safe: serials are hashed, serial & MAC stripped from captures, nothing sent until "
                    "you click, and GRAB never flashes firmware.\n\n");

        add (Note,  "Thanks for helping!  Reopen any time from Advanced \xe2\x96\xb8 How you can help.");

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
