// ProbePanel — "Active probe" dialog (advanced). Sends ONE chosen message to a
// device's MIDI output and logs the reply on its input, to reverse-engineer the
// host->device control protocol of a device GRAB doesn't yet fully map.
//
// Safety: only curated, documented, non-destructive messages are offered; firmware /
// reset / packed-BLOCKS messages are hard-blocked; the exact bytes are shown and the
// user must tick "Arm" before each send. See RISE-GRAND.md.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <sea_sysex.h>

#include "Anonymize.h"

#include <cstdint>
#include <thread>
#include <vector>

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

class ProbePanel  : public juce::Component,
                    private juce::MidiInputCallback,
                    private juce::Timer
{
public:
    ProbePanel()
    {
        buildLibrary();

        title.setText (juce::String::fromUTF8 (
                       "Active probe (advanced, use with care). Sends ONE message to a device's "
                       "output and logs the reply on its input - for working out the control "
                       "protocol of a device GRAB doesn't yet fully map (a new or unknown one). "
                       "Review the exact bytes and tick Arm before each send. Firmware/reset/packed "
                       "messages are blocked."),
                       juce::dontSendNotification);
        title.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (title);

        addAndMakeVisible (outLabel); outLabel.setText ("Send to (output):", juce::dontSendNotification);
        addAndMakeVisible (inLabel);  inLabel.setText ("Listen on (input):", juce::dontSendNotification);
        addAndMakeVisible (outBox);
        addAndMakeVisible (inBox);
        inBox.onChange = [this] { restartListening(); };
        refreshPorts();

        addAndMakeVisible (probeLabel); probeLabel.setText ("Message:", juce::dontSendNotification);
        addAndMakeVisible (probeBox);
        for (int i = 0; i < (int) library.size(); ++i)
            probeBox.addItem (library[(size_t) i].name, i + 1);
        probeBox.setSelectedId (1, juce::dontSendNotification);
        probeBox.onChange = [this] { updateBytes(); };

        addAndMakeVisible (valueLabel); valueLabel.setText ("Value (0-127):", juce::dontSendNotification);
        addAndMakeVisible (valueField); valueField.setText ("0", juce::dontSendNotification);
        valueField.onTextChange = [this] { updateBytes(); };
        addAndMakeVisible (customField);
        customField.setTextToShowWhenEmpty ("custom hex, e.g. F0 00 21 10 78 3D 0E 01 F7", juce::Colours::grey);
        customField.onTextChange = [this] { updateBytes(); };

        bytesLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, 0)));
        addAndMakeVisible (bytesLabel);

        addAndMakeVisible (armButton); armButton.setButtonText ("Arm send");
        addAndMakeVisible (sendButton); sendButton.setButtonText ("Send");
        sendButton.onClick = [this] { send(); };

        logView.setMultiLine (true);
        logView.setReadOnly (true);
        logView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0)));
        logView.setCaretVisible (false);
        addAndMakeVisible (logView);

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (submitBtn); submitBtn.setButtonText ("Submit");
        addAndMakeVisible (saveBtn);   saveBtn.setButtonText ("Save to file");
        addAndMakeVisible (closeBtn);  closeBtn.setButtonText ("Close");
        submitBtn.onClick = [this] { submit(); };
        saveBtn.onClick   = [this] { saveToFile(); };
        closeBtn.onClick  = [this]
        {
            stopListening();
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        updateBytes();
        restartListening();
        setSize (660, 600);
    }

    ~ProbePanel() override { stopListening(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (58));
        r.removeFromTop (4);

        auto row1 = r.removeFromTop (24);
        outLabel.setBounds (row1.removeFromLeft (110));
        outBox.setBounds (row1.reduced (2, 0));
        r.removeFromTop (4);
        auto row2 = r.removeFromTop (24);
        inLabel.setBounds (row2.removeFromLeft (110));
        inBox.setBounds (row2.reduced (2, 0));
        r.removeFromTop (6);

        auto row3 = r.removeFromTop (24);
        probeLabel.setBounds (row3.removeFromLeft (70));
        probeBox.setBounds (row3.reduced (2, 0));
        r.removeFromTop (4);
        auto row4 = r.removeFromTop (24);
        valueLabel.setBounds (row4.removeFromLeft (90));
        valueField.setBounds (row4.removeFromLeft (60).reduced (2, 0));
        customField.setBounds (row4.reduced (2, 0));
        r.removeFromTop (6);

        auto row5 = r.removeFromTop (24);
        sendButton.setBounds (row5.removeFromRight (80).reduced (2, 0));
        armButton.setBounds (row5.removeFromRight (90).reduced (2, 0));
        bytesLabel.setBounds (row5);
        r.removeFromTop (6);

        auto buttons = r.removeFromBottom (30);
        closeBtn .setBounds (buttons.removeFromRight (74).reduced (2, 0));
        saveBtn  .setBounds (buttons.removeFromRight (100).reduced (2, 0));
        submitBtn.setBounds (buttons.removeFromRight (80).reduced (2, 0));
        statusLabel.setBounds (buttons);
        r.removeFromBottom (6);

        logView.setBounds (r);
    }

private:
    struct Probe
    {
        juce::String name;
        std::vector<std::uint8_t> bytes;
        int valueIndex = -1;     // index of an editable 0-127 value byte, or -1
        bool custom = false;
    };

    void buildLibrary()
    {
        // Reads only by default; RISE 2 config now has its own page + `seaprobe rise2`,
        // so the GUI probe stays to read-identify an unknown device. Writes go via Custom.
        library.push_back ({ "Universal Identity Request (read-only, any device)",
                             { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }, -1, false });
        library.push_back ({ "ROLI identify (read-only, RISE/GRAND answer this)",
                             { 0xF0, 0x00, 0x21, 0x10, 0x78, 0x3F, 0xF7 }, -1, false });
        library.push_back ({ "Custom hex...", {}, -1, true });
    }

    // Hard safety guard: never transmit firmware/reset/bootloader or raw packed-BLOCKS
    // host->device messages. The curated entries are all safe; this guards custom input.
    static bool looksDangerous (const std::vector<std::uint8_t>& b)
    {
        if (b.size() >= 5 && b[0] == 0xF0 && b[1] == 0x00 && b[2] == 0x21 && b[3] == 0x10)
        {
            const auto ns = b[4];
            if (ns == 0x47) return true; // firmware file
            if (ns == 0x49) return true; // resetMaster
            if (ns == 0x77) return true; // packed BLOCKS host->device (config/firmware live inside)
        }
        return false;
    }

    std::vector<std::uint8_t> currentBytes() const
    {
        const int i = probeBox.getSelectedId() - 1;
        if (i < 0 || i >= (int) library.size())
            return {};
        const auto& p = library[(size_t) i];
        if (p.custom)
            return parseHex (customField.getText());

        auto b = p.bytes;
        if (p.valueIndex >= 0 && p.valueIndex < (int) b.size())
            b[(size_t) p.valueIndex] = (std::uint8_t) juce::jlimit (0, 127, valueField.getText().getIntValue());
        return b;
    }

    void updateBytes()
    {
        const int i = probeBox.getSelectedId() - 1;
        const bool custom = (i >= 0 && i < (int) library.size() && library[(size_t) i].custom);
        const bool hasValue = (i >= 0 && i < (int) library.size() && library[(size_t) i].valueIndex >= 0);
        customField.setVisible (custom);
        valueField.setEnabled (hasValue);

        const auto b = currentBytes();
        juce::String s = b.empty() ? juce::String ("(nothing to send)")
                                   : juce::String (sea::toHex (b.data(), b.size()));
        if (! b.empty() && looksDangerous (b))
            s += "   [BLOCKED: firmware/reset/packed]";
        bytesLabel.setText (s, juce::dontSendNotification);
    }

    void refreshPorts()
    {
        outputs = juce::MidiOutput::getAvailableDevices();
        inputs  = juce::MidiInput::getAvailableDevices();
        outBox.clear (juce::dontSendNotification);
        inBox.clear (juce::dontSendNotification);
        for (int i = 0; i < outputs.size(); ++i) outBox.addItem (outputs[i].name, i + 1);
        for (int i = 0; i < inputs.size();  ++i) inBox.addItem (inputs[i].name, i + 1);
        // Prefer a Seaboard/ROLI-looking pair.
        selectPreferred (outBox, outputs);
        selectPreferred (inBox, inputs);
    }

    static void selectPreferred (juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& list)
    {
        for (int i = 0; i < list.size(); ++i)
            if (list[i].name.containsIgnoreCase ("seaboard") || list[i].name.containsIgnoreCase ("rise")
                || list[i].name.containsIgnoreCase ("grand") || list[i].name.containsIgnoreCase ("roli"))
            { box.setSelectedId (i + 1, juce::dontSendNotification); return; }
        if (box.getSelectedId() == 0 && list.size() > 0) box.setSelectedId (1, juce::dontSendNotification);
    }

    void restartListening()
    {
        stopListening();
        const int i = inBox.getSelectedId() - 1;
        if (i < 0 || i >= inputs.size()) return;
        input = juce::MidiInput::openDevice (inputs[i].identifier, this);
        if (input != nullptr)
        {
            input->start();
            startTimer (100);
            setStatus ("Listening on '" + inputs[i].name + "'. Pick a message, review bytes, Arm, Send.");
        }
        else
        {
            setStatus ("Could not open input '" + inputs[i].name + "' (held by another app or GRAB).");
        }
    }

    void stopListening()
    {
        if (input != nullptr) { input->stop(); input.reset(); }
        stopTimer();
        drain();
    }

    void send()
    {
        const auto bytes = currentBytes();
        if (bytes.empty()) { setStatus (juce::String::fromUTF8 ("Nothing to send - enter/select a message.")); return; }
        if (looksDangerous (bytes)) { setStatus (juce::String::fromUTF8 ("Blocked: firmware/reset/packed message - not sending.")); return; }
        if (! armButton.getToggleState()) { setStatus ("Review the bytes above, then tick 'Arm send'."); return; }

        const int oi = outBox.getSelectedId() - 1;
        if (oi < 0 || oi >= outputs.size()) { setStatus ("Pick an output port."); return; }
        auto out = juce::MidiOutput::openDevice (outputs[oi].identifier);
        if (out == nullptr) { setStatus ("Could not open output '" + outputs[oi].name + "'."); return; }

        const juce::MidiMessage m (bytes.data(), (int) bytes.size());
        out->sendMessageNow (m);
        logLine ("SENT   " + juce::String (sea::toHex (bytes.data(), bytes.size())));
        setStatus ("Sent to '" + outputs[oi].name + "'. Watching for a reply on the input...");
        armButton.setToggleState (false, juce::dontSendNotification); // re-arm required each send
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

    void timerCallback() override { drain(); }

    void logLine (const juce::String& s)
    {
        const juce::ScopedLock sl (lock);
        pending.add (s);
    }

    void drain()
    {
        juce::StringArray got;
        { const juce::ScopedLock sl (lock); got.swapWith (pending); }
        if (got.isEmpty()) return;
        for (auto& l : got) allLines.add (l);
        logView.moveCaretToEnd();
        logView.insertTextAtCaret (got.joinIntoString ("\n") + "\n");
    }

    void saveToFile()
    {
        if (allLines.isEmpty()) { setStatus ("Nothing logged yet."); return; }
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("captures");
        dir.createDirectory();
        auto f = dir.getChildFile ("probe-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");
        juce::String out;
        out << "GRAB active-probe session\nOS: " << juce::SystemStats::getOperatingSystemName()
            << "\nOutput: " << portName (outBox, outputs) << "\nInput: " << portName (inBox, inputs)
            << "\nWhen: " << juce::Time::getCurrentTime().toISO8601 (true) << "\n\n"
            << allLines.joinIntoString ("\n") << "\n";
        if (f.replaceWithText (out)) setStatus ("Saved: " + f.getFullPathName());
        else setStatus ("Could not write file.");
    }

    void submit()
    {
        if (allLines.isEmpty()) { setStatus ("Nothing logged yet."); return; }
        const auto endpoint = capturesEndpoint();

        auto* root = new juce::DynamicObject();
        root->setProperty ("schemaVersion", 1);
        root->setProperty ("kind", "midi-capture");          // a probe session is a capture (+sends)
        root->setProperty ("submittedAt", juce::Time::getCurrentTime().toISO8601 (true));
        auto* tool = new juce::DynamicObject();
        tool->setProperty ("name", "GRAB"); tool->setProperty ("version", GRAB_VERSION);
        root->setProperty ("tool", juce::var (tool));
        auto* plat = new juce::DynamicObject();
        plat->setProperty ("os", juce::SystemStats::getOperatingSystemName());
        root->setProperty ("platform", juce::var (plat));
        auto* cap = new juce::DynamicObject();
        cap->setProperty ("port", portName (outBox, outputs) + " -> " + portName (inBox, inputs) + " (probe)");
        cap->setProperty ("messageCount", allLines.size());
        juce::Array<juce::var> lines; for (auto& l : allLines) lines.add (l);
        cap->setProperty ("lines", lines);
        root->setProperty ("capture", juce::var (cap));
        root->setProperty ("notes", "active-probe session");

        const auto body = juce::JSON::toString (juce::var (root), true);
        setStatus ("Submitting probe session...");
        submitBtn.setEnabled (false);
        juce::Component::SafePointer<ProbePanel> safe (this);
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
                    p->submitBtn.setEnabled (true);
                    p->setStatus (err.isNotEmpty() ? "Submit failed (" + err + "). Try Save to file."
                                  : (code >= 200 && code < 300) ? "Thank you! Probe session submitted."
                                  : "Server returned HTTP " + juce::String (code) + ". Try Save to file.");
                }
            });
        }).detach();
    }

    static std::vector<std::uint8_t> parseHex (const juce::String& s)
    {
        std::vector<std::uint8_t> out;
        auto tokens = juce::StringArray::fromTokens (s, " ,\t\r\n", "");
        tokens.removeEmptyStrings();
        for (auto& t : tokens)
        {
            const auto v = t.getHexValue32();
            if (v >= 0 && v <= 0xFF) out.push_back ((std::uint8_t) v);
            else return {}; // bad token -> treat whole thing as invalid
        }
        return out;
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
    static juce::String portName (const juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& list)
    {
        const int i = box.getSelectedId() - 1;
        return (i >= 0 && i < list.size()) ? list[i].name : juce::String ("(none)");
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    std::vector<Probe> library;
    juce::Array<juce::MidiDeviceInfo> outputs, inputs;

    juce::Label title, outLabel, inLabel, probeLabel, valueLabel, bytesLabel, statusLabel;
    juce::ComboBox outBox, inBox, probeBox;
    juce::TextEditor valueField, customField, logView;
    juce::ToggleButton armButton;
    juce::TextButton sendButton, submitBtn, saveBtn, closeBtn;

    std::unique_ptr<juce::MidiInput> input;
    juce::CriticalSection lock;
    juce::StringArray pending, allLines;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProbePanel)
};
