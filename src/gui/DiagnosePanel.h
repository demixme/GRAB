// DiagnosePanel — a "Diagnose connection" dialog. Most GRAB connection problems are NOT GRAB
// bugs: the device isn't connected/paired, or its MIDI port is held by another app (ROLI
// Dashboard/Connect, a DAW, or ROLI's background "Hardware Driver" service on Windows). This
// enumerates the MIDI ports GRAB can see, test-opens the ROLI ones to spot a held port, and
// gives plain, platform-specific steps. Read-only; it never writes to a device.

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "Theme.h"

class DiagnosePanel  : public juce::Component,
                       private juce::MidiInputCallback
{
public:
    DiagnosePanel()
    {
        addAndMakeVisible (title);
        title.setText ("Diagnose connection", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (body);
        body.setMultiLine (true);
        body.setReadOnly (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, 0)));
        body.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        body.setColour (juce::TextEditor::textColourId,       grab::theme::textPrimary);
        body.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);

        for (auto* b : { &rerunBtn, &copyBtn, &issueBtn, &closeBtn })
            addAndMakeVisible (b);
        rerunBtn.onClick = [this] { run(); };
        copyBtn.onClick  = [this] { juce::SystemClipboard::copyTextToClipboard (report); };
        issueBtn.onClick = [this]
        {
            const auto url = "https://github.com/demixme/GRAB/issues/new?title="
                           + juce::URL::addEscapeChars ("Connection issue", false)
                           + "&body=" + juce::URL::addEscapeChars ("What I tried:\n\n\n---\n" + report, false);
            juce::URL (url).launchInDefaultBrowser();
        };
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (680, 620);
        run();
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
        auto buttons = r.removeFromBottom (32);
        closeBtn.setBounds (buttons.removeFromRight (90).reduced (2, 0));
        issueBtn.setBounds (buttons.removeFromRight (130).reduced (2, 0));
        copyBtn.setBounds  (buttons.removeFromRight (110).reduced (2, 0));
        rerunBtn.setBounds (buttons.removeFromLeft (110).reduced (2, 0));
        r.removeFromBottom (8);
        body.setBounds (r);
    }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override {}

    // ---- classification (self-contained, mirrors LoggingMidi) ----
    static bool isRoli (const juce::MidiDeviceInfo& d)
    {
        return d.identifier.containsIgnoreCase ("vid_2af4")
            || d.name.containsIgnoreCase ("roli")     || d.name.containsIgnoreCase ("seaboard")
            || d.name.containsIgnoreCase ("rise")     || d.name.containsIgnoreCase ("grand")
            || d.name.containsIgnoreCase ("piano")    || d.name.containsIgnoreCase ("lumi")
            || d.name.containsIgnoreCase ("lightpad") || d.name.containsIgnoreCase ("airwave")
            || d.name.containsIgnoreCase ("block")
            || d.identifier.containsIgnoreCase ("seaboardblock") || d.identifier.containsIgnoreCase ("pianofortynine");
    }
    static bool isBle  (const juce::MidiDeviceInfo& d)
    { return d.identifier.containsIgnoreCase ("blemidi") || d.identifier.containsIgnoreCase ("bthle")
          || d.identifier.containsIgnoreCase ("bluetooth"); }
    static bool isRise (const juce::MidiDeviceInfo& d)
    { return d.name.containsIgnoreCase ("rise") || d.name.containsIgnoreCase ("grand"); }

    void run()
    {
        const auto ins  = juce::MidiInput::getAvailableDevices();
        const auto outs = juce::MidiOutput::getAvailableDevices();

        const auto os = juce::SystemStats::getOperatingSystemName();
        const bool mac = os.containsIgnoreCase ("mac") || os.containsIgnoreCase ("os x") || os.containsIgnoreCase ("osx");
        const bool win = os.containsIgnoreCase ("win");

        juce::StringArray portLines;
        int roliIn = 0, roliOut = 0, roliUsbIn = 0, roliBleIn = 0, busyUsbIn = 0, riseCount = 0;
        for (auto& d : ins)
        {
            if (! isRoli (d)) continue;
            ++roliIn;
            const bool ble = isBle (d);
            ble ? ++roliBleIn : ++roliUsbIn;
            if (isRise (d)) ++riseCount;
            // Test-open: a port that lists but won't open is held by another app. (On macOS
            // CoreMIDI is multi-client so this rarely fails even when another app has it.)
            bool openable = true;
            if (auto in = juce::MidiInput::openDevice (d.identifier, this)) { in.reset(); }
            else { openable = false; if (! ble) ++busyUsbIn; }
            portLines.add ("  IN   [" + juce::String (ble ? "BLE" : "USB") + "]  " + d.name
                           + (openable ? "" : "   <-- BUSY (can't open)"));
        }
        for (auto& d : outs)
        {
            if (! isRoli (d)) continue;
            ++roliOut;
            portLines.add ("  OUT  [" + juce::String (isBle (d) ? "BLE" : "USB") + "]  " + d.name);
        }

        juce::StringArray t;
        t.add ("GRAB talks to your ROLI device over MIDI. Most connection problems are the device");
        t.add ("not being connected, or its MIDI port being held by another app - not GRAB itself.");
        t.add ("This is read-only; it never changes or writes to your device.");
        t.add ("");
        t.add ("OS: " + os);
        t.add ("");
        t.add ("WHAT GRAB CAN SEE RIGHT NOW");
        if (roliIn == 0 && roliOut == 0)
            t.add ("  (no ROLI MIDI ports found)");
        else
            t.addArray (portLines);
        t.add ("");

        t.add ("WHICH CONNECTION ('Connection Type')");
        t.add ("  On 'Auto', GRAB uses USB if your device is on USB, otherwise Bluetooth (BLE).");
        t.add ("  You can force one with the Connection Type menu, then it re-scans.");
        t.add ("");

        t.add ("LIKELY ISSUE & WHAT TO DO");

        if (busyUsbIn > 0)
        {
            t.add ("  * Your device is on USB but its port is HELD BY ANOTHER APP (it lists but");
            t.add ("    won't open). This is the most common cause.");
            t.add ("    1. Quit ROLI Dashboard / ROLI Connect, and close any DAW.");
            if (win)
                t.add ("    2. Windows: ROLI's background \"ROLI Hardware Driver\" service can also hold");
            if (win)
                t.add ("       the port - stop that service (or uninstall ROLI Connect). GRAB needs no driver.");
            t.add ("    3. Then click 'Reload from device' (or re-run this).");
            t.add ("    Note: only one app can use the port at a time on the classic MIDI stack.");
            if (win)
                t.add ("    (Exception: Windows MIDI Services lets apps share a device - then a DAW can stay open.)");
        }
        else if (roliIn == 0 && roliOut == 0)
        {
            t.add ("  * GRAB can't see your device on any MIDI port. It isn't connected, or nothing");
            t.add ("    has brought it onto MIDI yet.");
            t.add ("    USB: plug it in directly (no driver needed); try another cable/port.");
            if (mac)
            {
                t.add ("    Bluetooth (macOS): BLE-MIDI devices do NOT appear under System Settings >");
                t.add ("      Bluetooth. Connect once via Audio MIDI Setup > Window > Show MIDI Studio >");
                t.add ("      the Bluetooth icon > Connect. After that it's a normal MIDI port.");
            }
            if (win)
            {
                t.add ("    Bluetooth (Windows): pair the device in Settings > Bluetooth & devices, and");
                t.add ("      use the new Windows MIDI Services stack for reliable BLE-MIDI.");
            }
            t.add ("    A ROLI device bonds to ONE computer at a time - turn Bluetooth off on any other");
            t.add ("    machine (or ROLI software) holding it, then power-cycle the device.");
            t.add ("    If you JUST connected it: GRAB on 'Auto' re-checks when a device appears - give");
            t.add ("    it a second, or click 'Re-run' above.");
        }
        else if (riseCount > 0 && roliUsbIn == 0 && roliBleIn == 0)
        {
            t.add ("  * A Seaboard RISE / GRAND is present. GRAB uses a separate page for those (they");
            t.add ("    speak a different protocol) - this isn't a BLOCKS connection failure.");
        }
        else if (roliBleIn > 0 && roliUsbIn == 0)
        {
            t.add ("  * Your device is visible over Bluetooth. If it isn't connecting:");
            if (mac)
                t.add ("    macOS: re-connect it in Audio MIDI Setup > MIDI Studio > Bluetooth, then set");
            if (mac)
                t.add ("      Connection Type to BLE (or Auto) and click 'Reload from device'.");
            if (win)
                t.add ("    Windows: use Windows MIDI Services for reliable BLE; re-pair if it won't advertise.");
            t.add ("    BLE can be flaky - if it keeps failing, try USB instead.");
        }
        else
        {
            t.add ("  * GRAB can see and open your device - the connection looks healthy.");
            t.add ("    If settings still aren't loading, click 'Reload from device', or set the");
            t.add ("    Connection Type to USB (or BLE) to force a re-scan.");
        }

        t.add ("");
        t.add ("Still stuck? Click 'Open an issue' to send this to the developers, or use");
        t.add ("'Help add device...' on the main window.");

        report = t.joinIntoString ("\n");
        body.setText (report, false);
    }

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton rerunBtn { "Re-run" }, copyBtn { "Copy report" },
                     issueBtn { "Open an issue" }, closeBtn { "Close" };
    juce::String     report;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiagnosePanel)
};
