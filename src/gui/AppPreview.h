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

#include "DeviceArtData.h"
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

    /** Draw the designer's Lightpad SVG (recolouring its 225 led-y-x cells from the LED buffer)
        instead of the hand-drawn grid. Falls back to the drawn grid if the art can't be loaded. */
    void setUseDeviceArtV2 (bool b) { if (useV2 == b) return; useV2 = b; repaint(); }

    void paint (juce::Graphics& g) override
    {
        if (useV2 && ensureArt())
        {
            for (int y = 0; y < LittleFootApp::H; ++y)
                for (int x = 0; x < LittleFootApp::W; ++x)
                    if (auto* shape = ledShapes[(size_t) (y * LittleFootApp::W + x)])
                        shape->setFill (juce::Colour (app.pixel (x, y)));

            auto r = getLocalBounds().toFloat().reduced (4.0f);
            const float side = juce::jmin (r.getWidth(), r.getHeight());
            r = r.withSizeKeepingCentre (side, side);
            art->drawWithin (g, r, juce::RectanglePlacement::stretchToFit, 1.0f);
            return;
        }

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

    // Find a sub-drawable by its SVG id (JUCE stores the id as the Drawable's componentID).
    static juce::Drawable* findById (juce::Drawable* d, juce::StringRef id)
    {
        if (d == nullptr) return nullptr;
        if (d->getComponentID() == id) return d;
        if (auto* comp = dynamic_cast<juce::DrawableComposite*> (d))
            for (int i = 0; i < comp->getNumChildComponents(); ++i)
                if (auto* child = dynamic_cast<juce::Drawable*> (comp->getChildComponent (i)))
                    if (auto* found = findById (child, id)) return found;
        return nullptr;
    }

    // Lazily parse lightpad-block.svg and cache a DrawableShape* for each led-y-x cell. Returns false
    // (once, cached) if the resource is missing/unparseable, so paint() falls back to the drawn grid.
    bool ensureArt()
    {
        if (artTried) return art != nullptr;
        artTried = true;

        for (int i = 0; i < DeviceArtData::namedResourceListSize; ++i)
            if (juce::String ("lightpad-block.svg") == DeviceArtData::originalFilenames[i])
            {
                int size = 0;
                if (const char* data = DeviceArtData::getNamedResource (DeviceArtData::namedResourceList[i], size))
                    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
                        art = juce::Drawable::createFromSVG (*xml);
                break;
            }

        if (art != nullptr)
            for (int y = 0; y < LittleFootApp::H; ++y)
                for (int x = 0; x < LittleFootApp::W; ++x)
                {
                    const auto id = juce::String ("led-") + juce::String (y) + "-" + juce::String (x);
                    ledShapes[(size_t) (y * LittleFootApp::W + x)]
                        = dynamic_cast<juce::DrawableShape*> (findById (art.get(), id));
                }

        return art != nullptr;
    }

    LittleFootApp& app;

    bool useV2 = false, artTried = false;
    std::unique_ptr<juce::Drawable> art;
    std::array<juce::DrawableShape*, (size_t) (LittleFootApp::W * LittleFootApp::H)> ledShapes { };

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
