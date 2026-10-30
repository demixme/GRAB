// GRAB — JUCE GUI application entry point.

#include <juce_gui_extra/juce_gui_extra.h>

#include "MainComponent.h"
#include "Theme.h"
#include "BinaryData.h"   // embedded translation files (see CMakeLists / localization/)

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

// Pick the UI language from the system locale and load its translation file (JUCE LocalisedStrings).
// Unmatched languages leave the default (no mappings), so TRANS() falls back to the English source.
// Extend coverage by wrapping more strings in TRANS() and adding lines to the localization/ files.
static void setupLocalisation()
{
    const auto lang = juce::SystemStats::getDisplayLanguage().substring (0, 2).toLowerCase();
    const char* data = nullptr; int size = 0;
    if      (lang == "fr") { data = BinaryData::grab_fr_txt; size = BinaryData::grab_fr_txtSize; }
    else if (lang == "it") { data = BinaryData::grab_it_txt; size = BinaryData::grab_it_txtSize; }
    else if (lang == "de") { data = BinaryData::grab_de_txt; size = BinaryData::grab_de_txtSize; }
    else if (lang == "es") { data = BinaryData::grab_es_txt; size = BinaryData::grab_es_txtSize; }

    if (data != nullptr)
        juce::LocalisedStrings::setCurrentMappings (
            new juce::LocalisedStrings (juce::String::fromUTF8 (data, size), /*ignoreCaseOfKeys*/ false));
}

class GRABApplication  : public juce::JUCEApplication
{
public:
    GRABApplication() = default;

    const juce::String getApplicationName() override    { return "GRAB"; }
    const juce::String getApplicationVersion() override { return GRAB_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        setupLocalisation();   // before any UI is built, so TRANS() in constructors resolves
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);   // before the window, so it picks up the dark bg
        // Show the version in the title bar so a user reporting a bug can read off their build.
        mainWindow = std::make_unique<MainWindow> (getApplicationName() + " " + getApplicationVersion());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    //==========================================================================
    class MainWindow  : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    grab::GrabLookAndFeel lookAndFeel;          // declared first, so it outlives the window
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (GRABApplication)
