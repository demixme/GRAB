// ContributePanel — the "Contribute device profile" dialog. Shows exactly what
// will be sent (anonymized JSON), lets the user set the endpoint + notes, then
// POSTs to the configured store or saves to a file. See DEVICE-PROFILES.md.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "DeviceProfile.h"

#include <thread>

class ContributePanel  : public juce::Component
{
public:
    // Holds a ref-counted Block::Ptr so the device stays alive for the dialog's
    // lifetime even if the topology changes (battery heartbeat fires constantly).
    ContributePanel (roli::Block::Ptr blockToUse, const juce::String& transportToUse)
        : block (std::move (blockToUse)), transport (transportToUse)
    {
        title.setText ("This sends an anonymized profile of what your device reports. "
                       "Your serial is hashed, never sent raw.", juce::dontSendNotification);
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
        endpointField.setText (loadEndpoint(), juce::dontSendNotification);

        addAndMakeVisible (nameLabel);
        nameLabel.setText ("Name to credit (optional):", juce::dontSendNotification);
        addAndMakeVisible (nameField);
        nameField.setText (loadCredit(), juce::dontSendNotification);   // remembered across sessions
        nameField.onTextChange = [this] { rebuildJson(); };

        addAndMakeVisible (notesLabel);
        notesLabel.setText ("Notes (optional):", juce::dontSendNotification);
        addAndMakeVisible (notesField);
        notesField.setMultiLine (true);
        notesField.onTextChange = [this] { rebuildJson(); };

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (submitBtn);
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (closeBtn);
        submitBtn.onClick = [this] { submit(); };
        saveBtn.onClick   = [this] { saveToFile(); };
        closeBtn.onClick  = [this]
        {
            // Defer deleting the owning dialog until after this callback returns
            // (the dialog owns this panel).
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        rebuildJson();
        setSize (560, 552);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (40));
        r.removeFromTop (4);

        auto buttons = r.removeFromBottom (30);
        closeBtn .setBounds (buttons.removeFromRight (90).reduced (2, 0));
        saveBtn  .setBounds (buttons.removeFromRight (110).reduced (2, 0));
        submitBtn.setBounds (buttons.removeFromRight (90).reduced (2, 0));
        statusLabel.setBounds (buttons);
        r.removeFromBottom (6);

        auto notes = r.removeFromBottom (60);
        notesLabel.setBounds (notes.removeFromTop (20));
        notesField.setBounds (notes);
        r.removeFromBottom (6);

        auto nameRow = r.removeFromBottom (24);
        nameLabel.setBounds (nameRow.removeFromLeft (180));
        nameField.setBounds (nameRow);
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

    static juce::String loadEndpoint()
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        return props.getValue ("profileEndpoint", "https://www.demix.me/grab/v1/profiles");
    }

    // Opt-in contributor name, shared with the Capture dialog and remembered across sessions.
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

    void saveEndpoint (const juce::String& ep)
    {
        juce::PropertiesFile::Options o; o.millisecondsBeforeSaving = 0;
        juce::PropertiesFile props (settingsFile(), o);
        props.setValue ("profileEndpoint", ep);
        props.saveIfNeeded();
    }

    void rebuildJson()
    {
        profile = grab::buildDeviceProfile (*block, transport, notesField.getText(), nameField.getText().trim());
        json = grab::profileToJson (profile);
        jsonView.setText (json, juce::dontSendNotification);
    }

    void setStatus (const juce::String& s) { statusLabel.setText (s, juce::dontSendNotification); }

    void saveToFile()
    {
        rebuildJson();
        // Per-user dir (not the working directory — a Finder/Explorer-launched app has an
        // unpredictable CWD it can't write to). Matches Capture/Diagnostics.
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("GRAB").getChildFile ("profiles");
        dir.createDirectory();
        auto f = dir.getChildFile (juce::String (grab::blockTypeName (block->getType()))
                                   + "-" + block->versionNumber + "-"
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
        saveEndpoint (endpoint);
        saveCredit (nameField.getText().trim());   // remember the name for next time
        setStatus ("Submitting...");
        submitBtn.setEnabled (false);

        const auto body = json;
        juce::Component::SafePointer<ContributePanel> safe (this);
        std::thread ([safe, endpoint, body]
        {
            int code = 0;
            juce::String err;
            {
                juce::URL url (endpoint);
                url = url.withPOSTData (body);
                juce::WebInputStream web (url, true);
                web.withExtraHeaders ("Content-Type: application/json");
                web.withConnectionTimeout (15000); // never hang the submit indefinitely
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
            setStatus ("Submit failed (" + err + "). You can Save to file instead.");
        else if (code >= 200 && code < 300)
            setStatus ("Thank you! Submitted (HTTP " + juce::String (code) + ").");
        else
            setStatus ("Server returned HTTP " + juce::String (code) + ". Try Save to file.");
    }

    roli::Block::Ptr block;
    juce::String transport;
    juce::var profile;
    juce::String json;

    juce::Label title, endpointLabel, nameLabel, notesLabel, statusLabel;
    juce::TextEditor jsonView, endpointField, nameField, notesField;
    juce::TextButton submitBtn { "Submit" }, saveBtn { "Save to file" }, closeBtn { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContributePanel)
};
