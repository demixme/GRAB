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
#include "SensitivityGraph.h"
#include "SegmentedControl.h"
#include "Stepper.h"
#include "Changeable.h"
#include "DeviceMenu.h"
#include "DeviceNameButton.h"
#include "ColourWell.h"
#include "DeviceInfoBar.h"
#include "SectionCard.h"
#include "FooterBar.h"
#include "KeyboardView.h"
#include "LiveKeysStrip.h"
#include "ViewToggle.h"
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
                   public juce::SettableTooltipClient,
                   public grab::Changeable
{
public:
    using ChangeFn = std::function<void (juce::uint32 id, juce::int32 value)>;

    // buttonConfig: this item is a Control Block packed button-data int (CC + mode + min/max/value).
    // The row then shows a CC slider + a mode dropdown, unpacking on setValue and repacking on change,
    // so the stored value stays the packed int the device expects.
    ConfigRow (const roli::Block::ConfigMetaData& meta, juce::int32 value, ChangeFn onChange,
               bool buttonConfig = false);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;        // right-click resets to the device value
    void mouseDoubleClick (const juce::MouseEvent&) override; // double-click resets too

    juce::uint32 getId()    const { return id; }
    juce::int32  getValue() const;
    void setValue (juce::int32 v);          // updates control without firing change
    void setChangedFromDevice (bool changed);
    int  changedCount() const override { return getValue() != initialValue ? 1 : 0; }

private:
    using ConfigType = roli::Block::ConfigMetaData::ConfigType;

    void fireChange();
    void resetToDevice();   // revert to the device value and write it (double-/right-click)
    void refreshChangedLook();   // label tone (ink2 -> ink when changed) + the changed dot

    juce::uint32 id;
    ConfigType   type;
    juce::Range<juce::int32> range;
    juce::int32  initialValue = 0;   // device value, for right-click reset

    // Control Block packed button-data (ids 66-75): CC in bits 3-9, mode in bits 0-2, with value/min/
    // max in the high bits (preserved on edit). Shared layout used by the row to split/rebuild it.
    bool buttonCfg = false;
    static juce::int32 buttonVal  (juce::int32 p) { return (p >> 24) & 0x7F; }   // Default Value
    static juce::int32 buttonMax  (juce::int32 p) { return (p >> 17) & 0x7F; }
    static juce::int32 buttonMin  (juce::int32 p) { return (p >> 10) & 0x7F; }
    static juce::int32 buttonCC   (juce::int32 p) { return (p >>  3) & 0x7F; }
    static juce::int32 buttonMode (juce::int32 p) { return p & 0x7; }
    static juce::int32 packButton (juce::int32 val, juce::int32 mx, juce::int32 mn, juce::int32 cc, juce::int32 mode)
    { return ((val & 0x7F) << 24) | ((mx & 0x7F) << 17) | ((mn & 0x7F) << 10) | ((cc & 0x7F) << 3) | (mode & 0x7); }
    juce::ComboBox   modeCombo;                    // control-block button mode (toggle/pulse/gate/...)
    juce::Slider     defSlider, minSlider, maxSlider;   // Default / Min / Max value steppers
    juce::Label      ccCap, modeCap, defCap, minCap, maxCap;   // tiny captions under each field

    juce::Label      nameLabel;
    juce::Slider     slider;     // integer
    juce::ToggleButton toggle;   // boolean
    juce::ComboBox   combo;      // options (combo index = value - range start): long/many-option sets
    grab::SegmentedControl segmented;   // options with a few short labels (MIDI mode, zone, ...)
    bool             useSegmented = false;
    grab::Stepper    intStepper;        // small integer fields as Fred's -/+ stepper
    bool             useStepper = false;
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
    void forceControlPreview();  // secret dev shortcut: Control Block config page - cycles Loop/Live/Touch, then off
    int  controlPreviewStep = -1; // -1 = not showing; 0 Loop, 1 Live, 2 Touch (Ctrl+Alt+Shift+C cycles)

    // Live config updates from the device (delivered on the message thread).
    void handleConfigItemChanged (roli::Block&, const roli::Block::ConfigMetaData&, juce::uint32 index) override;
    void handleConfigSyncEnded   (roli::Block&) override;
    void setListeningBlock (roli::Block::Ptr b);  // (un)register the config listener
    void applyLiveConfigChange (const roli::Block::ConfigMetaData& meta, juce::uint32 index);
    bool controlExistsFor (juce::uint32 id) const;

    void connect (seaprobe::Transport transport);
    void updateBlockList (const roli::BlockTopology& topology);
    void selectBlock (roli::Block::UID uid);

    // Unified "ROLI device:" selector (blockBox). It lists every connected ROLI device so the user
    // can switch between them live, with no restart: each BLOCKS cluster (in BLOCKS mode), every
    // RISE/GRAND port, and - while a RISE page is shown - a "Seaboard Block(s)" entry to switch back.
    // Selecting an entry re-binds GRAB to that device (connect() for a cross-type switch, selectBlock
    // within BLOCKS). One device => the selector hides itself (single-device users see no change).
    struct DeviceEntry
    {
        enum class Kind { blocksCluster, risePort, blocksSwitch };
        Kind kind = Kind::blocksCluster;
        roli::Block::UID repUid = 0;          // blocksCluster: the cluster's representative block
        juce::String identifier, portName;    // risePort: the RISE/GRAND output port
        bool grand = false;                    // risePort
        juce::String label;                    // what the combo shows
    };
    std::vector<DeviceEntry> deviceEntries;    // parallel to blockBox items (combo id = index + 1)
    void onDeviceBoxChanged();                 // blockBox.onChange: switch to the picked device
    void appendRiseDeviceEntries();            // add every RISE/GRAND port to deviceEntries
    void rebuildRiseDeviceEntries();           // RISE mode: (re)build deviceEntries + fill the box
    void fillDeviceBox (int selectId);         // mirror deviceEntries into blockBox, set visibility
    void refreshDeviceBox();                   // re-run the right rebuild for the current mode
    bool anyBlocksPortPresent() const;         // a non-RISE ROLI (Block/Lightpad/Piano) port is present
    // How connect() should choose when several devices are present (set by the device switcher).
    enum class Prefer { autoPick, blocks, rise };
    Prefer       preferredKind = Prefer::autoPick;
    juce::String preferredRiseId;   // identifier of the RISE/GRAND port to open (when preferring rise)
    juce::String currentRiseId;     // identifier of the RISE/GRAND port currently shown ("" if none)
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
    // Experimental BLOCKS app loader (Advanced, gated). appLoadTargetName() returns the selected
    // Lightpad's name if one is connected (apps target Lightpads), else "" so the UI disables loading.
    // loadAppToDevice() uploads a Littlefoot app TEMPORARILY (ProgramPersistency::setAsTemp - a
    // power-cycle restores the device). Apps only; GRAB never uploads firmware/bootloader images.
    juce::String appLoadTargetName() const;
    juce::Result loadAppToDevice (const juce::String& appSource, const juce::Array<juce::File>& searchPaths);
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
    std::map<int, SensitivityGraph::LiveInput> liveInputs;  // per-finger input, plotted live on the 5D graph
    bool liftObserved = false;   // we've seen at least one release on this device
    bool liftReported = false;   // ...and at least one carried a real release velocity (device has lift)
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
    juce::Label        statusLabel;      // transient messages only (Scanning / Reading / Saved); auto-clears
    DeviceInfoBar      deviceInfo;       // persistent device state (battery icon+% / name / fw / transport)
    int                statusGen = 0;    // generation guard so only the latest auto-clear fires
    void setTransient (const juce::String& s, bool autoClear = false);  // status that can self-clear
    void updateDeviceInfo();             // (re)fill the shared bar from the connected BLOCKS device
    juce::Label        transportLabel;
    juce::ComboBox     transportBox;
    juce::Label        blockLabel;
    juce::ComboBox     blockBox;        // selection model behind the pill (kept; drives onDeviceBoxChanged)
    grab::DeviceNameButton deviceMenuBtn;   // dashboard heading: the device name + chevron, opens the rich menu
    void showDeviceMenu();              // open Fred's DeviceMenu callout (devices + connection type)
    juce::Array<grab::DeviceMenu::Item> buildDeviceMenuItems() const;
    void updateLiveStripRange();        // feed the Live-keys strip the key span + octave/transpose shift
    juce::TextButton   advancedBtn   { "Advanced..." };
    juce::TextButton   contributeBtn { "Contribute..." };
    juce::TextButton   toolsBtn      { "Tools..." };    // bento: one menu for Diagnose / Advanced / Contribute
    // Top-row bento tiles (BLOCKS page): device drawing | dashboard | controls. Painted by paint(),
    // positioned by resized(); empty when the three-tile band isn't in use (RISE page / no device art).
    juce::Rectangle<int> drawTileR, dashTileR, ctrlTileR;
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
    std::unique_ptr<SensitivityGraph> sensGraph;        // the 5D overlay graph + inspector
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
    LiveKeysStrip    liveStrip;                 // Live keys: a big keyboard strip over the settings area
    juce::Label      liveKeysLabel;             // the "Live keys" heading above the strip
    juce::TextButton backToSettingsBtn { "Back to settings" };
    SectionCard* sensitivityCard = nullptr;     // the 5D card (kept visible above Live keys); owned by `cards`
    grab::ViewToggle viewToggle;                // drawing-tile switch: Device drawing | Keystroke map
    LiveKeysStrip    keystrokeMap;              // compact keystroke map shown in the drawing tile (toggle=map)
    bool drawingIsMap = false;                  // drawing tile shows the keystroke map (vs the device art)
    void setDrawingView (bool map);             // toggle the drawing-tile content (device art <-> keystroke map)
    void layoutDrawingTile (juce::Rectangle<int> tile);   // fit the art + reserve the view-toggle's column
    void updateSensitivityLayout();   // narrow fold: inspector stacks below the graph, card grows
    bool liveKeys = false;                      // Live keys view active (big strip below the sensitivity graph)
    void enterLiveKeys (bool on);               // toggle the Live-keys view (keeps the top band visible)
    bool deviceArtV2 = true;                    // Fred's SVG device-art kit (default); Advanced can toggle back
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
