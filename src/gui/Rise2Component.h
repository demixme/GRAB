// Rise2Component: settings page for the Seaboard RISE 2, which has no BLOCKS packed
// protocol. State comes from the identify reply (see Rise2Protocol.h), controls are the
// same ConfigRow / CurveComponent widgets the BLOCKS page uses. Edits stay local until
// "Save to device", which sends only the changed settings and then re-reads the state.

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "LoggingMidi.h"
#include "Rise2Protocol.h"

#include <map>
#include <memory>
#include <vector>

class ConfigRow;
class CurveRow;
class CurveComponent;

class Rise2Component  : public juce::Component,
                        private juce::MidiInputCallback,
                        private juce::Timer
{
public:
    explicit Rise2Component (seaprobe::RawLogger& loggerToUse);
    ~Rise2Component() override;

    /** True when a RISE MIDI port is present. */
    static bool isConnected();

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
    void updateEnabled();
    void saveToDevice();
    void setStatus (const juce::String&);

    int value (juce::uint32 id) const;   // pending value, falls back to the device value

    seaprobe::RawLogger& logger;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput>  midiIn;
    juce::MidiDeviceListConnection deviceList;
    juce::String portName;   // "Seaboard RISE 2", the product name the device does not report

    rise2::State state;
    std::map<juce::uint32, juce::int32> deviceValues;
    std::map<juce::uint32, juce::int32> pendingValues;
    std::map<juce::uint32, juce::int32> expected;  // after a save, what the device should report
    bool awaitingState = false;
    int  stateRetries = 0;

    juce::Label      statusLabel;
    juce::Label      infoLabel;
    juce::TextButton reloadButton { "Reload from device" };
    juce::TextButton saveButton   { "Save to device" };
    juce::Viewport   viewport;
    juce::Component  rowHolder;
    std::vector<std::unique_ptr<ConfigRow>> rows;
    std::unique_ptr<CurveRow> curveRow;
    std::vector<CurveComponent*> curves;
    std::vector<std::unique_ptr<juce::Label>> sectionHeaders;
    std::vector<std::pair<juce::Component*, int>> layoutOrder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rise2Component)
};
