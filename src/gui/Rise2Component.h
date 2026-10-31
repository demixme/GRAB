// Rise2Component: settings page for the Seaboard RISE 2, which has no BLOCKS packed
// protocol. State comes from the identify reply (see Rise2Protocol.h), controls are the
// same ConfigRow / CurveComponent widgets the BLOCKS page uses. Edits stay local until
// "Save to device", which sends only the changed settings and then re-reads the state.

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "LoggingMidi.h"
#include "Rise2Protocol.h"
#include "ThrottledWriter.h"

#include <array>
#include <map>
#include <memory>
#include <set>
#include <vector>

class ConfigRow;
class CurveRow;
class CurveComponent;
class SectionCard;
class FaderPanel;
class Fader;

class Rise2Component  : public juce::Component,
                        private juce::MidiInputCallback,
                        private juce::Timer
{
public:
    /** preferredPortId: when several RISE/GRAND ports are present, open this one (by MIDI
        identifier) rather than the first found - lets GRAB's device switcher target a specific
        keyboard. Empty = open the first RISE/GRAND port, as before. */
    explicit Rise2Component (seaprobe::RawLogger& loggerToUse, juce::String preferredPortId = {});
    ~Rise2Component() override;

    /** True when a RISE MIDI port is present. */
    static bool isConnected();

    /** ROLI firmware-download product name for the connected device, or "" if none is known.
        Confirmed on the server (2026-10-27): RISE 2 -> "Seaboard RISE2" (2.x), RISE 25 ->
        "Seaboard RISE 25", original RISE 49 -> "Seaboard RISE" (1.1.8). The GRAND is registered
        but has no downloadable build, so it stays "". */
    juce::String firmwareDownloadProduct() const
    {
        if (grand) return {};
        if (firmware.startsWith ("2."))        return "Seaboard RISE2";   // RISE 2
        // Match the RISE 25 model token, not just any "25" (a serial can contain "25").
        if (portName.containsIgnoreCase ("rise 25") || portName.containsIgnoreCase ("rise25"))
            return "Seaboard RISE 25";                                     // original RISE 25 (its own build)
        if (firmware.startsWith ("1."))        return "Seaboard RISE";    // original RISE 49 -> 1.1.8
        return {};
    }

    /** Dev/preview: render the page from a synthetic identify reply, no device needed.
        verifiedFirmware=true fakes a RISE 2 (2.0.2, full settings shown); false fakes an
        original RISE (1.1.8, the "not verified" path). No real serial/MAC. */
    void previewSyntheticState (bool verifiedFirmware);

    /** Dev/preview: render the **GRAND** page (4D, no slide/strike, 3 pedals) from a synthetic
        reply. No device, no real values — layout preview only. */
    void previewSyntheticGrand();

    void resized() override;

    /** Live keyboard touch, synthesised from the device's performance MIDI and forwarded to the
        keyboard graphic. Set by MainComponent. Args: MIDI channel 1-16, note, pressure 0-1,
        pitch bend -1..1, strike 0-1 (note-on velocity, fixed for the note), isEnd. Called on the
        MIDI thread. */
    std::function<void (int, int, float, float, float, bool)> onTouch;

    /** The GRAND's reported key count (from its reply), so MainComponent can size the keyboard
        graphic for the Studio (37) / Stage (61) / First Edition (88). Called on the message thread. */
    std::function<void (int)> onKeyCount;

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    void openPorts();
    void closePorts();
    void devicesChanged();   // keyboard switched off / on, cable pulled
    void send (const rise2::Messages&);
    void requestState();
    void onState (rise2::State);
    void rebuildRows();
    void onItemChanged (juce::uint32 id, juce::int32 value);
    void flushLiveWrites();   // throttled: send edited settings to the device as you make them
    void applyRisePreset (const juce::String& name);   // apply a built-in ROLI preset (live-writes)
    void updateEnabled();
    void saveToDevice();
    void flashSaved();        // briefly show "Saved" on the button after a write lands
    void loadPreset();        // import a preset from a user-chosen .json file
    void savePreset();        // export the current settings to a user-chosen .json file
    void setStatus (const juce::String&);

    // Unified "Preset" menu: built-in ROLI presets + the user's own saved presets in one list.
    juce::File presetDir() const;                         // %APPDATA%/GRAB/presets/<family>
    void refreshPresetMenu();                             // (re)populate the combo from disk
    void saveUserPresetAs();                              // prompt a name, write to presetDir()
    void applyUserPreset (const juce::File&);             // load + apply one saved preset (live)
    bool fieldShown (juce::uint32 id) const;              // is this field on the current (RISE/GRAND) page
    int  applyPresetValues (const std::map<juce::uint32, juce::int32>&, bool live);

    int value (juce::uint32 id) const;   // pending value, falls back to the device value
    rise2::Messages writeFor (juce::uint32 id, juce::int32 v) const;  // grand-aware single-field write

    seaprobe::RawLogger& logger;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput>  midiIn;
    juce::MidiDeviceListConnection deviceList;
    juce::String portName;   // "Seaboard RISE 2", the product name the device does not report
    juce::String preferredPortId;  // identifier to prefer when several RISE/GRAND ports exist ("" = first)
    juce::String firmware;   // last-read firmware string, for preset metadata

    rise2::State state;
    // Latest value per MIDI CC (0..127), written on the MIDI thread from incoming performance
    // messages; the fader panel polls it to animate the touch faders / XY pad. -1 = none yet.
    std::array<int, 128> liveCc;
    std::array<int, 17>   touchNote;   // active note per MIDI channel (1-16), -1 = none
    std::array<float, 17> touchZ;      // last pressure per channel (Press)
    std::array<float, 17> touchBend;   // last pitch bend (-1..1) per channel (Glide)
    std::array<float, 17> touchStrike; // note-on velocity per channel (Strike), fixed for the note
    std::map<juce::uint32, juce::int32> deviceValues;
    std::map<juce::uint32, juce::int32> pendingValues;
    std::map<juce::uint32, juce::int32> expected;  // after a save, what the device should report
    // Live-write (Dashboard-style): ids edited interactively, flushed to the device on a
    // ~75 ms throttle. Preset-staged values aren't here — they still wait for an explicit Save.
    std::set<juce::uint32> dirtyWrites;
    ThrottledWriter        liveWriter;
    bool awaitingState = false;
    int  stateRetries = 0;
    bool minMaxReliable = true;   // false on the original RISE: min/max read back as zeros, so hidden
    bool grand = false;           // true: render the GRAND field set (4D, 3 pedals, no slide/strike/faders)

    juce::Label      statusLabel;
    juce::Label      infoLabel;
    juce::TextButton   reloadButton { "Reload from device" };
    juce::TextButton   saveButton   { "Save to device" };
    juce::ToggleButton autosaveToggle { "Autosave" };   // on: edits write live; off: manual Save
    bool               savedFlash = false;               // button is briefly showing "Saved" after a write
    int                savedFlashGen = 0;                 // only the newest flash's timer reverts (no mid-drag blink)
    juce::ComboBox   presetBox;   // unified: ROLI presets + the user's own saved presets
    std::vector<juce::File> userPresetFiles;   // menu index -> saved-preset file (parallel to the combo)
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Viewport   viewport;
    juce::Component  rowHolder;
    std::vector<std::unique_ptr<ConfigRow>> rows;
    std::unique_ptr<CurveRow> curveRow;
    std::vector<CurveComponent*> curves;
    std::vector<std::unique_ptr<SectionCard>> cards;    // one titled card per section
    std::unique_ptr<FaderPanel> faderPanel;             // the MIDI CC Assignments faders
    std::vector<Fader*> faders;                          // non-owning, for id lookup
    std::vector<std::unique_ptr<juce::Label>> sectionHeaders;  // (unused by the card layout)
    std::vector<std::pair<juce::Component*, int>> layoutOrder;  // (unused by the card layout)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rise2Component)
};
