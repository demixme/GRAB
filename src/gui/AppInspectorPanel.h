// AppInspectorPanel - EXPERIMENTAL. Reads a local BLOCKS "app" (a Littlefoot .littlefoot file, e.g.
// from ROLI Dashboard's bundled set) and shows what it is + its settings, the way Dashboard lays them
// out. This is the read-only first step toward app support: 100% safe (opens a file, parses its
// <metadata>), no device contact. A live LED preview (running the app in the Littlefoot VM) and a
// gated Load-to-device come later. GRAB never bundles ROLI's apps - it reads the ones already on disk.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Theme.h"

class AppInspectorPanel  : public juce::Component
{
public:
    AppInspectorPanel()
    {
        addAndMakeVisible (title);
        title.setText ("BLOCKS app inspector " + grab::u8 ("-") + " experimental", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (19.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

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

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (640, 600);
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (28));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (32);
        closeBtn.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        browseBtn.setBounds (bottom.removeFromRight (190));
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
        body.setReadOnly (false);
        body.clear();
        add (Note,  "Experimental. Opens a BLOCKS app (a .littlefoot file) and shows what it does and its settings, "
                    "the way ROLI Dashboard presents them. Read-only - it opens a file, nothing touches a device.\n\n");
        add (H1,    "Open an app to inspect\n");
        add (Body,  "The factory apps ship with ROLI Dashboard. On this machine they're usually in:\n");
        add (Note,  "    C:\\Program Files (x86)\\ROLI\\ROLI Dashboard\\Littlefoot\\\n");
        add (Body,  "e.g. Note Grid, Fader Block, Drum Block, Mixer Block, Control Grid, XYZ Pad.\n\n");
        add (Note,  "Coming next: a live LED preview (running the app in the Littlefoot VM), then an opt-in, "
                    "clearly-warned Load-to-device (apps only - never firmware).");
        body.setReadOnly (true);
        body.setCaretPosition (0);
    }

    void browse()
    {
        juce::File start ("C:\\Program Files (x86)\\ROLI\\ROLI Dashboard\\Littlefoot");
        if (! start.isDirectory())
            start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

        chooser = std::make_unique<juce::FileChooser> ("Open a .littlefoot app", start, "*.littlefoot");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                    inspect (f);
            });
    }

    // Pull the <metadata ...> ... </metadata> block out of the file's leading comment.
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
        const auto def = v.getStringAttribute ("value");
        juce::String opts;
        if (type == "option" && v.hasAttribute ("options"))
            opts = juce::String (v.getStringAttribute ("options")).replaceCharacter (';', '/');
        const auto init = v.getStringAttribute ("initStart");

        juce::String line;
        line << "    " << prefix << name;
        line << "  (" << type;
        if (rng.isNotEmpty()) line << ", " << rng;
        line << ")";
        if (def.isNotEmpty())  line << " = " << def;
        if (opts.isNotEmpty()) line << "   [" << opts << "]";
        if (init.isNotEmpty()) line << "   cfg@" << init;
        line << "\n";
        return line;
    }

    void inspect (const juce::File& f)
    {
        const auto src = f.loadFileAsString();
        const auto xml = metadataXml (src);

        body.setReadOnly (false);
        body.clear();
        add (H1, f.getFileNameWithoutExtension() + "\n");

        if (xml.isEmpty())
        {
            add (Note, "No <metadata> block in this file - it may be a shared include, a default program, or a "
                       "game with no user settings.\n");
            body.setReadOnly (true); body.setCaretPosition (0);
            return;
        }

        auto root = juce::XmlDocument::parse (xml);
        if (root == nullptr)
        {
            add (Note, "Could not parse this app's metadata.\n");
            body.setReadOnly (true); body.setCaretPosition (0);
            return;
        }

        const auto target = root->getStringAttribute ("target");
        const auto tags   = root->getStringAttribute ("tags");
        const auto desc   = root->getStringAttribute ("description");
        if (target.isNotEmpty() || tags.isNotEmpty())
            add (Note, "Target: " + target + (tags.isNotEmpty() ? "    Tags: " + tags : juce::String()) + "\n");
        if (desc.isNotEmpty())
            add (Body, desc + "\n");
        add (Body, "\n");

        int count = 0;
        if (auto* vars = root->getChildByName ("variables"))
        {
            add (H2, "Settings\n");
            for (auto* child : vars->getChildIterator())
            {
                if (child->getStringAttribute ("internal") == "true") continue;
                if (child->hasTagName ("variable"))
                {
                    add (Body, varRow (*child, {}));
                    ++count;
                }
                else if (child->hasTagName ("vector"))
                {
                    const auto n = child->getStringAttribute ("count", "?");
                    for (auto* sub : child->getChildIterator())
                    {
                        if (sub->hasTagName ("variable") && sub->getStringAttribute ("internal") != "true")
                        {
                            add (Body, varRow (*sub, "[x" + n + "] "));
                            ++count;
                        }
                    }
                }
            }
        }

        if (count == 0)
            add (Note, "This app exposes no user-configurable settings.\n");

        add (Body, "\n");
        add (Note, "These settings live inside the app's program, not in device config - so (like Dashboard) "
                   "editing them means writing the program. See PRESERVATION.md / LIGHTPAD-APPS.md.\n");

        body.setReadOnly (true);
        body.setCaretPosition (0);
    }

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton browseBtn, closeBtn;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppInspectorPanel)
};
