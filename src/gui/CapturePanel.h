// CapturePanel — the one MIDI capture tool. Opens any chosen MIDI input and logs every message it
// emits, decoding each from the bytes themselves: ROLI config changes become plain English + value
// (via a describe callback that applies the connected device's names), and everything else (keepalives,
// topology, notes, touch, unknown devices' traffic) is annotated by message type. Strictly READ-ONLY:
// it listens, never transmits.
//
// One tool, three ways out: Save to file, Open a prefilled GitHub issue, or Submit to the self-learn
// store. The log is a selectable table (copy a row or the lot) showing English + raw SysEx side by side.
// This is how an owner of a device GRAB doesn't fully model (LUMI, Lightpad, RISE/GRAND) shows us what
// it does — record while changing settings in ROLI Dashboard, then send it in.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <sea_sysex.h>

#include "Anonymize.h"
#include "Theme.h"

#include <cstdint>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

class CapturePanel  : public juce::Component,
                      private juce::MidiInputCallback,
                      private juce::TableListBoxModel,
                      private juce::Timer
{
public:
    // describe: item id + value -> (plain-English name, formatted value). Supplied by MainComponent so
    // config changes read as "Pressure Sensitivity = 46" when the device is connected; falls back to
    // "item N" otherwise. May be empty.
    using DescribeFn = std::function<std::pair<juce::String, juce::String> (juce::uint32, juce::int32)>;

    explicit CapturePanel (DescribeFn describeToUse = {})
        : describe (std::move (describeToUse))
    {
        title.setText (juce::String::fromUTF8 (
                           "MIDI capture (read-only). Pick a port, press Start, then play or operate your device "
                           "\xe2\x80\x94 or change settings in ROLI Dashboard to capture those. Config changes are "
                           "shown in plain English; everything is logged raw. GRAB only listens, it never sends. "
                           "Save, open a GitHub issue, or Submit to help add device support."),
                       juce::dontSendNotification);
        title.setJustificationType (juce::Justification::topLeft);
        title.setColour (juce::Label::textColourId, grab::theme::textSecondary);
        addAndMakeVisible (title);

        addAndMakeVisible (portLabel);
        portLabel.setText ("Input port:", juce::dontSendNotification);
        addAndMakeVisible (portBox);
        addAndMakeVisible (refreshBtn);
        refreshBtn.onClick = [this] { refreshPorts(); };
        refreshPorts();

        addAndMakeVisible (guideBtn);
        guideBtn.setButtonText ("Capture guide");
        guideBtn.setTooltip ("Opens the step-by-step Dashboard/USB capture guide on GitHub.");
        guideBtn.onClick = [] { juce::URL (kGuideUrl).launchInDefaultBrowser(); };

        addAndMakeVisible (table);
        table.setModel (this);
        table.setColour (juce::TableListBox::backgroundColourId, grab::theme::panelBgAlt);
        table.setColour (juce::TableListBox::outlineColourId,    grab::theme::outline);
        table.setOutlineThickness (1);
        table.setMultipleSelectionEnabled (true);
        table.setHeaderHeight (24);
        table.setRowHeight (22);
        {
            auto& h = table.getHeader();
            const int f = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable;
            h.addColumn ("Time",    colTime,    78,  60, 140, f);
            h.addColumn ("Kind",    colKind,    74,  54, 120, f);
            h.addColumn ("Message", colMsg,     230, 120, 460, f);
            h.addColumn ("Value",   colValue,   110, 60,  260, f);
            h.addColumn ("Raw MIDI / SysEx", colRaw, 340, 160, 10000, f);
        }

        addAndMakeVisible (configOnlyToggle);
        configOnlyToggle.setButtonText ("Config changes only");
        configOnlyToggle.setTooltip ("Hide keepalives, topology and performance data; show only setting changes.");
        configOnlyToggle.onClick = [this] { rebuildView(); };

        addAndMakeVisible (copyRowBtn);
        copyRowBtn.setButtonText ("Copy row");
        copyRowBtn.onClick = [this] { copySelected(); };
        addAndMakeVisible (copyAllBtn);
        copyAllBtn.setButtonText ("Copy all");
        copyAllBtn.onClick = [this] { copyAll(); };

        addAndMakeVisible (nameLabel);
        nameLabel.setText ("Name to credit (optional):", juce::dontSendNotification);
        addAndMakeVisible (nameField);
        nameField.setText (loadCredit(), juce::dontSendNotification);   // shared with Contribute, remembered

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

        setSize (1040, 620);
        setStatus ("Idle. Pick a port and press Start.");
    }

    ~CapturePanel() override { stop(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (60));
        r.removeFromTop (4);

        auto top = r.removeFromTop (26);
        portLabel.setBounds (top.removeFromLeft (70));
        guideBtn.setBounds (top.removeFromRight (104).reduced (2, 0));
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

        auto nameRow = r.removeFromBottom (24);
        nameLabel.setBounds (nameRow.removeFromLeft (210));
        nameField.setBounds (nameRow);
        r.removeFromBottom (6);

        auto filterRow = r.removeFromBottom (26);
        copyAllBtn.setBounds (filterRow.removeFromRight (84).reduced (2, 0));
        copyRowBtn.setBounds (filterRow.removeFromRight (84).reduced (2, 0));
        configOnlyToggle.setBounds (filterRow.withTrimmedLeft (2));
        r.removeFromBottom (6);

        table.setBounds (r);
        stretchRawColumn();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)) { copySelected(); return true; }
        return false;
    }

private:
    enum ColumnIds { colTime = 1, colKind, colMsg, colValue, colRaw };

    struct Row { juce::String time, kind, msg, value, raw; bool isConfig = false; };
    struct Pending { juce::String time, kind, desc, rawHex; std::vector<sea::ConfigChange> changes; };

    // Classify a message by what it IS (which is the strongest hint at where it came from): performance
    // data is you touching the device; a setting change is ROLI Dashboard / GRAB / a hardware control;
    // the rest is housekeeping. The capture port only sees device->host, so this is as far as the bytes
    // alone can attribute a message.
    static juce::String kindOf (const juce::MidiMessage& m, bool hasConfig)
    {
        if (hasConfig)                                     return "Setting";
        if (m.isSysEx())                                   return "Status";
        if (m.isNoteOnOrOff())                             return "Note";
        if (m.isController())                              return "CC";
        if (m.isPitchWheel())                              return "Pitch";
        if (m.isChannelPressure() || m.isAftertouch())     return "Pressure";
        return "Other";
    }

    //==============================================================================
    // TableListBoxModel
    int getNumRows() override { return (int) view.size(); }

    void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
    {
        if (selected)            g.fillAll (grab::theme::accent.withAlpha (0.25f));
        else if (row % 2)        g.fillAll (grab::theme::panelBg.withAlpha (0.35f));
    }

    void paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool) override
    {
        if (row < 0 || row >= (int) view.size())
            return;
        const auto& e = rows[(std::size_t) view[(std::size_t) row]];

        juce::String text;
        switch (columnId)
        {
            case colTime:  text = e.time;  break;
            case colKind:  text = e.kind;  break;
            case colMsg:   text = e.msg;   break;
            case colValue: text = e.value; break;
            case colRaw:   text = e.raw;   break;
            default: break;
        }

        g.setColour (columnId == colMsg   ? grab::theme::textPrimary
                   : columnId == colValue ? grab::theme::accent
                   : columnId == colKind  ? (e.isConfig ? grab::theme::accent : grab::theme::textSecondary)
                                          : grab::theme::textSecondary);
        const bool mono = (columnId == colRaw || columnId == colTime);
        g.setFont (mono ? juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain))
                        : juce::Font (juce::FontOptions (13.0f)));
        g.drawText (text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }

    void stretchRawColumn()
    {
        auto& h = table.getHeader();
        if (table.getWidth() <= 0)
            return;
        const int others = h.getColumnWidth (colTime) + h.getColumnWidth (colKind)
                         + h.getColumnWidth (colMsg) + h.getColumnWidth (colValue);
        const int sb    = table.getVerticalScrollBar().isVisible() ? table.getVerticalScrollBar().getWidth() : 0;
        // Fill the leftover width (so there's no blank gap and no forced horizontal scroll); a small
        // floor means it still has room, and a wider window grows it to fit the whole SysEx string.
        h.setColumnWidth (colRaw, juce::jmax (160, table.getWidth() - others - sb - 4));
    }

    //==============================================================================
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
            setStatus ("Could not open '" + inputs[idx].name
                       + juce::String::fromUTF8 ("' \xe2\x80\x94 it may be held by another app "
                                                 "(or GRAB's own connection \xe2\x80\x94 disconnect first)."));
            return;
        }

        { const juce::ScopedLock sl (lock); pending.clear(); }
        rows.clear();
        view.clear();
        table.updateContent();
        messageCount = 0;
        capturedPortName = inputs[idx].name;
        startMs = juce::Time::getMillisecondCounterHiRes();
        input->start();
        startTimer (100);

        startBtn.setEnabled (false);
        stopBtn.setEnabled (true);
        portBox.setEnabled (false);
        refreshBtn.setEnabled (false);
        setStatus ("Listening on '" + capturedPortName
                   + juce::String::fromUTF8 ("' \xe2\x80\x94 play or operate your device (or edit it in Dashboard)."));
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
                       + "'. Save, Submit, or Open issue.");
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        // MIDI thread: decode what's cheap and thread-safe here (raw hex + config item/value from the
        // bytes), queue it, and let the timer build the friendly rows on the UI thread (where the
        // describe callback may touch the connected device).
        const auto* d = reinterpret_cast<const std::uint8_t*> (m.getRawData());
        const auto  n = (std::size_t) m.getRawDataSize();
        const auto  safe = grab::anonymize (d, n);   // no serial / MAC in what gets shared
        const double t = juce::Time::getMillisecondCounterHiRes() - startMs;

        Pending p;
        p.time    = "[" + juce::String (t / 1000.0, 3).paddedLeft (' ', 9) + "s]";
        p.rawHex  = juce::String (sea::toHex (safe.data(), safe.size()));
        p.changes = sea::extractConfigChanges (d, n);
        p.kind    = kindOf (m, ! p.changes.empty());
        // Non-config description: ROLI housekeeping gets the sysex annotation; channel-voice messages
        // (you playing) get JUCE's readable text, e.g. "Note on C3 Velocity 100 Channel 1".
        p.desc    = m.isSysEx() ? juce::String (sea::annotate (d, n)) : m.getDescription();

        const juce::ScopedLock sl (lock);
        pending.push_back (std::move (p));
    }

    void timerCallback() override { drain(); }

    void drain()
    {
        std::vector<Pending> got;
        { const juce::ScopedLock sl (lock); got.swap (pending); }
        if (got.empty())
            return;

        for (auto& p : got)
        {
            ++messageCount;
            if (! p.changes.empty())
            {
                for (const auto& c : p.changes)
                {
                    auto nv = describe ? describe (c.item, c.value)
                                       : std::make_pair (juce::String ("item ") + juce::String (c.item),
                                                         juce::String (c.value));
                    addRow ({ p.time, p.kind, nv.first, nv.second, p.rawHex, true });
                }
            }
            else
            {
                addRow ({ p.time, p.kind, p.desc, {}, p.rawHex, false });
            }
        }

        table.updateContent();
        stretchRawColumn();        // the vertical scrollbar may have just appeared, changing the width left
        table.scrollToEnsureRowIsOnscreen ((int) view.size() - 1);

        if (rows.size() > 50000)   // safety: auto-stop a runaway capture
        {
            stop();
            setStatus ("Stopped at 50000 rows (capture limit). Save or Submit.");
        }
    }

    void addRow (Row&& r)
    {
        const bool show = ! configOnlyToggle.getToggleState() || r.isConfig;
        rows.push_back (std::move (r));
        if (show)
            view.push_back ((int) rows.size() - 1);
    }

    void rebuildView()
    {
        view.clear();
        const bool configOnly = configOnlyToggle.getToggleState();
        for (int i = 0; i < (int) rows.size(); ++i)
            if (! configOnly || rows[(std::size_t) i].isConfig)
                view.push_back (i);
        table.updateContent();
        stretchRawColumn();
        table.repaint();
    }

    //==============================================================================
    juce::String rowToText (const Row& r) const
    {
        // Tab-separated: time, kind, message, value, raw — pastes cleanly into an issue or a sheet.
        return r.time + "\t" + r.kind + "\t" + r.msg + "\t" + r.value + "\t" + r.raw;
    }

    void copySelected()
    {
        const auto sel = table.getSelectedRows();
        if (sel.isEmpty()) { setStatus ("Select one or more rows first."); return; }

        juce::StringArray out;
        for (int i = 0; i < sel.size(); ++i)
        {
            const int viewRow = sel[i];
            if (viewRow >= 0 && viewRow < (int) view.size())
                out.add (rowToText (rows[(std::size_t) view[(std::size_t) viewRow]]));
        }
        juce::SystemClipboard::copyTextToClipboard (out.joinIntoString ("\n"));
        setStatus (juce::String (out.size()) + " row(s) copied.");
    }

    void copyAll()
    {
        if (rows.empty()) { setStatus ("Nothing captured yet."); return; }
        juce::StringArray out;
        out.add ("Time\tKind\tMessage\tValue\tRaw MIDI / SysEx");
        for (auto& r : rows)
            out.add (rowToText (r));
        juce::SystemClipboard::copyTextToClipboard (out.joinIntoString ("\n"));
        setStatus (juce::String (rows.size()) + " rows copied.");
    }

    //==============================================================================
    // One line per captured row for file / issue / submit: raw hex + decoded label.
    juce::StringArray buildLines() const
    {
        juce::StringArray out;
        for (auto& r : rows)
        {
            juce::String line = r.time + " " + r.raw + "   [" + r.kind + "] ";
            line << (r.isConfig ? (r.msg + " = " + r.value) : r.msg);
            out.add (line);
        }
        return out;
    }

    void saveToFile()
    {
        if (rows.empty()) { setStatus ("Nothing captured yet."); return; }

        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("captures");
        dir.createDirectory();
        auto f = dir.getChildFile ("capture-" + sanitize (capturedPortName) + "-"
                     + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");

        juce::String out;
        out << "GRAB MIDI capture\n"
            << "Port: " << capturedPortName << "\n"
            << "OS:   " << juce::SystemStats::getOperatingSystemName() << "\n"
            << "When: " << juce::Time::getCurrentTime().toISO8601 (true) << "\n"
            << (nameField.getText().trim().isNotEmpty() ? "Credit: " + nameField.getText().trim() + "\n" : juce::String())
            << "Notes: " << notesField.getText() << "\n"
            << "Messages: " << messageCount << "\n\n"
            << buildLines().joinIntoString ("\n") << "\n";

        if (f.replaceWithText (out))
            setStatus ("Saved: " + f.getFullPathName());
        else
            setStatus ("Could not write file.");
    }

    void openIssue()
    {
        const auto lines = buildLines();
        juce::String body;
        body << "**Device model:** " << notesField.getText() << "\n"
             << (nameField.getText().trim().isNotEmpty() ? "**Credit:** " + nameField.getText().trim() + "\n" : juce::String())
             << "**OS:** " << juce::SystemStats::getOperatingSystemName() << "\n"
             << "**Port:** " << capturedPortName << "\n\n"
             << juce::String::fromUTF8 ("MIDI capture (GRAB -> Capture -> Save to file) \xe2\x80\x94 "
                                        "**please attach the saved .txt file**. First lines:\n```\n")
             << lines.joinIntoString ("\n").substring (0, 1500) << "\n```\n";

        juce::URL url ("https://github.com/demixme/GRAB/issues/new");
        url = url.withParameter ("labels", "device")
                 .withParameter ("title", "[capture] ")
                 .withParameter ("body", body);
        url.launchInDefaultBrowser();
        setStatus (juce::String::fromUTF8 ("Opened a prefilled issue \xe2\x80\x94 attach the saved capture file to it."));
    }

    void submit()
    {
        if (rows.empty()) { setStatus ("Nothing captured yet."); return; }
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
        for (auto& l : buildLines()) lines.add (l);
        cap->setProperty ("lines", lines);
        root->setProperty ("capture", juce::var (cap));
        root->setProperty ("notes", notesField.getText());
        const auto credit = nameField.getText().trim();
        if (credit.isNotEmpty()) { root->setProperty ("credit", credit); saveCredit (credit); }  // opt-in, remembered

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

    // Opt-in contributor name, shared with the Contribute dialog and remembered across sessions.
    static juce::String loadCredit()
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        return props.getValue ("contributeCredit", juce::String());
    }
    static void saveCredit (const juce::String& c)
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        props.setValue ("contributeCredit", c);
        props.saveIfNeeded();
    }

    static juce::String sanitize (const juce::String& s)
    {
        return s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    //==============================================================================
    static constexpr const char* kGuideUrl =
        "https://github.com/demixme/GRAB/blob/main/tools/CAPTURE-DASHBOARD.md";

    DescribeFn describe;

    juce::Label       title, portLabel, nameLabel, notesLabel, statusLabel;
    juce::ComboBox    portBox;
    juce::Array<juce::MidiDeviceInfo> inputs;
    juce::TextButton  refreshBtn { "Refresh" }, startBtn { "Start" }, stopBtn { "Stop" },
                      submitBtn { "Submit" }, saveBtn { "Save to file" },
                      issueBtn { "Open issue" }, closeBtn { "Close" },
                      copyRowBtn { "Copy row" }, copyAllBtn { "Copy all" },
                      guideBtn { "Capture guide" };
    juce::ToggleButton configOnlyToggle;
    juce::TableListBox table;
    juce::TextEditor  notesField, nameField;

    std::unique_ptr<juce::MidiInput> input;
    juce::CriticalSection lock;
    std::vector<Pending> pending;   // MIDI -> UI handoff
    std::vector<Row>     rows;      // full capture (all rows)
    std::vector<int>     view;      // indices of rows shown under the current filter
    double startMs = 0.0;
    int    messageCount = 0;
    juce::String capturedPortName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapturePanel)
};
