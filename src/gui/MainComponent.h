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
#include "ColourWell.h"
#include "SectionCard.h"
#include "FooterBar.h"
#include "KeyboardView.h"
#include "Rise2Component.h"
#include "ThrottledWriter.h"
#include "CapturePanel.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

//==============================================================================
/** One editable config item: label + the control appropriate to its type. */
class ConfigRow  : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    ConfigRow (const roli::Block::ConfigMetaData& meta, juce::int32 value, ChangeFn onChange);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;        // right-click resets to the device value
    void mouseDoubleClick (const juce::MouseEvent&) override; // double-click resets too

    juce::uint32 getId()    const { return id; }
    juce::int32  getValue() const;
    void setValue (juce::int32 v);          // updates control without firing change
    void setChangedFromDevice (bool changed);

private:
    using ConfigType = roli::Block::ConfigMetaData::ConfigType;

    void fireChange();
    void resetToDevice();   // revert to the device value and write it (double-/right-click)

    juce::uint32 id;
    ConfigType   type;
    juce::Range<juce::int32> range;
    juce::int32  initialValue = 0;   // device value, for right-click reset

    juce::Label      nameLabel;
    juce::Slider     slider;     // integer
    juce::ToggleButton toggle;   // boolean
    juce::ComboBox   combo;      // options (combo index = value - range start)
    grab::ColourWell colourWell; // colour (ARGB)
    juce::Label      readOnly;   // float — shown, not edited

    juce::Component* control = nullptr;
    ChangeFn change;
    bool changed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigRow)
};

//==============================================================================
class MainComponent  : public juce::Component,
                       private roli::TopologySource::Listener,
                       private roli::Block::ConfigItemListener,
                       private roli::TouchSurface::Listener,
                       private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void topologyChanged() override;
    void timerCallback() override;
    void forceRisePreview();   // secret dev shortcut: show the RISE page with synthetic data
    void forcePianoPreview();  // secret dev shortcut: synthetic ROLI Piano config page (no hardware)
    void forceLumiPreview();   // secret dev shortcut: synthetic Piano M / LUMI Keys page (no hardware)
    void forceGrandPreview();  // secret dev shortcut: synthetic GRAND page (4D, 3 pedals) — no hardware
    void forceLightpadPreview(); // secret dev shortcut: Lightpad config + synthetic DNA arrangement (no hardware)

    // Live config updates from the device (delivered on the message thread).
    void handleConfigItemChanged (roli::Block&, const roli::Block::ConfigMetaData&, juce::uint32 index) override;
    void handleConfigSyncEnded   (roli::Block&) override;
    void setListeningBlock (roli::Block::Ptr b);  // (un)register the config listener
    void applyLiveConfigChange (const roli::Block::ConfigMetaData& meta, juce::uint32 index);
    bool controlExistsFor (juce::uint32 id) const;

    void connect (seaprobe::Transport transport);
    void updateBlockList (const roli::BlockTopology& topology);
    void selectBlock (roli::Block::UID uid);
    void selectBlockByUid (juce::uint64 clickedUid); // click a tile in the arrangement -> select its group
    void updateKeyboardForBlock();  // show/size the keywave for the current block (joined for a chain)
    void updateLumiKeyboardColours(); // LUMI/Piano: light the keyboard graphic per scale/key/colours
    void pushChordGhosts();           // Seaboard: feed the active Chord mode to the keyboard's touch ghosts
    std::vector<KeyboardView::Tile> buildBlockTiles(); // the connected DNA group's real X/Y arrangement

    // Live 5D-touch rings on the keyboard graphic. Registers on every cluster member's touch
    // surface; touchChanged (MIDI thread) maps a touch into the combined keyboard and forwards it.
    void setTouchListening (const std::vector<roli::Block::UID>& members);
    void touchChanged (roli::TouchSurface&, const roli::TouchSurface::Touch&) override;
    void contribute();
    void openDialog (juce::Component* panel, const juce::String& title); // owns panel in a DialogWindow
    void openCaptureWindow();   // Advanced ▸ Capture MIDI: the unified capture + decode tool
    void checkForUpdate();      // async: ask GitHub for the latest release, light up the footer if newer
    // Friendly name + formatted value for a config item id (applies overrides + learned names).
    // Passed to the capture tool so device→host config echoes read as "Pressure Sensitivity = 46".
    std::pair<juce::String, juce::String> describeConfig (juce::uint32 item, juce::int32 value) const;
    void showHelpWanted();   // the "how you can help" dialog (shown on every launch)
    void fetchNames (const juce::String& family, const juce::String& type);
    void onNamesFetched (juce::String type, grab::LearnedMap learned);
    void requestConfig();
    void rebuildRows();
    void saveToDevice();
    void reloadFromDevice();
    void factoryReset();   // Blocks only: library block->factoryReset(), behind a confirm dialog
    juce::String roliProductName() const;  // ROLI download product name for the connected device, or empty
    void downloadFirmware();               // open ROLI's firmware download page in the browser (archival; never flashes)
    void populatePresetBox();                 // (re)fill the unified Preset menu for the current device
    void applyBlockPreset (const juce::String& name);   // apply a built-in ROLI preset (live-writes)
    void loadPreset();        // import a preset from a user-chosen .json file
    void savePreset();        // export the current settings to a user-chosen .json file
    // Unified "Preset" menu: built-in ROLI presets + the user's own saved presets in one list.
    juce::File presetDir() const;                         // %APPDATA%/GRAB/presets/<device type>
    void saveUserPresetAs();                              // prompt a name, write to presetDir()
    void applyUserPreset (const juce::File&);             // load + apply one saved preset (live)
    int  applyPresetValues (const std::map<juce::uint32, juce::int32>&, bool live);
    void onItemChanged (juce::uint32 id, juce::int32 value); // from a row or a curve
    void flushLiveWrites();                                  // throttled: write edited items to the device
    std::vector<roli::Block::Ptr> writeTargets() const;      // the selected block + its DNA cluster
    void updateSaveEnabled();
    void flashSaved();        // briefly show "Saved" on the button after a write lands
    void setStatus (const juce::String&);
    void setBlocksControlsVisible (bool);

    // Session
    std::unique_ptr<seaprobe::RawLogger>            logger;
    std::unique_ptr<seaprobe::LoggingDeviceDetector> detector;
    std::unique_ptr<roli::PhysicalTopologySource>    source;
    roli::Block::Ptr block;                 // currently selected block
    roli::Block::Ptr listeningBlock;        // block we've registered the config listener on
    roli::Block::UID selectedBlockUid = 0;  // kept across topology updates
    std::vector<roli::Block::UID> blockOrder; // combo index -> representative (master) uid
    // A DNA-chained, same-type group shares one config (edit one, the chain mirrors it), so
    // it's shown as a single picker entry. Maps the representative uid -> all member uids,
    // left-to-right. A write fans out to every member. Lone blocks map to a 1-element list.
    std::map<roli::Block::UID, std::vector<roli::Block::UID>> clusterMembers;

    // Config sync bookkeeping
    bool   configRequested = false;
    double configRequestedAtMs = 0.0;
    bool   rowsBuilt = false;

    // Connect-timeout bookkeeping: detect a port that opens but never produces a BLOCKS topology
    // (a device GRAB doesn't model, or a held port; RISE/GRAND are excluded and use the RISE page).
    double sourceActiveAtMs   = 0.0;
    bool   deviceSeen         = false;
    bool   connectTimeoutShown = false;
    juce::File logFolder;     // where the log is written, to point users at for reports
    void checkConnectTimeout();

    std::map<juce::uint32, juce::int32> deviceValues;  // last values read from device
    std::map<juce::uint32, juce::int32> pendingValues; // edited values, rows + curves
    // Live-write: ids the user edited interactively, pending a throttled write to the device.
    // Preset-staged values are NOT in here — those still wait for an explicit Save.
    std::set<juce::uint32> dirtyWrites;
    ThrottledWriter        liveWriter;

    // Dev preview (no hardware): when simConfig is non-empty, rebuildRows() builds the page
    // from it instead of a real block. grandPreview shows the identity-only GRAND state.
    std::vector<std::pair<roli::Block::ConfigMetaData, juce::int32>> simConfig;
    bool grandPreview = false;
    juce::String lastDnaSig;   // signature of the stitched DNA links; re-layout when it changes

    grab::LearnedMap learnedNames;      // names pulled from the store for the current type
    juce::String     learnedForType;    // which device type learnedNames is for (avoid refetch)
    seaprobe::Transport requestedTransport = seaprobe::Transport::Auto; // for diagnostics

    // UI
    juce::Label        statusLabel;
    juce::Label        transportLabel;
    juce::ComboBox     transportBox;
    juce::Label        blockLabel;
    juce::ComboBox     blockBox;
    juce::TextButton   advancedBtn   { "Advanced..." };
    juce::TextButton   contributeBtn { "Contribute..." };
    juce::TextButton   reloadButton  { "Reload from device" };
    juce::TextButton   diagnoseBtn   { "Diagnose..." };
    juce::TextButton   saveButton    { "Save to device" };
    juce::ToggleButton autosaveToggle { "Autosave" };   // on: edits write live; off: manual Save
    bool               savedFlash = false;              // button briefly showing "Saved" after a write
    int                savedFlashGen = 0;               // only the newest flash's timer reverts (no mid-drag blink)
    juce::ComboBox     presetBox;   // unified: ROLI presets + the user's own saved presets
    std::vector<juce::File> userPresetFiles;   // menu index -> saved-preset file (parallel to the combo)
    juce::Viewport     viewport;
    juce::Component    rowHolder;
    std::vector<std::unique_ptr<ConfigRow>> rows;
    std::vector<std::unique_ptr<CurveRow>> curveRows;   // 5D curve panels
    std::vector<CurveComponent*> curves;                // non-owning, for id lookup
    std::vector<std::unique_ptr<SectionCard>> cards;    // one titled card per section
    std::vector<std::unique_ptr<juce::Label>> sectionHeaders;  // (unused by the card layout)
    // Ordered list of what to stack in rowHolder: section headers and rows, each
    // with its height. Drives both rebuildRows() and resized().
    std::vector<std::pair<juce::Component*, int>> layoutOrder;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> captureWindow;  // non-modal, so GRAB stays usable while capturing

    std::unique_ptr<Rise2Component> rise2Page;  // shown instead of the BLOCKS page for a RISE
    bool risePreviewVerified = true;            // toggles verified/unverified on the dev preview shortcut

    FooterBar footer;                           // hover-help strip along the bottom
    KeyboardView keyboardView;                  // stylised keywave at the top (keyboard devices)
    juce::Label  busyLabel;                     // centred overlay shown while the device reboots
    double resettingSinceMs = 0.0;              // >0 while a factory reset's reboot is expected
    juce::MidiDeviceListConnection deviceList;  // to pick up a RISE switched on after launch

    // Touch-surface listening for the live rings. Hold a Block::Ptr to each listened block so its
    // touch surface stays alive until we removeListener, even if the block leaves the topology.
    std::vector<roli::Block::Ptr> listenedBlocks;
    struct TouchGeom { int index; int count; float w, h; }; // block's place in the chain + its size
    std::map<roli::Block::UID, TouchGeom> touchGeometry;
    juce::CriticalSection touchGeomLock;
    // Strike per live touch id: the device's initial contact velocity (zVelocity on the touch-start,
    // already 0..1 - what ROLI's own stack treats as strike), captured once and held for the note.
    std::map<juce::uint64, float> blockTouchStrike;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
