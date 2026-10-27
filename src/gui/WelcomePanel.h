// WelcomePanel — a first-launch "how you can help" dialog. GRAB is community-driven, so it
// explains the outstanding gaps and exactly what a user can do on their setup + in GRAB to help.
// Shown once (a flag in settings.properties); reopenable from Advanced ▸ How you can help.

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

        setSize (660, 640);
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
        // Lines are word-wrapped by the editor, so each item is one line (no mid-sentence breaks).
        const char* utf8 =
            "GRAB reads and writes ROLI MPE device settings directly over MIDI \xe2\x80\x94 no ROLI software needed. "
            "It's community-driven: reports from real hardware are how it grows, and how we confirm it's safe on "
            "devices we don't own. Here's how to help.\n"
            "\n"
            "IMPORTANT \xe2\x80\x94 ONE APP AT A TIME (on the legacy MIDI stack)\n"
            "On the classic Windows MIDI stack, ports are exclusive: ROLI Dashboard / Connect (and DAWs) hold the "
            "port, so GRAB can't read your device while they're open. Always QUIT ROLI Dashboard and close any DAW "
            "before opening GRAB (and vice versa).\n"
            "Exception: if you run the new Windows MIDI Services stack (built into recent Windows 11, installable on "
            "older builds), ports are shared \xe2\x80\x94 GRAB can run alongside your DAW, no need to quit it (confirmed on "
            "hardware). On macOS and Linux, sharing already works too.\n"
            "\n"
            "WHAT WORKS TODAY  (what we believe, and how sure we are)\n"
            "\n"
            "Seaboard Block / Block M  \xe2\x80\x94  confirmed on hardware\n"
            "     Read, live editing, ROLI presets, and factory reset. USB + Bluetooth. Chained blocks show as one device.\n"
            "\n"
            "Seaboard RISE 2  \xe2\x80\x94  supported\n"
            "     Read, live editing, ROLI presets. (No factory reset yet \xe2\x80\x94 that command is still unknown for RISE.)\n"
            "\n"
            "Seaboard RISE 25 / 49 (original)  \xe2\x80\x94  editable, wants more field reports\n"
            "     Read and live editing (confirmed from a RISE 25 capture), plus ROLI presets (new \xe2\x80\x94 please test). Min/max are hidden on fw 1.x. No factory reset yet.\n"
            "\n"
            "Piano M / LUMI Keys  \xe2\x80\x94  should work, untested\n"
            "     Same BLOCKS protocol, so read / edit / factory reset should apply \xe2\x80\x94 but we haven't seen one. Reports very welcome.\n"
            "\n"
            "Lightpad / Note Grid, and Live / Loop / Touch  \xe2\x80\x94  detection only\n"
            "     GRAB sees them; full settings support needs your profiles / captures.\n"
            "\n"
            "Seaboard GRAND  \xe2\x80\x94  not supported yet, but close\n"
            "     It speaks the RISE protocol (confirmed from a capture); we just need its settings to add it. GRAND owners \xe2\x80\x94 please help, see E) below.\n"
            "\n"
            "HOW TO TEST  (pick whatever fits your device)\n"
            "\n"
            "A)  Does GRAB read your device correctly?\n"
            "     1. Quit ROLI Dashboard / your DAW.\n"
            "     2. Open GRAB \xe2\x80\x94 it should list your device and show its settings.\n"
            "     3. If it looks right, click \"Contribute...\" to send an anonymised profile. If anything looks wrong, see HOW TO REPORT.\n"
            "\n"
            "B)  Do GRAB and ROLI Dashboard agree?  (really useful)\n"
            "     1. In ROLI Dashboard, choose a preset or change some settings (save if it asks).\n"
            "     2. QUIT ROLI Dashboard.\n"
            "     3. Open GRAB and check the same settings match what Dashboard showed.\n"
            "     4. Click \"Contribute...\", and tell us about any mismatch.\n"
            "\n"
            "C)  Does editing work and stick?\n"
            "     1. In GRAB, change a control (a sensitivity, octave, a mode...). Edits write to the device straight away \xe2\x80\x94 play it and you should hear the change.\n"
            "     2. Click \"Reload from device\" \xe2\x80\x94 the value should read back the same.\n"
            "     3. Optional: re-open ROLI Dashboard and confirm it agrees.\n"
            "\n"
            "D)  Presets & factory reset  (Seaboard / Blocks)\n"
            "     1. Use \"Apply ROLI preset...\" to load MPE / Multi / Single / Piano, and check it matches Dashboard's same preset.\n"
            "     2. Advanced \xe2\x96\xb8 Factory reset (presets only) \xe2\x80\x94 confirm the device reboots and returns to defaults, then \"Contribute...\" to capture its factory values.\n"
            "\n"
            "E)  Seaboard RISE / GRAND owners\n"
            "     - Original RISE (25/49): writing is new here \xe2\x80\x94 change a setting, reconnect, and confirm it stuck. Please report success or problems.\n"
            "     - RISE 2: supported \xe2\x80\x94 just Contribute a profile.\n"
            "     - Seaboard GRAND: good news \xe2\x80\x94 it uses the RISE protocol, so we're close. The missing piece is its SETTINGS. Please help by either: (a) clicking \"Help add device...\" and submitting, and/or (b) capturing ROLI Dashboard while you edit your GRAND's config (see tools/CAPTURE-DASHBOARD.md) and sending it. That data is what lets us add full GRAND support.\n"
            "\n"
            "HOW TO REPORT\n"
            "     - \"Contribute...\"  \xe2\x80\x94 sends an anonymised settings profile (needs a connected device).\n"
            "     - \"Help add device...\"  \xe2\x80\x94 for a device GRAB doesn't detect: a safe, read-only identity probe + report.\n"
            "     - Advanced \xe2\x96\xb8 Raw MIDI capture  \xe2\x80\x94 logs exactly what your device sends (read-only), to submit or attach to a GitHub issue.\n"
            "\n"
            "WHAT WE'RE WORKING ON\n"
            "     - Running alongside a DAW on Windows (multi-client MIDI, so you don't have to quit the DAW)\n"
            "     - Full RISE / GRAND configuration support\n"
            "     - Confirming presets + factory reset across the whole Block family\n"
            "     - Friendly names for every setting (built from your contributed profiles)\n"
            "\n"
            "PRIVACY\n"
            "     Nothing leaves your machine unless you click Contribute / Report / Submit. Serials are SHA-256 hashed; serial & MAC are stripped from any raw capture.\n"
            "\n"
            "Thanks for helping!  Reopen this any time from Advanced \xe2\x96\xb8 How you can help.";
        return juce::String::fromUTF8 (utf8);
    }

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePanel)
};
