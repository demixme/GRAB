// AppPreview - runs a BLOCKS Littlefoot app host-side in the open-source littlefoot VM and captures
// its 15x15 LED output, the way ROLI Dashboard previews an app. Read-only: it compiles and executes
// a program in a sandbox; it never contacts a device. The device-facing native functions (MIDI,
// touch, cluster, config) are host stubs - enough to drive the drawing. GRAB never uploads programs.
//
// The VM (roli_blocks_basics littlefoot) is kept in AppPreview.cpp behind a pimpl, because the
// vendored Runner calls some juce functions unqualified and so must be compiled with
// `using namespace juce` - which this GRAB target otherwise disables.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <memory>

#include "Theme.h"

class LittleFootApp
{
public:
    static constexpr int W = 15, H = 15;             // Lightpad LED grid

    LittleFootApp();
    ~LittleFootApp();

    /** Compile + load a Littlefoot source. searchPaths resolves its #includes (e.g. the
        LittleFootLibrary folder). Returns an error string on failure; calls initialise() on success. */
    juce::Result load (const juce::String& source, const juce::Array<juce::File>& searchPaths);

    void repaint();                                  // runs the app's repaint() into the LED grid
    bool isLoaded() const;
    juce::uint32 pixel (int x, int y) const;         // 0xAARRGGBB at cell (x,y)

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LittleFootApp)
};

//==============================================================================
/** Renders a LittleFootApp's 15x15 LED grid as a Lightpad-style panel of rounded cells. */
class LEDGridComponent  : public juce::Component,
                          private juce::Timer
{
public:
    explicit LEDGridComponent (LittleFootApp& appToShow) : app (appToShow) {}

    void start() { startTimer (33); }   // ~30 fps
    void stop()  { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        const float side = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (side, side);
        const float cell = side / (float) LittleFootApp::W;
        const float pad  = cell * 0.12f;

        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.expanded (4.0f), 8.0f);

        for (int y = 0; y < LittleFootApp::H; ++y)
            for (int x = 0; x < LittleFootApp::W; ++x)
            {
                g.setColour (juce::Colour (app.pixel (x, y)));
                g.fillRoundedRectangle (r.getX() + x * cell + pad, r.getY() + y * cell + pad,
                                        cell - 2 * pad, cell - 2 * pad, (cell - 2 * pad) * 0.25f);
            }
    }

private:
    void timerCallback() override { app.repaint(); if (! app.isLoaded()) stopTimer(); repaint(); }

    LittleFootApp& app;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LEDGridComponent)
};

namespace grab
{
    /** The Littlefoot preamble synthesised from an app's <metadata> (globals + vector accessors,
        seeded to their value= defaults) - the same code ROLI Dashboard generates before compiling.
        Prepend it to the raw source so a metadata-driven app compiles. Empty if there's no metadata.
        Used by both the preview VM and the (experimental) load-to-device path, so they compile the
        same bytes. */
    juce::String generateMetadataPreamble (const juce::String& source);

    // A tiny built-in demo app so the LED preview can be shown with no device and no file.
    inline const char* demoLittleFootApp()
    {
        return R"littlefoot(
            int t0;
            void initialise() { t0 = getMillisecondCounter(); }
            void repaint()
            {
                int t = getMillisecondCounter();
                for (int y = 0; y < 15; ++y)
                    for (int x = 0; x < 15; ++x)
                    {
                        int r = (x * 17 + t / 11) & 255;
                        int g = (y * 17 + t / 13) & 255;
                        int b = ((x + y) * 9 + t / 7) & 255;
                        fillPixel(makeARGB(255, r, g, b), x, y);
                    }
            }
        )littlefoot";
    }
}
