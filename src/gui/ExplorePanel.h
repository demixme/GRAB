// ExplorePanel — one-click "Help add this device" for non-technical contributors.
// Auto-picks the ROLI device's ports, sends a single READ-ONLY MIDI Identity Request,
// records the reply plus a few seconds of whatever the device emits, and submits it all
// (ports seen + sent + received) in one go. It NEVER changes a setting — only the manual
// Probe… sends writes. See DEVICE-PROFILES.md / RISE-GRAND.md.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <sea_sysex.h>

#include "Anonymize.h"

#include <cstdint>
#include <thread>

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

class ExplorePanel  : public juce::Component,
                      private juce::MidiInputCallback,
                      private juce::Timer
{
public:
    ExplorePanel()
    {
        outputs = juce::MidiOutput::getAvailableDevices();
        inputs  = juce::MidiInput::getAvailableDevices();
        outIdx  = pickRoli (outputs);
        inIdx   = pickRoli (inputs);

        const auto dev = (inIdx >= 0) ? inputs[inIdx].name
                       : (outIdx >= 0) ? outputs[outIdx].name : juce::String ("(no ROLI device found)");

        title.setText ("Help add this device.\n\nGRAB will send one harmless, read-only "
                       "\"who are you?\" request to " + dev + " and record what it sends back for "
                       "a few seconds, then submit it (anonymized) to help add support. It does NOT "
                       "change any settings. One click.", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (title);

        addAndMakeVisible (runBtn);
        runBtn.setButtonText ("Run & submit");
        runBtn.onClick = [this] { run(); };
        runBtn.setEnabled (inIdx >= 0 || outIdx >= 0);

        logView.setMultiLine (true);
        logView.setReadOnly (true);
        logView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0)));
        logView.setCaretVisible (false);
        addAndMakeVisible (logView);

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (notesLabel); notesLabel.setText ("Notes (device model):", juce::dontSendNotification);
        addAndMakeVisible (notesField);

        addAndMakeVisible (saveBtn);  saveBtn.setButtonText ("Save to file");
        addAndMakeVisible (issueBtn); issueBtn.setButtonText ("Open issue");
        addAndMakeVisible (closeBtn); closeBtn.setButtonText ("Close");
        saveBtn.setEnabled (false); issueBtn.setEnabled (false);
        saveBtn.onClick  = [this] { saveToFile(); };
        issueBtn.onClick = [this] { openIssue(); };
        closeBtn.onClick = [this]
        {
            stopListening();
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (560, 480);
        setStatus (inIdx >= 0 || outIdx >= 0 ? "Ready. Click Run & submit."
                                             : "No ROLI device found — connect one and reopen.");
    }

    ~ExplorePanel() override { stopListening(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (110));
        r.removeFromTop (4);
        runBtn.setBounds (r.removeFromTop (30).removeFromLeft (160));
        r.removeFromTop (6);

        auto buttons = r.removeFromBottom (30);
        closeBtn .setBounds (buttons.removeFromRight (74).reduced (2, 0));
        issueBtn .setBounds (buttons.removeFromRight (94).reduced (2, 0));
        saveBtn  .setBounds (buttons.removeFromRight (100).reduced (2, 0));
        statusLabel.setBounds (buttons);
        r.removeFromBottom (6);

        auto notes = r.removeFromBottom (24);
        notesLabel.setBounds (notes.removeFromLeft (140));
        notesField.setBounds (notes);
        r.removeFromBottom (6);

        logView.setBounds (r);
    }

private:
    static int pickRoli (const juce::Array<juce::MidiDeviceInfo>& list)
    {
        for (int i = 0; i < list.size(); ++i)
            if (list[i].name.containsIgnoreCase ("seaboard") || list[i].name.containsIgnoreCase ("rise")
                || list[i].name.containsIgnoreCase ("grand") || list[i].name.containsIgnoreCase ("roli"))
                return i;
        return -1;
    }

    void run()
    {
        runBtn.setEnabled (false);
        allLines.clear(); logView.clear();
        { const juce::ScopedLock sl (lock); pending.clear(); }

        logLine ("# GRAB auto-explore");
        logLine ("# OS: " + juce::SystemStats::getOperatingSystemName());
        logLine ("# Output: " + (outIdx >= 0 ? outputs[outIdx].name : juce::String ("(none)")));
        logLine ("# Input:  " + (inIdx  >= 0 ? inputs[inIdx].name   : juce::String ("(none)")));
        for (auto& d : inputs)  logLine ("# port IN   " + d.name + "  id=[" + grab::anonymizeIdentifier (d.identifier) + "]");
        for (auto& d : outputs) logLine ("# port OUT  " + d.name + "  id=[" + grab::anonymizeIdentifier (d.identifier) + "]");

        if (inIdx >= 0)
        {
            input = juce::MidiInput::openDevice (inputs[inIdx].identifier, this);
            if (input != nullptr) input->start();
        }

        if (outIdx >= 0)
        {
            if (auto out = juce::MidiOutput::openDevice (outputs[outIdx].identifier))
            {
                // Universal Identity Request (read-only) — works on most MIDI gear.
                const std::uint8_t idReq[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
                out->sendMessageNow (juce::MidiMessage (idReq, (int) sizeof (idReq)));
                logLine ("SENT   " + juce::String (sea::toHex (idReq, sizeof (idReq))) + "   (Universal Identity Request)");

                // ROLI identify (read-only) — RISE/GRAND ignore the universal one but answer this
                // with a full settings dump (confirmed safe: the 0x78 handler only reads here).
                const std::uint8_t roliId[] = { 0xF0, 0x00, 0x21, 0x10, 0x78, 0x3F, 0xF7 };
                out->sendMessageNow (juce::MidiMessage (roliId, (int) sizeof (roliId)));
                logLine ("SENT   " + juce::String (sea::toHex (roliId, sizeof (roliId))) + "   (ROLI identify)");
            }
            else logLine ("# could not open output to send the Identity Request");
        }

        elapsedTicks = 0;
        startTimer (250); // collect for ~5s (20 ticks), then auto-submit
        setStatus ("Listening for ~5 seconds... (you can wiggle/press the device)");
    }

    void timerCallback() override
    {
        drain();
        if (++elapsedTicks >= 20)
        {
            stopListening();
            setStatus ("Done — submitting...");
            submit();
        }
    }

    void stopListening()
    {
        if (input != nullptr) { input->stop(); input.reset(); }
        stopTimer();
        drain();
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        const auto* d = reinterpret_cast<const std::uint8_t*> (m.getRawData());
        const auto  n = (std::size_t) m.getRawDataSize();
        const auto  safe = grab::anonymize (d, n);   // no serial / MAC in what gets shared
        juce::String line = "RECV   " + juce::String (sea::toHex (safe.data(), safe.size()));
        const juce::String note (sea::annotate (d, n));
        if (note.isNotEmpty()) line << "   (" << note << ")";
        const juce::ScopedLock sl (lock);
        pending.add (line);
    }

    void logLine (const juce::String& s) { const juce::ScopedLock sl (lock); pending.add (s); }

    void drain()
    {
        juce::StringArray got;
        { const juce::ScopedLock sl (lock); got.swapWith (pending); }
        if (got.isEmpty()) return;
        for (auto& l : got) allLines.add (l);
        logView.moveCaretToEnd();
        logView.insertTextAtCaret (got.joinIntoString ("\n") + "\n");
    }

    juce::var buildPayload()
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("schemaVersion", 1);
        root->setProperty ("kind", "midi-capture");
        root->setProperty ("submittedAt", juce::Time::getCurrentTime().toISO8601 (true));
        auto* tool = new juce::DynamicObject();
        tool->setProperty ("name", "GRAB"); tool->setProperty ("version", GRAB_VERSION);
        root->setProperty ("tool", juce::var (tool));
        auto* plat = new juce::DynamicObject();
        plat->setProperty ("os", juce::SystemStats::getOperatingSystemName());
        root->setProperty ("platform", juce::var (plat));
        auto* cap = new juce::DynamicObject();
        cap->setProperty ("port", (inIdx >= 0 ? inputs[inIdx].name : juce::String ("?")) + " (auto-explore)");
        cap->setProperty ("messageCount", allLines.size());
        juce::Array<juce::var> lines; for (auto& l : allLines) lines.add (l);
        cap->setProperty ("lines", lines);
        root->setProperty ("capture", juce::var (cap));
        root->setProperty ("notes", "auto-explore" + (notesField.getText().isNotEmpty() ? " — " + notesField.getText() : juce::String()));
        return juce::var (root);
    }

    void submit()
    {
        const auto body = juce::JSON::toString (buildPayload(), true);
        const auto endpoint = capturesEndpoint();
        juce::Component::SafePointer<ExplorePanel> safe (this);
        std::thread ([safe, endpoint, body]
        {
            int code = 0; juce::String err;
            { juce::URL url (endpoint); url = url.withPOSTData (body);
              juce::WebInputStream web (url, true); web.withExtraHeaders ("Content-Type: application/json");
              web.withConnectionTimeout (20000);
              if (web.connect (nullptr)) code = web.getStatusCode(); else err = "connection failed"; }
            juce::MessageManager::callAsync ([safe, code, err]
            {
                if (auto* p = safe.getComponent())
                {
                    p->saveBtn.setEnabled (true); p->issueBtn.setEnabled (true); p->runBtn.setEnabled (true);
                    p->setStatus (err.isNotEmpty() ? "Submit failed (" + err + "). Use Save to file or Open issue."
                                  : (code >= 200 && code < 300) ? "Thank you! Submitted — this really helps."
                                  : "Server returned HTTP " + juce::String (code) + ". Use Save to file or Open issue.");
                }
            });
        }).detach();
    }

    void saveToFile()
    {
        if (allLines.isEmpty()) { setStatus ("Run it first."); return; }
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("captures");
        dir.createDirectory();
        auto f = dir.getChildFile ("explore-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");
        if (f.replaceWithText (allLines.joinIntoString ("\n") + "\n")) setStatus ("Saved: " + f.getFullPathName());
        else setStatus ("Could not write file.");
    }

    void openIssue()
    {
        juce::String body;
        body << "**Device model:** " << notesField.getText() << "\n**OS:** "
             << juce::SystemStats::getOperatingSystemName() << "\n\nAuto-explore result:\n```\n"
             << allLines.joinIntoString ("\n").substring (0, 4000) << "\n```\n";
        juce::URL url ("https://github.com/demixme/GRAB/issues/new");
        url = url.withParameter ("labels", "device").withParameter ("title", "[device] ").withParameter ("body", body);
        url.launchInDefaultBrowser();
        setStatus ("Opened a prefilled issue in your browser.");
    }

    static juce::File settingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GRAB").getChildFile ("settings.properties");
    }
    static juce::String capturesEndpoint()
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        const auto base = props.getValue ("profileEndpoint", "https://www.demix.me/grab/v1/profiles");
        return base.replace ("profiles", "captures");
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    juce::Array<juce::MidiDeviceInfo> outputs, inputs;
    int outIdx = -1, inIdx = -1;

    juce::Label title, statusLabel, notesLabel;
    juce::TextEditor logView, notesField;
    juce::TextButton runBtn, saveBtn, issueBtn, closeBtn;

    std::unique_ptr<juce::MidiInput> input;
    juce::CriticalSection lock;
    juce::StringArray pending, allLines;
    int elapsedTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExplorePanel)
};
