// AppInspectorPanel - EXPERIMENTAL. Opens a local BLOCKS "app" (a Littlefoot .littlefoot file, e.g.
// from ROLI Dashboard's bundled set), shows what it is + its settings the way Dashboard does, and
// runs it host-side in the littlefoot VM to show a live 15x15 LED preview (like Dashboard's device
// view). Read-only: opens a file / runs a sandboxed program, never contacts a device. A built-in
// demo app lets the preview run with no device and no file. GRAB never bundles ROLI's apps.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "AppPreview.h"
#include "Theme.h"

class AppInspectorPanel  : public juce::Component
{
public:
    AppInspectorPanel()
    {
        addAndMakeVisible (title);
        title.setText ("BLOCKS app inspector - experimental", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (19.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        led = std::make_unique<LEDGridComponent> (app);
        addAndMakeVisible (*led);

        addAndMakeVisible (previewStatus);
        previewStatus.setJustificationType (juce::Justification::centred);
        previewStatus.setFont (juce::Font (juce::FontOptions (13.0f)));
        previewStatus.setColour (juce::Label::textColourId, grab::theme::textPrimary.withAlpha (0.7f));

        addAndMakeVisible (body);
        body.setMultiLine (true);
        body.setReadOnly (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        body.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);
        showIntro();

        addAndMakeVisible (browseBtn);
        browseBtn.setButtonText ("Open a .littlefoot app...");
        browseBtn.onClick = [this] { browse(); };

        addAndMakeVisible (demoBtn);
        demoBtn.setButtonText ("Preview demo");
        demoBtn.onClick = [this] { previewDemo(); };

        addChildComponent (loadBtn);     // shown only once a device loader is wired (Advanced path)
        loadBtn.setButtonText ("Load to device...");
        loadBtn.setColour (juce::TextButton::buttonColourId, grab::theme::accent.withAlpha (0.25f));
        loadBtn.onClick = [this] { confirmLoadToDevice(); };

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (660, 720);
    }

    /** Wire the (experimental) load-to-device path. `query` returns the compatible device's name or
        "" when none is connected; `load` uploads an app. Makes the "Load to device..." button appear. */
    void setDeviceLoader (std::function<juce::String()> query,
                          std::function<juce::Result (const juce::String&, const juce::Array<juce::File>&)> load)
    {
        queryLoadTarget = std::move (query);
        loadToDevice    = std::move (load);
        loadBtn.setVisible (true);
        updateLoadButton();
    }

    /** Run the built-in demo app - used by the no-device preview shortcut. */
    void previewDemo()
    {
        body.setReadOnly (false); body.clear();
        add (H1, "Demo app\n");
        add (Body, "A tiny built-in Littlefoot program (not a ROLI app), running in the preview VM to show the "
                   "live LED grid with no device attached.\n");
        body.setReadOnly (true); body.setCaretPosition (0);

        const auto r = app.load (grab::demoLittleFootApp(), {});
        if (r.wasOk()) { led->start(); previewStatus.setText ("Live preview (built-in demo)", juce::dontSendNotification); }
        else           { led->stop();  previewStatus.setText ("Demo failed: " + r.getErrorMessage(), juce::dontSendNotification); }

        currentSource = grab::demoLittleFootApp();
        currentSearchPaths.clear();
        currentName = "Demo app";
        hasApp = r.wasOk();
        updateLoadButton();
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (28));
        r.removeFromTop (6);

        led->setBounds (r.removeFromTop (230));
        previewStatus.setBounds (r.removeFromTop (20));
        r.removeFromTop (8);

        auto bottom = r.removeFromBottom (32);
        closeBtn.setBounds   (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        demoBtn.setBounds    (bottom.removeFromRight (120));
        bottom.removeFromRight (8);
        browseBtn.setBounds  (bottom.removeFromRight (180));
        if (loadBtn.isVisible())
        {
            bottom.removeFromRight (8);
            loadBtn.setBounds (bottom.removeFromRight (150));
        }
        r.removeFromBottom (8);

        body.setBounds (r);
    }

private:
    enum Style { Body, H1, H2, Note };

    void add (Style s, const juce::String& text)
    {
        juce::Font f (juce::FontOptions (14.5f));
        juce::Colour col = grab::theme::textPrimary;
        switch (s)
        {
            case H1:   f = juce::Font (juce::FontOptions (16.5f)).boldened(); col = grab::theme::accent; break;
            case H2:   f = juce::Font (juce::FontOptions (14.5f)).boldened();                            break;
            case Note: f = juce::Font (juce::FontOptions (13.0f)); col = grab::theme::textPrimary.withAlpha (0.7f); break;
            case Body: default: break;
        }
        body.setFont (f);
        body.setColour (juce::TextEditor::textColourId, col);
        body.insertTextAtCaret (text);
    }

    void showIntro()
    {
        body.setReadOnly (false); body.clear();
        add (Note,  "Experimental. Opens a BLOCKS app (.littlefoot), shows its settings like Dashboard, and runs it "
                    "in a sandbox to preview the LED grid above. Read-only - nothing touches a device.\n\n");
        add (H1,    "Try it\n");
        add (Body,  "Press \"Preview demo\" for a built-in animation, or open a factory app (bundled with ROLI "
                    "Dashboard) from:\n");
        add (Note,  "    C:\\Program Files (x86)\\ROLI\\ROLI Dashboard\\Littlefoot\\\n");
        add (Body,  "e.g. Note Grid, Fader Block, Drum Block. (Some apps use device features the preview stubs, so "
                    "the picture is an approximation.)\n");
        body.setReadOnly (true); body.setCaretPosition (0);
    }

    void browse()
    {
        juce::File start ("C:\\Program Files (x86)\\ROLI\\ROLI Dashboard\\Littlefoot");
        if (! start.isDirectory())
            start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Open a .littlefoot app", start, "*.littlefoot");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            { const auto f = fc.getResult(); if (f.existsAsFile()) inspect (f); });
    }

    static juce::String metadataXml (const juce::String& src)
    {
        const auto a = src.indexOf ("<metadata");
        if (a < 0) return {};
        const auto b = src.indexOf (a, "</metadata>");
        if (b < 0) return {};
        return src.substring (a, b + (int) juce::String ("</metadata>").length());
    }

    juce::String varRow (const juce::XmlElement& v, const juce::String& prefix) const
    {
        const auto name = v.getStringAttribute ("displayName", v.getStringAttribute ("name"));
        const auto type = v.getStringAttribute ("type");
        juce::String rng;
        if (v.hasAttribute ("min") || v.hasAttribute ("max"))
            rng = v.getStringAttribute ("min") + ".." + v.getStringAttribute ("max");
        const auto def  = v.getStringAttribute ("value");
        juce::String opts;
        if (type == "option" && v.hasAttribute ("options"))
            opts = juce::String (v.getStringAttribute ("options")).replaceCharacter (';', '/');

        juce::String line;
        line << "    " << prefix << name << "  (" << type;
        if (rng.isNotEmpty()) line << ", " << rng;
        line << ")";
        if (def.isNotEmpty())  line << " = " << def;
        if (opts.isNotEmpty()) line << "   [" << opts << "]";
        line << "\n";
        return line;
    }

    void inspect (const juce::File& f)
    {
        const auto src = f.loadFileAsString();

        // Settings (from <metadata>).
        const auto xml = metadataXml (src);
        body.setReadOnly (false); body.clear();
        add (H1, f.getFileNameWithoutExtension() + "\n");

        if (auto root = (xml.isNotEmpty() ? juce::XmlDocument::parse (xml) : nullptr))
        {
            const auto target = root->getStringAttribute ("target");
            const auto tags   = root->getStringAttribute ("tags");
            const auto desc   = root->getStringAttribute ("description");
            if (target.isNotEmpty()) add (Note, "Target: " + target + (tags.isNotEmpty() ? "    Tags: " + tags : juce::String()) + "\n");
            if (desc.isNotEmpty())   add (Body, desc + "\n");
            add (Body, "\n");
            int count = 0;
            if (auto* vars = root->getChildByName ("variables"))
            {
                add (H2, "Settings\n");
                for (auto* child : vars->getChildIterator())
                {
                    if (child->getStringAttribute ("internal") == "true") continue;
                    if (child->hasTagName ("variable")) { add (Body, varRow (*child, {})); ++count; }
                    else if (child->hasTagName ("vector"))
                    {
                        const auto n = child->getStringAttribute ("count", "?");
                        for (auto* sub : child->getChildIterator())
                            if (sub->hasTagName ("variable") && sub->getStringAttribute ("internal") != "true")
                                { add (Body, varRow (*sub, "[x" + n + "] ")); ++count; }
                    }
                }
            }
            if (count == 0) add (Note, "This app exposes no user-configurable settings.\n");
        }
        else
        {
            add (Note, "No readable <metadata> - a shared include, default program, or game.\n");
        }
        body.setReadOnly (true); body.setCaretPosition (0);

        // Live LED preview: compile + run, resolving includes from the file's dir + LittleFootLibrary.
        juce::Array<juce::File> searchPaths { f.getParentDirectory(),
            f.getParentDirectory().getSiblingFile ("LittleFootLibrary") };
        const auto r = app.load (src, searchPaths);
        if (r.wasOk()) { led->start(); previewStatus.setText ("Live preview", juce::dontSendNotification); }
        else           { led->stop();  previewStatus.setText ("Preview unavailable - " + r.getErrorMessage(), juce::dontSendNotification); }

        currentSource = src;
        currentSearchPaths = searchPaths;
        currentName = f.getFileNameWithoutExtension();
        hasApp = r.wasOk();
        updateLoadButton();
    }

    void updateLoadButton()
    {
        const bool haveLoader = loadToDevice != nullptr;
        const juce::String target = (haveLoader && queryLoadTarget) ? queryLoadTarget() : juce::String();
        loadBtn.setEnabled (hasApp && haveLoader && target.isNotEmpty());
        loadBtn.setTooltip (! haveLoader ? juce::String()
                            : ! hasApp    ? juce::String ("Open or preview an app first.")
                            : target.isEmpty() ? juce::String ("Connect a Lightpad Block to load an app onto it.")
                            : "Load this app onto " + target + " (temporary - a power-cycle restores it).");
    }

    void confirmLoadToDevice()
    {
        if (loadToDevice == nullptr || ! hasApp) return;
        const juce::String target = queryLoadTarget ? queryLoadTarget() : juce::String();
        if (target.isEmpty()) return;

        const juce::String msg =
            "EXPERIMENTAL - load a BLOCKS app onto your " + target + ".\n\n"
            "This uploads the app you're previewing (\"" + currentName + "\") to the device as a Littlefoot "
            "program. It loads TEMPORARILY: it is not written to flash, so turning the Block off and on again "
            "restores whatever app was there before.\n\n"
            "GRAB only ever uploads apps - never firmware or bootloader code - so the risk is low, but this is "
            "unofficial and unsupported. In the worst case an app can misbehave until you power-cycle the Block. "
            "Proceed only if you understand that.\n\n"
            "Load \"" + currentName + "\" onto " + target + " now?";

        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
            "Load app to device (experimental)", msg, "Load it", "Cancel", this,
            juce::ModalCallbackFunction::create ([this] (int result) { if (result == 1) doLoadToDevice(); }));
    }

    void doLoadToDevice()
    {
        previewStatus.setText ("Sending app to device...", juce::dontSendNotification);
        const auto r = loadToDevice (currentSource, currentSearchPaths);
        if (r.wasOk())
            previewStatus.setText ("Upload started - if it compiles and the Lightpad stays connected, "
                                   "the app appears in a moment.", juce::dontSendNotification);
        else
            previewStatus.setText ("Load failed: " + r.getErrorMessage(), juce::dontSendNotification);
    }

    LittleFootApp   app;                       // declared before led (led holds a reference to it)
    std::unique_ptr<LEDGridComponent> led;
    juce::Label      title, previewStatus;
    juce::TextEditor body;
    juce::TextButton browseBtn, demoBtn, loadBtn, closeBtn;
    std::unique_ptr<juce::FileChooser> chooser;

    // Experimental load-to-device (wired by MainComponent via setDeviceLoader).
    std::function<juce::String()> queryLoadTarget;
    std::function<juce::Result (const juce::String&, const juce::Array<juce::File>&)> loadToDevice;
    juce::String currentSource, currentName;
    juce::Array<juce::File> currentSearchPaths;
    bool hasApp = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppInspectorPanel)
};
