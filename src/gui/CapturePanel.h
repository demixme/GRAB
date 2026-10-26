// CapturePanel — "Raw MIDI capture" dialog. Opens any chosen MIDI input and logs
// every message it emits (hex + ROLI-sysex annotation), so an owner of an
// unsupported device (Seaboard RISE/GRAND especially) can record what it sends and
// attach it to a GitHub issue. Strictly READ-ONLY: it listens, never transmits.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <sea_sysex.h>

#include "Anonymize.h"

#include <cstdint>
#include <thread>

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

class CapturePanel  : public juce::Component,
                      private juce::MidiInputCallback,
                      private juce::Timer
{
public:
    CapturePanel()
    {
        title.setText ("Raw MIDI capture (read-only). Pick a port, press Start, then play or "
                       "operate your device. GRAB only listens — it sends nothing. Save the "
                       "capture and attach it to a GitHub issue to help add device support.",
                       juce::dontSendNotification);
        title.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (title);

        addAndMakeVisible (portLabel);
        portLabel.setText ("Input port:", juce::dontSendNotification);
        addAndMakeVisible (portBox);
        addAndMakeVisible (refreshBtn);
        refreshBtn.onClick = [this] { refreshPorts(); };
        refreshPorts();

        logView.setMultiLine (true);
        logView.setReadOnly (true);
        logView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0)));
        logView.setCaretVisible (false);
        addAndMakeVisible (logView);

        addAndMakeVisible (notesLabel);
        notesLabel.setText ("Notes (device model, what you did):", juce::dontSendNotification);
        addAndMakeVisible (notesField);

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (startBtn);
        addAndMakeVisible (stopBtn);
        addAndMakeVisible (submitBtn);
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (issueBtn);
        addAndMakeVisible (closeBtn);
        submitBtn.onClick = [this] { submit(); };
        stopBtn.setEnabled (false);
        startBtn.onClick = [this] { start(); };
        stopBtn.onClick  = [this] { stop(); };
        saveBtn.onClick  = [this] { saveToFile(); };
        issueBtn.onClick = [this] { openIssue(); };
        closeBtn.onClick = [this]
        {
            stop();
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (640, 560);
        setStatus ("Idle. Pick a port and press Start.");
    }

    ~CapturePanel() override { stop(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (54));
        r.removeFromTop (4);

        auto top = r.removeFromTop (26);
        portLabel.setBounds (top.removeFromLeft (70));
        refreshBtn.setBounds (top.removeFromRight (80).reduced (2, 0));
        startBtn.setBounds (top.removeFromRight (70).reduced (2, 0));
        stopBtn.setBounds (top.removeFromRight (70).reduced (2, 0));
        portBox.setBounds (top.reduced (2, 0));
        r.removeFromTop (6);

        auto buttons = r.removeFromBottom (30);
        closeBtn .setBounds (buttons.removeFromRight (74).reduced (2, 0));
        issueBtn .setBounds (buttons.removeFromRight (94).reduced (2, 0));
        saveBtn  .setBounds (buttons.removeFromRight (94).reduced (2, 0));
        submitBtn.setBounds (buttons.removeFromRight (80).reduced (2, 0));
        statusLabel.setBounds (buttons);
        r.removeFromBottom (6);

        auto notesRow = r.removeFromBottom (24);
        notesLabel.setBounds (notesRow.removeFromLeft (210));
        notesField.setBounds (notesRow);
        r.removeFromBottom (6);

        logView.setBounds (r);
    }

private:
    void refreshPorts()
    {
        inputs = juce::MidiInput::getAvailableDevices();
        portBox.clear (juce::dontSendNotification);
        for (int i = 0; i < inputs.size(); ++i)
            portBox.addItem (inputs[i].name, i + 1);
        if (portBox.getSelectedId() == 0 && ! inputs.isEmpty())
            portBox.setSelectedId (1, juce::dontSendNotification);
    }

    void start()
    {
        const int idx = portBox.getSelectedId() - 1;
        if (idx < 0 || idx >= inputs.size()) { setStatus ("Pick an input port first."); return; }

        input = juce::MidiInput::openDevice (inputs[idx].identifier, this);
        if (input == nullptr)
        {
            setStatus ("Could not open '" + inputs[idx].name + "' — it may be held by another app "
                       "(or GRAB's own connection — disconnect first).");
            return;
        }

        { const juce::ScopedLock sl (lock); pending.clear(); }
        allLines.clear();
        logView.clear();
        messageCount = 0;
        capturedPortName = inputs[idx].name;
        startMs = juce::Time::getMillisecondCounterHiRes();
        input->start();
        startTimer (100);

        startBtn.setEnabled (false);
        stopBtn.setEnabled (true);
        portBox.setEnabled (false);
        refreshBtn.setEnabled (false);
        setStatus ("Listening on '" + capturedPortName + "' — play or operate your device.");
    }

    void stop()
    {
        if (input != nullptr) { input->stop(); input.reset(); }
        stopTimer();
        drain();
        startBtn.setEnabled (true);
        stopBtn.setEnabled (false);
        portBox.setEnabled (true);
        refreshBtn.setEnabled (true);
        if (capturedPortName.isNotEmpty())
            setStatus ("Stopped. " + juce::String (messageCount) + " message(s) from '" + capturedPortName
                       + "'. Save to file, then attach it to an issue.");
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        // Background (MIDI) thread: format and queue only; the UI is touched on the timer.
        const auto* d = reinterpret_cast<const std::uint8_t*> (m.getRawData());
        const auto  n = (std::size_t) m.getRawDataSize();
        const auto  safe = grab::anonymize (d, n);   // no serial / MAC in what gets shared
        const double t = juce::Time::getMillisecondCounterHiRes() - startMs;

        juce::String line;
        line << "[" << juce::String (t, 1).paddedLeft (' ', 10) << "ms] " << juce::String (sea::toHex (safe.data(), safe.size()));
        const juce::String note (sea::annotate (d, n));
        if (note.isNotEmpty())
            line << "   (" << note << ")";

        const juce::ScopedLock sl (lock);
        pending.add (line);
    }

    void timerCallback() override { drain(); }

    void drain()
    {
        juce::StringArray got;
        { const juce::ScopedLock sl (lock); got.swapWith (pending); }
        if (got.isEmpty())
            return;

        for (auto& l : got)
            allLines.add (l);
        messageCount += got.size();

        logView.moveCaretToEnd();
        logView.insertTextAtCaret (got.joinIntoString ("\n") + "\n");

        if (allLines.size() > 50000)   // safety: auto-stop a runaway capture
        {
            stop();
            setStatus ("Stopped at 50000 messages (capture limit). Save to file.");
        }
    }

    void saveToFile()
    {
        if (allLines.isEmpty()) { setStatus ("Nothing captured yet."); return; }

        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("captures");
        dir.createDirectory();
        auto f = dir.getChildFile ("capture-" + sanitize (capturedPortName) + "-"
                     + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");

        juce::String out;
        out << "GRAB raw MIDI capture\n"
            << "Port: " << capturedPortName << "\n"
            << "OS:   " << juce::SystemStats::getOperatingSystemName() << "\n"
            << "When: " << juce::Time::getCurrentTime().toISO8601 (true) << "\n"
            << "Messages: " << allLines.size() << "\n\n"
            << allLines.joinIntoString ("\n") << "\n";

        if (f.replaceWithText (out))
            setStatus ("Saved: " + f.getFullPathName());
        else
            setStatus ("Could not write file.");
    }

    void openIssue()
    {
        juce::String body;
        body << "**Device model:** \n"
             << "**OS:** " << juce::SystemStats::getOperatingSystemName() << "\n"
             << "**Port:** " << capturedPortName << "\n\n"
             << "Raw MIDI capture (GRAB -> Capture -> Save to file) — **please attach the saved "
                ".txt file**. First lines:\n```\n"
             << allLines.joinIntoString ("\n").substring (0, 1500) << "\n```\n";

        juce::URL url ("https://github.com/demixme/GRAB/issues/new");
        url = url.withParameter ("labels", "device")
                 .withParameter ("title", "[capture] ")
                 .withParameter ("body", body);
        url.launchInDefaultBrowser();
        setStatus ("Opened a prefilled issue — attach the saved capture file to it.");
    }

    void submit()
    {
        if (allLines.isEmpty()) { setStatus ("Nothing captured yet."); return; }
        if (input != nullptr) stop(); // finalize the capture first

        const auto endpoint = capturesEndpoint();
        if (! juce::URL (endpoint).isWellFormed()) { setStatus ("Bad endpoint; use Save to file instead."); return; }

        auto* root = new juce::DynamicObject();
        root->setProperty ("schemaVersion", 1);
        root->setProperty ("kind", "midi-capture");
        root->setProperty ("submittedAt", juce::Time::getCurrentTime().toISO8601 (true));
        auto* tool = new juce::DynamicObject();
        tool->setProperty ("name", "GRAB");
        tool->setProperty ("version", GRAB_VERSION);
        root->setProperty ("tool", juce::var (tool));
        auto* plat = new juce::DynamicObject();
        plat->setProperty ("os", juce::SystemStats::getOperatingSystemName());
        root->setProperty ("platform", juce::var (plat));
        auto* cap = new juce::DynamicObject();
        cap->setProperty ("port", capturedPortName);
        cap->setProperty ("messageCount", messageCount);
        juce::Array<juce::var> lines;
        for (auto& l : allLines) lines.add (l);
        cap->setProperty ("lines", lines);
        root->setProperty ("capture", juce::var (cap));
        root->setProperty ("notes", notesField.getText());

        const auto body = juce::JSON::toString (juce::var (root), true); // compact
        setStatus ("Submitting " + juce::String (messageCount) + " messages...");
        submitBtn.setEnabled (false);

        juce::Component::SafePointer<CapturePanel> safe (this);
        std::thread ([safe, endpoint, body]
        {
            int code = 0;
            juce::String err;
            {
                juce::URL url (endpoint);
                url = url.withPOSTData (body);
                juce::WebInputStream web (url, true);
                web.withExtraHeaders ("Content-Type: application/json");
                web.withConnectionTimeout (20000);
                if (web.connect (nullptr))
                    code = web.getStatusCode();
                else
                    err = "connection failed";
            }
            juce::MessageManager::callAsync ([safe, code, err]
            {
                if (auto* p = safe.getComponent())
                    p->onSubmitResult (code, err);
            });
        }).detach();
    }

    void onSubmitResult (int code, const juce::String& err)
    {
        submitBtn.setEnabled (true);
        if (err.isNotEmpty())
            setStatus ("Submit failed (" + err + "). Try Save to file or Open issue.");
        else if (code >= 200 && code < 300)
            setStatus ("Thank you! Capture submitted (HTTP " + juce::String (code) + ").");
        else
            setStatus ("Server returned HTTP " + juce::String (code) + ". Try Save or Open issue.");
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

    static juce::String sanitize (const juce::String& s)
    {
        return s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    juce::Label       title, portLabel, notesLabel, statusLabel;
    juce::ComboBox    portBox;
    juce::Array<juce::MidiDeviceInfo> inputs;
    juce::TextButton  refreshBtn { "Refresh" }, startBtn { "Start" }, stopBtn { "Stop" },
                      submitBtn { "Submit" }, saveBtn { "Save to file" },
                      issueBtn { "Open issue" }, closeBtn { "Close" };
    juce::TextEditor  logView, notesField;

    std::unique_ptr<juce::MidiInput> input;
    juce::CriticalSection lock;
    juce::StringArray pending, allLines;   // pending: bg->ui handoff; allLines: full capture
    double startMs = 0.0;
    int    messageCount = 0;
    juce::String capturedPortName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapturePanel)
};
