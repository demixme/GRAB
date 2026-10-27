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
    explicit Rise2Component (seaprobe::RawLogger& loggerToUse);
    ~Rise2Component() override;

    /** True when a RISE MIDI port is present. */
    static bool isConnected();

    /** Dev/preview: render the page from a synthetic identify reply, no device needed.
        verifiedFirmware=true fakes a RISE 2 (2.0.2, full settings shown); false fakes an
        original RISE (1.1.8, the "not verified" path). No real serial/MAC. */
    void previewSyntheticState (bool verifiedFirmware);

    void resized() override;

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
    void applyRisePreset (const juce::String& name);   // apply a built-in ROLI RISE preset (live-writes)
    void updateEnabled();
    void saveToDevice();
    void loadPreset();
    void savePreset();
    void setStatus (const juce::String&);

    int value (juce::uint32 id) const;   // pending value, falls back to the device value

    seaprobe::RawLogger& logger;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput>  midiIn;
    juce::MidiDeviceListConnection deviceList;
    juce::String portName;   // "Seaboard RISE 2", the product name the device does not report
    juce::String firmware;   // last-read firmware string, for preset metadata

    rise2::State state;
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

    juce::Label      statusLabel;
    juce::Label      infoLabel;
    juce::TextButton reloadButton { "Reload from device" };
    juce::TextButton saveButton   { "Save to device" };
    juce::TextButton loadPresetBtn { "Load preset..." };
    juce::TextButton savePresetBtn { "Save preset..." };
    juce::ComboBox   presetBox;   // built-in ROLI RISE presets (MPE/Multi/Single/Piano)
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
