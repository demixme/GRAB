// DiagnosticsPanel — the "Report connection" dialog. Shows an anonymized connection
// diagnostic (the MIDI ports GRAB saw + how the attempt ended) and lets the user
// submit it to the store, save it to a file, or open it as a prefilled GitHub issue.
// See Diagnostics.h.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Diagnostics.h"

#include <thread>

class DiagnosticsPanel  : public juce::Component
{
public:
    explicit DiagnosticsPanel (juce::var diagnosticToUse)
        : diagnostic (std::move (diagnosticToUse))
    {
        title.setText ("This reports how GRAB saw your MIDI ports and how the connection "
                       "attempt ended, to help diagnose and add device support (Seaboard "
                       "RISE/GRAND especially). No audio or notes you play; your serial, if "
                       "a device connected, is hashed.", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (title);

        jsonView.setMultiLine (true);
        jsonView.setReadOnly (true);
        jsonView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0)));
        jsonView.setCaretVisible (false);
        addAndMakeVisible (jsonView);

        addAndMakeVisible (endpointLabel);
        endpointLabel.setText ("Endpoint:", juce::dontSendNotification);
        addAndMakeVisible (endpointField);
        endpointField.setText (diagnosticsEndpoint(), juce::dontSendNotification);

        addAndMakeVisible (notesLabel);
        notesLabel.setText ("Notes (device model, what you tried):", juce::dontSendNotification);
        addAndMakeVisible (notesField);
        notesField.setMultiLine (true);
        notesField.onTextChange = [this] { updateNotes(); };

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (submitBtn);
        addAndMakeVisible (issueBtn);
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (closeBtn);
        submitBtn.onClick = [this] { submit(); };
        issueBtn.onClick  = [this] { openIssue(); };
        saveBtn.onClick   = [this] { saveToFile(); };
        closeBtn.onClick  = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        rebuildJson();
        setSize (580, 560);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (54));
        r.removeFromTop (4);

        auto buttons = r.removeFromBottom (30);
        closeBtn .setBounds (buttons.removeFromRight (80).reduced (2, 0));
        saveBtn  .setBounds (buttons.removeFromRight (100).reduced (2, 0));
        issueBtn .setBounds (buttons.removeFromRight (110).reduced (2, 0));
        submitBtn.setBounds (buttons.removeFromRight (90).reduced (2, 0));
        statusLabel.setBounds (buttons);
        r.removeFromBottom (6);

        auto notes = r.removeFromBottom (54);
        notesLabel.setBounds (notes.removeFromTop (20));
        notesField.setBounds (notes);
        r.removeFromBottom (6);

        auto ep = r.removeFromBottom (24);
        endpointLabel.setBounds (ep.removeFromLeft (70));
        endpointField.setBounds (ep);
        r.removeFromBottom (6);

        jsonView.setBounds (r);
    }

private:
    static juce::File settingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GRAB").getChildFile ("settings.properties");
    }

    static juce::String diagnosticsEndpoint()
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        const auto base = props.getValue ("profileEndpoint", "https://www.demix.me/grab/v1/profiles");
        return base.replace ("profiles", "diagnostics");
    }

    void updateNotes()
    {
        if (auto* o = diagnostic.getDynamicObject())
            o->setProperty ("notes", notesField.getText());
        rebuildJson();
    }

    void rebuildJson()
    {
        json = juce::JSON::toString (diagnostic, false);
        jsonView.setText (json, juce::dontSendNotification);
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    void saveToFile()
    {
        rebuildJson();
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("diagnostics");
        dir.createDirectory();
        auto f = dir.getChildFile ("diagnostic-"
                     + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".json");
        if (f.replaceWithText (json))
            setStatus ("Saved: " + f.getFullPathName());
        else
            setStatus ("Could not write file.");
    }

    void submit()
    {
        const auto endpoint = endpointField.getText().trim();
        if (endpoint.isEmpty() || ! juce::URL (endpoint).isWellFormed())
        {
            setStatus ("Enter a valid endpoint URL (or use Save to file).");
            return;
        }
        rebuildJson();
        setStatus ("Submitting...");
        submitBtn.setEnabled (false);

        const auto body = json;
        juce::Component::SafePointer<DiagnosticsPanel> safe (this);
        std::thread ([safe, endpoint, body]
        {
            int code = 0;
            juce::String err;
            {
                juce::URL url (endpoint);
                url = url.withPOSTData (body);
                juce::WebInputStream web (url, true);
                web.withExtraHeaders ("Content-Type: application/json");
                web.withConnectionTimeout (15000);
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
            setStatus ("Submit failed (" + err + "). You can Save to file or Open as issue.");
        else if (code >= 200 && code < 300)
            setStatus ("Thank you! Reported (HTTP " + juce::String (code) + ").");
        else
            setStatus ("Server returned HTTP " + juce::String (code) + ". Try Save or Open as issue.");
    }

    /** Short human-readable summary of the ports, for a GitHub issue body. */
    juce::String portsSummary() const
    {
        juce::String s;
        const auto ports = diagnostic.getProperty ("ports", juce::var());
        for (auto dir : { std::make_pair ("inputs", "IN "), std::make_pair ("outputs", "OUT") })
        {
            if (auto* arr = ports.getProperty (dir.first, juce::var()).getArray())
                for (const auto& p : *arr)
                    s << dir.second << " [" << p.getProperty ("transport", "?").toString() << "] "
                      << p.getProperty ("name", "").toString()
                      << (bool (p.getProperty ("seaboard", false)) ? "  <seaboard>" : "")
                      << "  id=" << p.getProperty ("identifier", "").toString() << "\n";
        }
        return s;
    }

    void openIssue()
    {
        const auto os      = diagnostic.getProperty ("platform", juce::var()).getProperty ("os", "").toString();
        const auto session = diagnostic.getProperty ("session", juce::var());
        const auto outcome = session.getProperty ("outcome", "").toString();

        juce::String body;
        body << "**Device model:** \n"
             << "**OS:** " << os << "\n"
             << "**Requested transport:** " << session.getProperty ("requestedTransport", "").toString() << "\n"
             << "**Outcome:** " << outcome << "\n\n"
             << "**Ports GRAB saw:**\n```\n" << portsSummary() << "```\n"
             << "**Notes:** " << notesField.getText() << "\n";
        if (body.length() > 5000)
            body = body.substring (0, 5000) + "\n…(truncated)";

        juce::URL url ("https://github.com/demixme/GRAB/issues/new");
        url = url.withParameter ("labels", "connection")
                 .withParameter ("title", "[connect] ")
                 .withParameter ("body", body);
        url.launchInDefaultBrowser();
        setStatus ("Opened a prefilled issue in your browser — review and submit it there.");
    }

    juce::var    diagnostic;
    juce::String json;

    juce::Label title, endpointLabel, notesLabel, statusLabel;
    juce::TextEditor jsonView, endpointField, notesField;
    juce::TextButton submitBtn { "Submit" }, issueBtn { "Open as issue" },
                     saveBtn { "Save to file" }, closeBtn { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiagnosticsPanel)
};
