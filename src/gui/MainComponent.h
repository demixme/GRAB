// GRAB — minimal JUCE GUI to read/write Seaboard Block config.
//
// Controls are generated from what the device reports (active config items only),
// not hard-coded. Edits stay local until "Save to device" is pressed, which writes
// the changed items via setConfig (confirmed in Phase 3 to persist to flash).

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "LoggingMidi.h"
#include "ConfigOverrides.h"
#include "LearnedNames.h"
#include "CurveComponent.h"

#include <map>
#include <vector>

//==============================================================================
/** One editable config item: label + the control appropriate to its type. */
class ConfigRow  : public juce::Component
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    ConfigRow (const roli::Block::ConfigMetaData& meta, juce::int32 value, ChangeFn onChange);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::uint32 getId()    const { return id; }
    juce::int32  getValue() const;
    void setValue (juce::int32 v);          // updates control without firing change
    void setChangedFromDevice (bool changed);

private:
    using ConfigType = roli::Block::ConfigMetaData::ConfigType;

    void fireChange();

    juce::uint32 id;
    ConfigType   type;
    juce::Range<juce::int32> range;

    juce::Label      nameLabel;
    juce::Slider     slider;     // integer
    juce::ToggleButton toggle;   // boolean
    juce::ComboBox   combo;      // options (combo index = value - range start)
    juce::Label      readOnly;   // colour / float — shown, not edited

    juce::Component* control = nullptr;
    ChangeFn change;
    bool changed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigRow)
};

//==============================================================================
class MainComponent  : public juce::Component,
                       private roli::TopologySource::Listener,
                       private roli::Block::ConfigItemListener,
                       private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void topologyChanged() override;
    void timerCallback() override;

    // Live config updates from the device (delivered on the message thread).
    void handleConfigItemChanged (roli::Block&, const roli::Block::ConfigMetaData&, juce::uint32 index) override;
    void handleConfigSyncEnded   (roli::Block&) override;
    void setListeningBlock (roli::Block::Ptr b);  // (un)register the config listener
    void applyLiveConfigChange (const roli::Block::ConfigMetaData& meta, juce::uint32 index);
    bool controlExistsFor (juce::uint32 id) const;

    void connect (seaprobe::Transport transport);
    void updateBlockList (const roli::BlockTopology& topology);
    void selectBlock (roli::Block::UID uid);
    void contribute();
    void reportConnection();
    void fetchNames (const juce::String& family, const juce::String& type);
    void onNamesFetched (juce::String type, grab::LearnedMap learned);
    void requestConfig();
    void rebuildRows();
    void saveToDevice();
    void reloadFromDevice();
    void loadPreset();
    void savePreset();
    void onItemChanged (juce::uint32 id, juce::int32 value); // from a row or a curve
    void updateSaveEnabled();
    void setStatus (const juce::String&);

    // Session
    std::unique_ptr<seaprobe::RawLogger>            logger;
    std::unique_ptr<seaprobe::LoggingDeviceDetector> detector;
    std::unique_ptr<roli::PhysicalTopologySource>    source;
    roli::Block::Ptr block;                 // currently selected block
    roli::Block::Ptr listeningBlock;        // block we've registered the config listener on
    roli::Block::UID selectedBlockUid = 0;  // kept across topology updates
    std::vector<roli::Block::UID> blockOrder; // combo index -> uid

    // Config sync bookkeeping
    bool   configRequested = false;
    double configRequestedAtMs = 0.0;
    bool   rowsBuilt = false;

    // Connect-timeout bookkeeping: detect a port that opens but never produces a
    // BLOCKS topology (e.g. a Seaboard RISE/GRAND — a different, unsupported protocol).
    double sourceActiveAtMs   = 0.0;
    bool   deviceSeen         = false;
    bool   connectTimeoutShown = false;
    juce::File logFolder;     // where the log is written, to point users at for reports
    void checkConnectTimeout();

    std::map<juce::uint32, juce::int32> deviceValues;  // last values read from device
    std::map<juce::uint32, juce::int32> pendingValues; // edited (unsaved) values, rows + curves

    grab::LearnedMap learnedNames;      // names pulled from the store for the current type
    juce::String     learnedForType;    // which device type learnedNames is for (avoid refetch)
    seaprobe::Transport requestedTransport = seaprobe::Transport::Auto; // for diagnostics

    // UI
    juce::Label        statusLabel;
    juce::Label        transportLabel;
    juce::ComboBox     transportBox;
    juce::Label        blockLabel;
    juce::ComboBox     blockBox;
    juce::TextButton   captureBtn    { "Capture..." };
    juce::TextButton   reportBtn     { "Report..." };
    juce::TextButton   contributeBtn { "Contribute..." };
    juce::TextButton   reloadButton  { "Reload from device" };
    juce::TextButton   probeBtn      { "Probe..." };
    juce::TextButton   saveButton    { "Save to device" };
    juce::TextButton   loadPresetBtn { "Load preset..." };
    juce::TextButton   savePresetBtn { "Save preset..." };
    juce::Viewport     viewport;
    juce::Component    rowHolder;
    std::vector<std::unique_ptr<ConfigRow>> rows;
    std::vector<std::unique_ptr<CurveRow>> curveRows;   // 5D curve panels
    std::vector<CurveComponent*> curves;                // non-owning, for id lookup
    std::vector<std::unique_ptr<juce::Label>> sectionHeaders;
    // Ordered list of what to stack in rowHolder: section headers and rows, each
    // with its height. Drives both rebuildRows() and resized().
    std::vector<std::pair<juce::Component*, int>> layoutOrder;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
