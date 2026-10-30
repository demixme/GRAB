#include "MainComponent.h"
#include "ContributePanel.h"
#include "CapturePanel.h"
#include "ProbePanel.h"
#include "ExplorePanel.h"
#include "WelcomePanel.h"
#include "DiagnosePanel.h"
#include "AirwavePanel.h"
#include "AirwaveTesterPanel.h"
#include "Help.h"

#include <algorithm>
#include <map>
#include <thread>

#ifndef GRAB_VERSION
 #define GRAB_VERSION "0.0.0-dev"
#endif

namespace
{
// Item ids for the unified Preset combo. Factory (ROLI) presets are 1..N; the user's saved presets
// start at kUserPresetBase (index = id - kUserPresetBase); the trailing actions use high ids.
enum PresetMenuId
{
    kUserPresetBase = 1000,
    kPresetSaveAs   = 90001,
    kPresetImport   = 90002,
    kPresetExport   = 90003,
    kPresetReveal   = 90004
};

// Rough key-range (octaves) for the keyboard graphic, from the device's port/name.
int keyboardOctavesForName (const juce::String& name)
{
    if (name.containsIgnoreCase ("grand"))                                        // Seaboard GRAND
        return name.contains ("88") ? 7 : name.contains ("61") ? 5 : 5;          // GRAND 88 / 61 / 62
    if (name.contains ("25")) return 2;                                           // RISE 25
    if (name.contains ("49")) return 4;                                           // RISE 49 / Piano 49
    if (name.containsIgnoreCase ("RISE 2") || name.containsIgnoreCase ("RISE2"))  // RISE 2 (61 keys)
        return 5;
    return 2;                                                                     // Seaboard Block / original RISE / default
}
}

//==============================================================================
ConfigRow::ConfigRow (const roli::Block::ConfigMetaData& meta, juce::int32 value, ChangeFn onChange)
    : id (meta.item), type (meta.type), range (meta.range), initialValue (value), change (std::move (onChange))
{
    nameLabel.setText (meta.name, juce::dontSendNotification);
    addAndMakeVisible (nameLabel);

    switch (type)
    {
        case ConfigType::boolean:
            toggle.setToggleState (value != 0, juce::dontSendNotification);
            toggle.onClick = [this] { fireChange(); };
            control = &toggle;
            break;

        case ConfigType::options:
            // Only offer options that map to an in-range value. optionNames[i] maps to
            // value range.start + i, so valid i is 0..range.getLength(). Some items
            // (e.g. Glide/Slide Tracking: range 1..4) carry a trailing name ("Disabled")
            // beyond max; showing it would let the user write an out-of-range value.
            for (int i = 0; i <= range.getLength() && i < roli::Block::ConfigMetaData::numOptionNames; ++i)
                if (meta.optionNames[i].isNotEmpty())
                    combo.addItem (meta.optionNames[i], i + 1); // itemId = optionIndex + 1
            combo.setSelectedId ((value - range.getStart()) + 1, juce::dontSendNotification);
            combo.onChange = [this] { fireChange(); };
            control = &combo;
            break;

        case ConfigType::integer:
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
            slider.setRange ((double) range.getStart(), (double) range.getEnd(), 1.0);
            slider.setValue ((double) value, juce::dontSendNotification);
            slider.setDoubleClickReturnValue (true, (double) value);  // double-click reverts to the device value
            slider.onValueChange = [this] { fireChange(); };
            control = &slider;
            break;

        case ConfigType::colour:
            colourWell.setSwatchColour (juce::Colour ((juce::uint32) value));
            colourWell.onColourChanged = [this] (juce::Colour) { fireChange(); };
            control = &colourWell;
            break;

        case ConfigType::floating:
        default:
            readOnly.setText (juce::String (value) + "  (read-only)", juce::dontSendNotification);
            readOnly.setColour (juce::Label::textColourId, juce::Colours::grey);
            control = &readOnly;
            break;
    }

    addAndMakeVisible (control);

    // Hover help (shown in the footer bar) + right-click reset.
    const auto help = grab::helpForParameter (meta.name);
    setTooltip (help);
    nameLabel.setTooltip (help);
    if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (control))
        tc->setTooltip (help);
    if (control != nullptr)
        control->addMouseListener (this, true);
}

void ConfigRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())   // right-click resets to the device value (no ctrl-click)
        resetToDevice();
}

void ConfigRow::mouseDoubleClick (const juce::MouseEvent&)
{
    resetToDevice();   // double-click also resets
}

void ConfigRow::resetToDevice()
{
    setValue (initialValue);
    fireChange();
}

void ConfigRow::fireChange()
{
    if (change != nullptr)
        change (id, getValue());
}

juce::int32 ConfigRow::getValue() const
{
    switch (type)
    {
        case ConfigType::boolean: return toggle.getToggleState() ? 1 : 0;
        case ConfigType::options: return range.getStart() + (combo.getSelectedId() - 1);
        case ConfigType::integer: return (juce::int32) juce::roundToInt (slider.getValue());
        case ConfigType::colour:  return (juce::int32) colourWell.getSwatchColour().getARGB();
        default:                  return readOnly.getText().getIntValue();
    }
}

void ConfigRow::setValue (juce::int32 v)
{
    switch (type)
    {
        case ConfigType::boolean: toggle.setToggleState (v != 0, juce::dontSendNotification); break;
        case ConfigType::options: combo.setSelectedId ((v - range.getStart()) + 1, juce::dontSendNotification); break;
        case ConfigType::integer: slider.setValue ((double) v, juce::dontSendNotification); break;
        case ConfigType::colour:  colourWell.setSwatchColour (juce::Colour ((juce::uint32) v)); break;
        default:                  readOnly.setText (juce::String (v) + "  (read-only)", juce::dontSendNotification); break;
    }
}

void ConfigRow::setChangedFromDevice (bool c)
{
    if (changed != c) { changed = c; repaint(); }
}

void ConfigRow::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    const int labelW = juce::jlimit (90, 200, (int) (r.getWidth() * 0.46f));
    nameLabel.setBounds (r.removeFromLeft (labelW));
    if (control != nullptr)
        control->setBounds (r);
}

void ConfigRow::paint (juce::Graphics& g)
{
    // A small accent dot on the left marks a value you've changed from what the device reported.
    // It persists (unlike the old orange flash, which cleared the instant a live-write landed) and
    // clears on Reload from device or a right-click / double-click reset.
    if (getValue() != initialValue)
    {
        const float d = 5.0f;
        g.setColour (grab::theme::accent);
        g.fillEllipse (2.0f, ((float) getHeight() - d) * 0.5f, d, d);
    }
}

//==============================================================================
static constexpr int rowHeight = 30;
static constexpr int sectionHeaderHeight = 30;

// Order config sections roughly like ROLI Dashboard (Sensitivity / 5D first).
static int groupRank (const juce::String& g)
{
    static const char* order[] = {
        "Sensitivity", "MIDI Settings", "Pitch", "Play mode",
        "Pedal", "Colors", "Rhythm", "Mode 1", "Mode 2", "Mode 3", "Mode 4"
    };
    for (int i = 0; i < (int) (sizeof (order) / sizeof (order[0])); ++i)
        if (g == order[i])
            return i;
    return g.isEmpty() ? 100 : 50; // unknown groups after, empty/"Other" last
}

MainComponent::MainComponent()
{
    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (diagnoseBtn);
    diagnoseBtn.onClick = [this] { openDialog (new DiagnosePanel(), "Diagnose connection"); };
    addAndMakeVisible (reloadButton);
    addAndMakeVisible (saveButton);
    reloadButton.onClick  = [this] { reloadFromDevice(); };
    saveButton.onClick    = [this] { saveToDevice(); };
    liveWriter.onTick     = [this] { flushLiveWrites(); };

    // One "Preset" menu: ROLI's built-in presets (Seaboard) and the user's own saved presets in
    // one list, plus Save / Import / Export - the way ROLI Dashboard presents presets.
    addChildComponent (presetBox);   // shown with the other Block controls (setBlocksControlsVisible)
    presetBox.setTextWhenNothingSelected ("Preset...");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0)
            return;

        if (id >= 1 && id < kUserPresetBase)
            applyBlockPreset (presetBox.getText());
        else if (id >= kUserPresetBase && id < kPresetSaveAs)
        {
            const size_t idx = (size_t) (id - kUserPresetBase);
            if (idx < userPresetFiles.size())
                applyUserPreset (userPresetFiles[idx]);
        }
        else if (id == kPresetSaveAs) saveUserPresetAs();
        else if (id == kPresetImport) loadPreset();
        else if (id == kPresetExport) savePreset();
        else if (id == kPresetReveal) { auto d = presetDir(); d.createDirectory(); d.revealToUser(); }

        presetBox.setSelectedId (0, juce::dontSendNotification);   // back to the placeholder
    };
    saveButton.setEnabled (false);

    addAndMakeVisible (transportLabel);
    transportLabel.setText ("Connection Type:", juce::dontSendNotification);
    transportLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (transportBox);
    transportBox.addItem ("Auto", 1);
    transportBox.addItem ("USB",  2);
    transportBox.addItem ("BLE",  3);
    transportBox.setSelectedId (1, juce::dontSendNotification);
    transportBox.onChange = [this]
    {
        const auto t = transportBox.getSelectedId() == 2 ? seaprobe::Transport::Usb
                     : transportBox.getSelectedId() == 3 ? seaprobe::Transport::Ble
                                                         : seaprobe::Transport::Auto;
        connect (t);
    };

    addAndMakeVisible (blockLabel);
    blockLabel.setText ("ROLI device:", juce::dontSendNotification);
    blockLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (blockBox);
    blockBox.onChange = [this]
    {
        const int idx = blockBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) blockOrder.size())
            selectBlock (blockOrder[(size_t) idx]);
    };

    addAndMakeVisible (contributeBtn);
    contributeBtn.onClick = [this] { contribute(); };
    addAndMakeVisible (advancedBtn);
    advancedBtn.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (6, "Diagnose connection...");
        m.addItem (1, "Capture MIDI...");            // capture + decode: config changes, keepalives, notes, topology
        m.addItem (8, "Help add device (auto-explore)...");  // one-click device->host probe + submit
        m.addItem (2, "Active probe...");
        m.addItem (5, "Download firmware for archiving...");   // always enabled; explains if unmapped
        m.addSeparator();
        m.addItem (3, "Factory reset (presets only)...", block != nullptr);  // Blocks only; confirm dialog follows
        m.addSeparator();
        juce::PopupMenu airwave;                               // documentation/research; talks to no ROLI device
        airwave.addItem (9,  "About Airwave (documentation)...");
        airwave.addItem (10, "Airwave CC tester (emulator)...");   // sends CC21-31 for DAW mapping, no hardware
        m.addSubMenu ("Airwave (experimental)", airwave);
        m.addItem (4, "How you can help...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (advancedBtn),
            [this] (int r)
            {
                if      (r == 1) openCaptureWindow();
                else if (r == 2) openDialog (new ProbePanel(),  "Active probe");
                else if (r == 3) factoryReset();
                else if (r == 4) showHelpWanted();
                else if (r == 5) downloadFirmware();
                else if (r == 6) openDialog (new DiagnosePanel(), "Diagnose connection");
                else if (r == 8) openDialog (new ExplorePanel(), "Help add this device");
                else if (r == 9) openDialog (new AirwavePanel(), "Airwave & gesture control (experimental)");
                else if (r == 10) openDialog (new AirwaveTesterPanel(), "Airwave CC tester (experimental)");
            });
    };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (footer);
    footer.setVersion (GRAB_VERSION);
    checkForUpdate();   // ask GitHub for the latest release; lights up the footer if newer
    addChildComponent (keyboardView);   // shown only for keyboard devices
    keyboardView.onTileClicked = [this] (juce::uint64 uid) { selectBlockByUid (uid); }; // Dashboard-style click-to-select

    addChildComponent (busyLabel);      // shown only while a factory reset reboots the device
    busyLabel.setJustificationType (juce::Justification::centred);
    busyLabel.setColour (juce::Label::textColourId, grab::theme::textSecondary);
    busyLabel.setFont (juce::Font (juce::FontOptions (18.0f)));

    setSize (920, 910);   // tall enough for the RISE page + the keyboard graphic
    setWantsKeyboardFocus (true);   // so the secret RISE-preview shortcut reaches keyPressed
    // Grab focus once the window is on screen, so the shortcut works without clicking first.
    juce::Component::SafePointer<MainComponent> sp (this);
    juce::MessageManager::callAsync ([sp] { if (sp != nullptr) { sp->grabKeyboardFocus(); sp->showHelpWanted(); } });

    // Logs go to a stable per-user dir (same base as settings.properties), not the
    // current working directory — a Finder/Explorer-launched app has an unpredictable
    // CWD (e.g. "/" on macOS) where it can't write. Windows: %APPDATA%\GRAB\logs;
    // macOS: ~/Library/Application Support/GRAB/logs; Linux: ~/.config/GRAB/logs.
    const auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             .getChildFile ("GRAB")
                             .getChildFile ("logs")
                             .getChildFile ("GRAB-"
                                 + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".log");
    logger = std::make_unique<seaprobe::RawLogger> (logFile);
    logger->line ("GRAB log: " + logFile.getFullPathName());
    logFolder = logFile.getParentDirectory();

    connect (seaprobe::Transport::Auto);

    // Re-scan when the MIDI device list changes. Previously this only picked up a RISE; now,
    // when nothing is connected, any ROLI device appearing (plugged in, paired, or freed by
    // another app) triggers a (re-)connect using the current Connection Type - so "Auto" keeps
    // watching instead of giving up after launch. A live session or dev preview is left alone.
    deviceList = juce::MidiDeviceListConnection::make ([this]
    {
        // Already connected: leave the live session alone UNLESS a port we're using has vanished
        // (the device was unplugged or powered off). The stock library can be slow to drop the
        // block then, leaving a stale keybed on screen, so reconnect to rebuild the topology - that
        // clears the gone device. (Physically separating two BLE Seaboards does NOT drop a port, so
        // this never fires for a DNA separate - that's handled by the link tracker.)
        if (source != nullptr)
        {
            if (detector != nullptr)
            {
                const auto avail = juce::MidiInput::getAvailableDevices();
                for (auto& p : detector->getSelectedPairs())
                {
                    bool present = false;
                    for (auto& a : avail) if (a.identifier == p.input.identifier) { present = true; break; }
                    if (! present) { connect (requestedTransport); return; }
                }
            }
            return;
        }

        // Otherwise only (re-)scan when fully idle.
        if (block != nullptr || rise2Page != nullptr || ! simConfig.empty() || grandPreview)
            return;
        auto looksRoli = [] (const juce::MidiDeviceInfo& d)
        {
            return d.identifier.containsIgnoreCase ("vid_2af4")
                || d.name.containsIgnoreCase ("roli")     || d.name.containsIgnoreCase ("seaboard")
                || d.name.containsIgnoreCase ("rise")     || d.name.containsIgnoreCase ("grand")
                || d.name.containsIgnoreCase ("piano")    || d.name.containsIgnoreCase ("lumi")
                || d.name.containsIgnoreCase ("lightpad") || d.name.containsIgnoreCase ("airwave")
                || d.name.containsIgnoreCase ("block")
                || d.identifier.containsIgnoreCase ("seaboardblock") || d.identifier.containsIgnoreCase ("pianofortynine");
        };
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (looksRoli (d)) { connect (requestedTransport); return; }
    });
}

MainComponent::~MainComponent()
{
    stopTimer();
    liveWriter.cancel();
    setTouchListening ({});   // remove touch listeners while the blocks are still alive
    setListeningBlock (nullptr);
    if (source != nullptr)
    {
        source->removeListener (this);
        source->setActive (false);
    }
}

void MainComponent::connect (seaprobe::Transport transport)
{
    requestedTransport = transport;
    simConfig.clear();        // leave any dev preview
    grandPreview = false;

    // Tear down any existing session.
    stopTimer();
    liveWriter.cancel();
    dirtyWrites.clear();
    setListeningBlock (nullptr);
    setTouchListening ({});   // drop touch listeners before the source (and its blocks) go away
    if (source != nullptr)
    {
        source->removeListener (this);
        source->setActive (false);
        source.reset();
    }
    detector.reset();
    rise2Page.reset();
    setBlocksControlsVisible (true);
    keyboardView.setVisible (false);   // re-shown below for a keyboard device
    block = nullptr;
    selectedBlockUid = 0;
    learnedForType = {};
    learnedNames.clear();
    blockOrder.clear();
    blockBox.clear (juce::dontSendNotification);
    rows.clear();
    curveRows.clear();
    curves.clear();
    sectionHeaders.clear();
    layoutOrder.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();
    dirtyWrites.clear();
    liveWriter.cancel();
    configRequested = false;
    rowsBuilt = false;
    deviceSeen = false;
    connectTimeoutShown = false;
    sourceActiveAtMs = 0.0;
    saveButton.setEnabled (false);

    setStatus ("Scanning (" + juce::String (seaprobe::transportName (transport)) + ")...");

    detector = std::make_unique<seaprobe::LoggingDeviceDetector> (*logger, transport);

    if (detector->getSelectedPairs().empty() && Rise2Component::isConnected())
    {
        juce::String riseName = "Seaboard RISE";
        for (auto& d : juce::MidiOutput::getAvailableDevices())
            if (rise2::isRisePort (d.name)) { riseName = d.name; break; }
        const bool isGrand = rise2::isGrandName (riseName);

        setStatus (riseName + " found.");
        rise2Page = std::make_unique<Rise2Component> (*logger);
        addAndMakeVisible (*rise2Page);
        setBlocksControlsVisible (false);

        keyboardView.setOctaves (keyboardOctavesForName (riseName));
        keyboardView.setFamily (isGrand ? KeyboardView::Family::grand : KeyboardView::Family::rise);
        keyboardView.setBlocks (1);   // a RISE/GRAND is a single keybed (no DNA chaining)
        keyboardView.setChord ({});   // RISE/GRAND have no Chord mode - clear any Seaboard ghosts
        keyboardView.setVisible (true);

        // Size the GRAND keyboard from its reported key count (Studio 37 / Stage 61 / 88): the port
        // name carries no size, so the name-based guess defaults to 5 octaves. keyCount is exact.
        rise2Page->onKeyCount = [this] (int keyCount)
        {
            keyboardView.setOctaves (juce::roundToInt ((float) keyCount / 12.0f));
            resized();
        };

        // Live touch rings from the RISE/GRAND's performance MIDI (they have no BLOCKS touch surface).
        keyboardView.startTouchDisplay();
        rise2Page->onTouch = [this] (int chan, int note, float pressure, float bend, float strike, bool end)
        {
            const int   octs = juce::jmax (1, keyboardView.getOctaves());
            const int   base = 60 - octs * 6;   // centre the drawn keyboard roughly on middle C
            const float x = juce::jlimit (0.0f, 1.0f,
                ((float) note - (float) base + bend * 2.0f) / (float) (octs * 12));   // bend nudges x (Glide)
            // y stays centred: RISE/GRAND Slide (CC74) isn't parsed here, and the GRAND has none (4D).
            // Tile 0 is the single RISE/GRAND keybed (not a DNA block).
            keyboardView.setTouch (0, chan, x, 0.5f, pressure, strike, end);   // setTouch is thread-safe
        };

        resized();
        return;
    }

    if (detector->getSelectedPairs().empty())
    {
        setStatus ("No " + juce::String (seaprobe::transportName (transport)) + " Seaboard found. "
                   "Check the cable / Bluetooth, and that ROLI Dashboard isn't holding the port - "
                   "click 'Diagnose...' for help.");
        return;
    }

    setStatus ("Connecting over " + detector->getActiveTransport() + " to '"
               + detector->getSelectedPairs().front().input.name + "'...");
    source = std::make_unique<roli::PhysicalTopologySource> (*detector);
    source->addListener (this);
    source->setActive (true);
    sourceActiveAtMs = juce::Time::getMillisecondCounterHiRes(); // start the connect timeout
    startTimer (250);
}

void MainComponent::setBlocksControlsVisible (bool visible)
{
    for (auto* c : std::initializer_list<juce::Component*> { &reloadButton, &saveButton, &presetBox,
                                                            &blockLabel, &blockBox, &contributeBtn, &viewport })
        c->setVisible (visible);
}

void MainComponent::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

void MainComponent::topologyChanged()
{
    if (source == nullptr)
        return;

    const auto topo = source->getCurrentTopology();

    if (topo.blocks.isEmpty())
    {
        setListeningBlock (nullptr);
        block = nullptr;
        selectedBlockUid = 0;
        blockOrder.clear();
        clusterMembers.clear();
        setTouchListening ({});   // device gone: stop the live rings
        blockBox.clear (juce::dontSendNotification);
        rowsBuilt = false;
        configRequested = false;
        rows.clear();
        curveRows.clear();
        curves.clear();
        sectionHeaders.clear();
        layoutOrder.clear();
        rowHolder.removeAllChildren();
        pendingValues.clear();
        dirtyWrites.clear();
        liveWriter.cancel();
        saveButton.setEnabled (false);

        // A factory reset drops the device off the bus while it reboots — that's expected, not a
        // disconnect. Keep the "Resetting..." overlay/status until it comes back (or ~30s passes).
        const bool rebooting = resettingSinceMs > 0.0
                            && juce::Time::getMillisecondCounterHiRes() - resettingSinceMs < 30000.0;
        if (rebooting)
        {
            busyLabel.setVisible (true);
            busyLabel.toFront (false);
            setStatus (grab::u8 ("Resetting \xe2\x80\x94 please wait for the device to reboot..."));
        }
        else
        {
            resettingSinceMs = 0.0;
            busyLabel.setVisible (false);
            if (deviceSeen)
                setStatus ("Seaboard disconnected.");
            // else: we never got a device on this connection — keep the "Connecting…"
            // status; checkConnectTimeout() will give specific guidance if it stays empty.
        }
        return;
    }

    updateBlockList (topo);
}

void MainComponent::updateBlockList (const roli::BlockTopology& topo)
{
    blockOrder.clear();
    clusterMembers.clear();
    blockBox.clear (juce::dontSendNotification);

    // Group DNA-chained, same-type blocks under one logical device: they share one config
    // (edit one, the chain mirrors it), exactly like ROLI Dashboard. A block connected only
    // via its magnetic port reports isMasterBlock()==false and getConnectedMasterUID()==<the
    // USB master>, so grouping by (master uid, type) collapses a homogeneous chain into one
    // entry and leaves two separate USB cables (two masters) as two entries. Mixed-type
    // chains stay separate too (a Lightpad on the same master is configured on its own).
    struct Group { roli::Block::UID master; roli::Block::Type type; std::vector<roli::Block::Ptr> members; };
    std::vector<Group> groups;
    for (auto& b : topo.blocks)
    {
        const auto m = b->isMasterBlock() ? b->uid : b->getConnectedMasterUID();
        Group* g = nullptr;
        for (auto& c : groups)
            if (c.master == m && c.type == b->getType()) { g = &c; break; }
        if (g == nullptr) { groups.push_back ({ m, b->getType(), {} }); g = &groups.back(); }
        g->members.push_back (b);
    }

    int selectId = 0;
    roli::Block::UID newSelected = 0;
    for (int i = 0; i < (int) groups.size(); ++i)
    {
        auto& g = groups[(size_t) i];
        // left-to-right within the chain, so the keyboard halves draw in physical order
        std::sort (g.members.begin(), g.members.end(), [] (const roli::Block::Ptr& a, const roli::Block::Ptr& b)
                   { return a->getBlockAreaWithinLayout().x < b->getBlockAreaWithinLayout().x; });

        // Representative we read/display from: the USB master if present, else the left-most.
        roli::Block::Ptr rep = g.members.front();
        for (auto& mb : g.members) if (mb->isMasterBlock()) { rep = mb; break; }

        std::vector<roli::Block::UID> uids;
        for (auto& mb : g.members) uids.push_back (mb->uid);
        clusterMembers[rep->uid] = uids;
        blockOrder.push_back (rep->uid);

        juce::String label = rep->getDeviceDescription();
        if (g.members.size() > 1)
            label += "  x" + juce::String ((int) g.members.size()) + " (chained)";
        label += "  " + rep->serialNumber;
        blockBox.addItem (label, i + 1);

        // Keep the selection if the previously-selected uid is any member of this group.
        for (auto& mb : g.members)
            if (mb->uid == selectedBlockUid) { selectId = i + 1; newSelected = rep->uid; }
    }

    if (selectId == 0) // first time, or previously-selected block is gone
    {
        selectId = 1;
        newSelected = blockOrder.empty() ? 0 : blockOrder.front();
    }
    selectedBlockUid = newSelected;
    blockBox.setSelectedId (selectId, juce::dontSendNotification);

    const bool blockChanged = (block == nullptr) || (block->uid != selectedBlockUid);
    block = topo.getBlockWithUID (selectedBlockUid);
    setListeningBlock (block); // live config updates for the selected block
    deviceSeen = true;             // a real BLOCKS device responded; disarm the connect timeout
    connectTimeoutShown = false;

    updateKeyboardForBlock(); // joined keywave for a Seaboard (chain draws side-by-side)

    // Count distinct devices (a dual-homed device appears on both transports with the same uid).
    std::vector<juce::uint64> uniqueUids;
    for (auto& b : topo.blocks)
        if (b != nullptr && std::find (uniqueUids.begin(), uniqueUids.end(), (juce::uint64) b->uid) == uniqueUids.end())
            uniqueUids.push_back ((juce::uint64) b->uid);
    const int blockCount = (int) uniqueUids.size();
    const int dnaLinks   = detector != nullptr ? (int) detector->getDnaLinks().size() : 0;

    const bool unseen = blockCount == 1 && detector != nullptr && detector->hasUnseenDnaNeighbour();

    setStatus ("Connected (" + (detector != nullptr ? detector->getActiveTransport() : juce::String())
               + "): " + juce::String (blockCount) + " block(s)"
               + (blockCount > 1 ? ", " + juce::String (dnaLinks) + " DNA link(s)" : juce::String())
               + (block != nullptr ? "   -   " + block->serialNumber + " fw " + block->versionNumber
                                       + " battery " + juce::String (juce::roundToInt (block->getBatteryLevel() * 100.0f)) + "%"
                                     : juce::String())
               + (unseen ? grab::u8 ("   -   DNA-linked block visible over Bluetooth \xe2\x80\x94 switch to Auto to see it")
                         : juce::String()));

    // Diagnostic: log what the SDK reports about the physical arrangement, so we can see whether DNA
    // join/separate is detectable (does connections[] change? do block positions become adjacent?).
    if (logger != nullptr)
    {
        logger->line ("TOPOLOGY: " + juce::String (topo.blocks.size()) + " block(s), "
                      + juce::String (topo.connections.size()) + " connection(s)");
        for (auto& b : topo.blocks)
            if (b != nullptr)
            {
                const auto a = b->getBlockAreaWithinLayout();
                logger->line ("  block " + b->serialNumber
                              + "  master=" + juce::String (b->isMasterBlock() ? "yes" : "no")
                              + "  area=(" + juce::String (a.x) + "," + juce::String (a.y) + " "
                              + juce::String (a.width) + "x" + juce::String (a.height) + ")"
                              + "  rot=" + juce::String (b->getRotation()));
            }
        for (auto& c : topo.connections)
            logger->line ("  connection: " + juce::String ((juce::int64) c.device1)
                          + " <-> " + juce::String ((juce::int64) c.device2));
    }

    if (block != nullptr)
    {
        const auto type = juce::String (grab::blockTypeName (block->getType()));
        if (learnedForType != type)
            fetchNames ("BLOCKS", type); // self-learn: pull names for this device type
    }

    if (block != nullptr && (blockChanged || ! configRequested))
        requestConfig();
}

void MainComponent::updateLumiKeyboardColours()
{
    // Reads the current (edited) config values; the KeyboardView only uses them when it's showing
    // the LUMI/Piano family, so this is a no-op for other devices. Works for the dev preview too
    // (values live in pendingValues whether or not a real block is connected).
    const auto val = [this] (juce::uint32 id, int def)
    {
        auto it = pendingValues.find (id);
        return it != pendingValues.end() ? (int) it->second : def;
    };

    const int  scale      = val (22, 0);                       // Scale (0..18)
    const int  key        = val (27, 0);                       // Key / root (0..11)
    const auto globalC     = (juce::uint32) val (34, (int) 0xff00b0bb);  // Global Key Color (in-scale)
    const auto rootC       = (juce::uint32) val (35, (int) 0xffffffff);  // Root Key Color
    const int  activeMode = juce::jlimit (0, 3, val (20, 0));  // Active Mode (0..3)
    const int  schemeId   = 100 + 7 * activeMode;              // that mode's Color Scheme id
    const int  scheme     = val (schemeId, 1);                 // 0 Pro, 1 User, 2 Piano, 3 Stage, 4 Rainbow

    keyboardView.setColourScheme (scheme, scale, key, globalC, rootC);
}

// Chord-mode intervals (semitones above the played note) per ROLI's Chord config (id 24), in the
// device's option order (see ConfigOverrides::chordNames). Value 0 = Off = no ghost notes. Verified
// against ROLI Dashboard / a Seaboard Block (fixed shapes rooted on the played note, not diatonic).
static std::vector<int> chordIntervalsFor (int chord)
{
    switch (chord)
    {
        case 1:  return { 12 };             // Octave
        case 2:  return { 7 };              // Fifth
        case 3:  return { 4, 7 };           // Major
        case 4:  return { 3, 7 };           // minor
        case 5:  return { 5, 7 };           // Sus4
        case 6:  return { 2, 7 };           // Sus2
        case 7:  return { 4, 7, 10 };       // 7th
        case 8:  return { 4, 7, 11 };       // Major7
        case 9:  return { 3, 7, 10 };       // minor7
        case 10: return { 4, 7, 10, 14 };   // 9th
        case 11: return { 3, 7, 10, 14 };   // minor9
        case 12: return { 3, 6 };           // Diminished
        case 13: return { 4, 8 };           // Augmented
        case 14: return { 3, 6, 10 };       // m7b5
        default: return {};                 // 0 Off (or unknown)
    }
}

void MainComponent::pushChordGhosts()
{
    // Chord ghosts only make sense on the Seaboard Block (the one touch-ring family with a Chord
    // config), so clear them otherwise - a stale chord must never haunt another device's rings.
    const bool seaboard = block != nullptr && block->getType() == roli::Block::Type::seaboardBlock;
    if (! seaboard) { keyboardView.setChord ({}); return; }

    auto it = pendingValues.find (24);                                   // Chord (id 24): 0 = Off
    keyboardView.setChord (chordIntervalsFor (it != pendingValues.end() ? (int) it->second : 0));
}

// Map each block in the selected device's DNA group to a drawn tile, positioned by the SDK's
// shared-unit layout (getBlockAreaWithinLayout + rotation). However the user snaps the blocks,
// this reproduces the real X/Y arrangement — Seaboard keybed, Lightpad pad-grid, Control row, LUMI.
std::vector<KeyboardView::Tile> MainComponent::buildBlockTiles()
{
    std::vector<KeyboardView::Tile> out;
    if (source == nullptr || block == nullptr)
        return out;

    const auto topo = source->getCurrentTopology();

    // Draw EVERY connected block, like ROLI Dashboard. In Auto (USB+BLE) a device reachable on both
    // transports is reported twice with the SAME uid; dedup so it draws once. The selected block is
    // highlighted; tiles with no usable layout (not DNA-joined) get reflowed into a gapped row.
    std::vector<juce::uint64> seen;
    std::map<juce::uint64, juce::String> serialOf;
    for (auto& b : topo.blocks)
    {
        if (b == nullptr || std::find (seen.begin(), seen.end(), (juce::uint64) b->uid) != seen.end())
            continue;
        seen.push_back ((juce::uint64) b->uid);
        serialOf[(juce::uint64) b->uid] = b->serialNumber;

        KeyboardView::Tile t;
        t.uid      = (juce::uint64) b->uid;
        t.rotation = ((b->getRotation() % 4) + 4) % 4;

        // Default footprint per model (logical units, from roli_BlockModels.h), used if the SDK
        // reports a degenerate layout area.
        int dw = 2, dh = 2;
        switch (b->getType())
        {
            case roli::Block::Type::seaboardBlock:    t.family = KeyboardView::Family::block;    t.octaves = 2; dw = 6; dh = 3; break;
            case roli::Block::Type::lumiKeysBlock:    t.family = KeyboardView::Family::lumi;      t.octaves = 2; dw = 6; dh = 3; break;
            case roli::Block::Type::roliPiano49:      t.family = KeyboardView::Family::lumi;      t.octaves = 4; dw = 14; dh = 4; break;
            case roli::Block::Type::lightPadBlock:    t.family = KeyboardView::Family::lightpad;  t.grid = 15;   dw = 2; dh = 2; break;
            case roli::Block::Type::lightpadProBlock: t.family = KeyboardView::Family::lightpad;  t.grid = 15;   dw = 4; dh = 4; break;
            case roli::Block::Type::liveBlock:
            case roli::Block::Type::loopBlock:
            case roli::Block::Type::touchBlock:
            case roli::Block::Type::developerControlBlock: t.family = KeyboardView::Family::control; dw = 2; dh = 1; break;
            default: continue;   // airwave / unknown: not drawn
        }

        const auto area = b->getBlockAreaWithinLayout();
        t.ux = (float) area.x;  t.uy = (float) area.y;
        t.uw = (float) (area.width  > 0 ? area.width  : dw);
        t.uh = (float) (area.height > 0 ? area.height : dh);
        out.push_back (t);
    }

    // The SDK only lays a cluster out when it sees the DNA link as a topology connection; otherwise
    // every block comes back at the origin and the tiles would stack on top of each other. Detect
    // that (any overlap) and fall back to a left-to-right row so a chain still draws as two bodies.
    bool overlap = false;
    for (size_t i = 0; i < out.size() && ! overlap; ++i)
        for (size_t j = i + 1; j < out.size() && ! overlap; ++j)
        {
            const auto& a = out[i]; const auto& c = out[j];
            overlap = a.ux < c.ux + c.uw && c.ux < a.ux + a.uw && a.uy < c.uy + c.uh && c.uy < a.uy + a.uh;
        }
    if (overlap && out.size() > 1)
    {
        // The stock library doesn't lay a cross-transport DNA cluster out, so lay it out ourselves,
        // using the links we stitched from the raw topology packets: DNA-joined neighbours draw
        // touching (gap 0), independent blocks keep a gap so they read as separate boards.
        const auto links = (detector != nullptr) ? detector->getDnaLinks()
                                                  : std::vector<std::pair<juce::String, juce::String>>{};
        auto linked = [&links] (const juce::String& a, const juce::String& b)
        {
            for (auto& l : links)
                if ((l.first == a && l.second == b) || (l.first == b && l.second == a)) return true;
            return false;
        };
        std::sort (out.begin(), out.end(), [] (const KeyboardView::Tile& a, const KeyboardView::Tile& b)
                   { return a.ux != b.ux ? a.ux < b.ux : a.uy != b.uy ? a.uy < b.uy : a.uid < b.uid; });
        float x = 0.0f;
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i].ux = x; out[i].uy = 0.0f; out[i].rotation = 0;
            x += out[i].uw;
            if (i + 1 < out.size())
                x += linked (serialOf[out[i].uid], serialOf[out[i + 1].uid]) ? 0.0f : 1.0f;
        }
    }

    return out;
}

void MainComponent::selectBlockByUid (juce::uint64 clickedUid)
{
    // The dropdown lists one entry per group (rep uid in blockOrder); a clicked tile may be any
    // member of a group. Find the group that owns the clicked block and drive the dropdown, which
    // fires its onChange -> selectBlock on the rep. Selecting the already-current group is a no-op.
    for (size_t i = 0; i < blockOrder.size(); ++i)
    {
        const auto rep = blockOrder[i];
        bool match = (rep == (roli::Block::UID) clickedUid);
        if (! match)
        {
            auto it = clusterMembers.find (rep);
            if (it != clusterMembers.end())
                for (auto m : it->second)
                    if (m == (roli::Block::UID) clickedUid) { match = true; break; }
        }
        if (match) { blockBox.setSelectedId ((int) i + 1, juce::sendNotification); return; }
    }
}

void MainComponent::updateKeyboardForBlock()
{
    const auto type  = block != nullptr ? block->getType() : roli::Block::Type::unknown;
    auto       tiles = buildBlockTiles();

    if (! tiles.empty())
    {
        keyboardView.setLayout (tiles);        // the real DNA arrangement, tiled by topology
        keyboardView.setVisible (true);

        // Highlight the selected device's block(s) within the arrangement (dims the rest, like Dashboard).
        std::vector<juce::uint64> selUids;
        auto cit = clusterMembers.find (selectedBlockUid);
        if (cit != clusterMembers.end()) for (auto m : cit->second) selUids.push_back ((juce::uint64) m);
        else                             selUids.push_back ((juce::uint64) selectedBlockUid);
        keyboardView.setSelected (selUids);

        // Listen for touches on every member; setTouchListening skips blocks with no touch surface
        // (control blocks, un-surfaced keybeds), so the arrangement still draws statically for those.
        std::vector<roli::Block::UID> members;
        for (auto& t : tiles) members.push_back ((roli::Block::UID) t.uid);
        setTouchListening (members);

        if (type == roli::Block::Type::lumiKeysBlock || type == roli::Block::Type::roliPiano49)
            updateLumiKeyboardColours();        // light any LUMI/Piano tile from its scale/colours
    }
    else
    {
        keyboardView.setVisible (false);
        setTouchListening ({});
    }

    pushChordGhosts();     // Seaboard/Lightpad: feed the active Chord mode to the touch-ring ghosts (clears otherwise)
    populatePresetBox();   // ROLI preset picker: shown + filled only for a Seaboard
}

void MainComponent::setTouchListening (const std::vector<roli::Block::UID>& members)
{
    for (auto& b : listenedBlocks)
        if (auto* ts = b->getTouchSurface())
            ts->removeListener (this);
    listenedBlocks.clear();

    std::map<roli::Block::UID, TouchGeom> geom;
    if (source != nullptr && ! members.empty())
    {
        const auto topo = source->getCurrentTopology();
        const int n = (int) members.size();
        for (int i = 0; i < n; ++i)
            if (auto b = topo.getBlockWithUID (members[(size_t) i]))
                if (auto* ts = b->getTouchSurface())
                {
                    ts->addListener (this);
                    listenedBlocks.push_back (b);   // keep the block (and its surface) alive
                    geom[b->uid] = { i, n, (float) juce::jmax (1, b->getWidth()), (float) juce::jmax (1, b->getHeight()) };
                }
    }

    { const juce::ScopedLock sl (touchGeomLock); touchGeometry = std::move (geom); }

    if (listenedBlocks.empty()) keyboardView.stopTouchDisplay();
    else                        keyboardView.startTouchDisplay();
}

void MainComponent::touchChanged (roli::TouchSurface&, const roli::TouchSurface::Touch& t)
{
    TouchGeom g;
    {
        const juce::ScopedLock sl (touchGeomLock);
        auto it = touchGeometry.find (t.blockUID);
        if (it == touchGeometry.end())
            return;
        g = it->second;
    }

    // nx/ny are 0..1 within this block's own surface; the keyboard view places them on the block's
    // tile, so no cross-chain remap is needed any more (each block is its own positioned tile).
    const float nx = juce::jlimit (0.0f, 1.0f, t.x / g.w);
    const float ny = juce::jlimit (0.0f, 1.0f, t.y / g.h);
    const juce::uint64 id = t.blockUID ^ ((juce::uint64) (juce::uint32) t.index * 0x9E3779B97F4A7C15ULL);

    // Strike = how hard the finger hit, captured ONCE at contact and held for the note's life, so the
    // solid disc reflects only the initial impact (Press - the ring - tracks live z). The Seaboard
    // reports strike as the touch-start's zVelocity (initial contact velocity, already 0..1 - this is
    // exactly what ROLI's own stack stores as the strike pressure), not the instantaneous z, which is
    // still ramping at that instant. Fall back to z only if the device didn't send a velocity.
    const float z = juce::jlimit (0.0f, 1.0f, t.z);
    float strike;
    if (t.isTouchEnd)
    {
        blockTouchStrike.erase (id);
        strike = 0.0f;
    }
    else if (t.isTouchStart)
    {
        strike = juce::jlimit (0.0f, 1.0f, t.zVelocity > 0.0f ? t.zVelocity : z);
        blockTouchStrike[id] = strike;
    }
    else
    {
        auto s = blockTouchStrike.find (id);
        strike = s != blockTouchStrike.end() ? s->second : z;
    }

    keyboardView.setTouch ((juce::uint64) t.blockUID, (int) t.index, nx, ny, z, strike, t.isTouchEnd);
}

void MainComponent::selectBlock (roli::Block::UID uid)
{
    selectedBlockUid = uid;
    if (source == nullptr)
        return;

    block = source->getCurrentTopology().getBlockWithUID (uid);
    setListeningBlock (block);

    // Keyboard graphic for keyboard-family blocks (Seaboard); hidden for pad/control blocks.
    updateKeyboardForBlock();

    // Reset per-block config state and re-read for the newly selected block.
    rows.clear();
    curveRows.clear();
    curves.clear();
    sectionHeaders.clear();
    layoutOrder.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();
    dirtyWrites.clear();
    liveWriter.cancel();
    configRequested = false;
    rowsBuilt = false;
    saveButton.setEnabled (false);

    if (block != nullptr)
    {
        setStatus ("Reading " + block->serialNumber + "...");
        requestConfig();
    }
}

void MainComponent::openDialog (juce::Component* panel, const juce::String& title)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (panel);
    o.dialogTitle = title;
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::checkForUpdate()
{
    // Compare dotted versions ("0.1.31" vs "0.1.30"); returns true if `latest` is newer than `cur`.
    auto isNewer = [] (juce::String latest, juce::String cur)
    {
        latest = latest.trimCharactersAtStart ("vV");
        cur    = cur.trimCharactersAtStart ("vV");
        juce::StringArray a, b;
        a.addTokens (latest, ".", ""); b.addTokens (cur, ".", "");
        for (int i = 0; i < juce::jmax (a.size(), b.size()); ++i)
        {
            const int x = i < a.size() ? a[i].getIntValue() : 0;
            const int y = i < b.size() ? b[i].getIntValue() : 0;
            if (x != y) return x > y;
        }
        return false;
    };

    juce::Component::SafePointer<MainComponent> safe (this);
    std::thread ([safe, isNewer]
    {
        juce::String tag, htmlUrl;
        {
            juce::URL url ("https://api.github.com/repos/demixme/GRAB/releases/latest");
            juce::WebInputStream web (url, false);
            web.withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: GRAB");
            web.withConnectionTimeout (8000);
            if (web.connect (nullptr) && web.getStatusCode() == 200)
            {
                const auto json = juce::JSON::parse (web.readEntireStreamAsString());
                tag     = json.getProperty ("tag_name", {}).toString();
                htmlUrl = json.getProperty ("html_url", {}).toString();
            }
        }
        if (tag.isEmpty())
            return;

        juce::MessageManager::callAsync ([safe, tag, htmlUrl, isNewer]
        {
            auto* p = safe.getComponent();
            if (p != nullptr && isNewer (tag, GRAB_VERSION))
                p->footer.setUpdateAvailable (tag, htmlUrl.isNotEmpty() ? htmlUrl
                                                  : "https://github.com/demixme/GRAB/releases/latest");
        });
    }).detach();
}

void MainComponent::openCaptureWindow()
{
    // Give the capture tool a way to turn config item ids into plain-English names + values, using
    // whatever device is currently connected (falls back to "item N" when nothing is connected).
    juce::Component::SafePointer<MainComponent> self (this);
    CapturePanel::DescribeFn describe = [self] (juce::uint32 item, juce::int32 value)
        -> std::pair<juce::String, juce::String>
    {
        if (auto* p = self.getComponent())
            return p->describeConfig (item, value);
        return { "item " + juce::String (item), juce::String (value) };
    };

    openDialog (new CapturePanel (std::move (describe)), "MIDI capture");
}

std::pair<juce::String, juce::String> MainComponent::describeConfig (juce::uint32 item, juce::int32 value) const
{
    if (block != nullptr && item < block->getMaxConfigIndex())
    {
        auto meta = block->getLocalConfigMetaData (item);
        seaprobe::applyConfigOverrides (meta);
        grab::applyLearnedNames (meta, learnedNames);

        juce::String name = meta.name.isNotEmpty() ? meta.name : ("item " + juce::String (item));

        using ConfigType = roli::Block::ConfigMetaData::ConfigType;
        juce::String val;
        if (meta.type == ConfigType::boolean)
        {
            val = (value != 0) ? "On" : "Off";
        }
        else if (meta.type == ConfigType::options)
        {
            const int i = value - meta.range.getStart();
            if (i >= 0 && i < roli::Block::ConfigMetaData::numOptionNames && meta.optionNames[i].isNotEmpty())
                val = meta.optionNames[i] + "  (" + juce::String (value) + ")";
            else
                val = juce::String (value);
        }
        else
        {
            val = juce::String (value);
        }
        return { name, val };
    }

    // Item the connected device doesn't model (or no block): show it raw — still useful for
    // spotting a setting GRAB can't see yet.
    return { "item " + juce::String (item), juce::String (value) };
}

void MainComponent::showHelpWanted()
{
    openDialog (new WelcomePanel(), "How you can help");
}

void MainComponent::contribute()
{
    if (block == nullptr)
    {
        setStatus ("Connect a device first, then Contribute.");
        return;
    }
    openDialog (new ContributePanel (block, detector != nullptr ? detector->getActiveTransport()
                                                                : juce::String ("unknown")),
                "Contribute device profile");
}

void MainComponent::fetchNames (const juce::String& family, const juce::String& type)
{
    learnedForType = type; // mark attempted so topology churn doesn't refetch

    // Derive the names URL from the saved contribute endpoint (.../profiles -> .../names).
    juce::PropertiesFile::Options po; po.millisecondsBeforeSaving = 0;
    juce::PropertiesFile props (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                    .getChildFile ("GRAB").getChildFile ("settings.properties"), po);
    const auto base = props.getValue ("profileEndpoint", "https://www.demix.me/grab/v1/profiles");

    juce::URL url (base.replace ("profiles", "names"));
    url = url.withParameter ("family", family).withParameter ("type", type);

    juce::Component::SafePointer<MainComponent> safe (this);
    std::thread ([safe, url, type]
    {
        juce::String resp;
        {
            juce::WebInputStream web (url, false);
            web.withConnectionTimeout (10000);
            if (web.connect (nullptr) && web.getStatusCode() == 200)
                resp = web.readEntireStreamAsString();
        }
        auto map = grab::parseNamesJson (resp);
        juce::MessageManager::callAsync ([safe, type, map]
        {
            if (auto* p = safe.getComponent())
                p->onNamesFetched (type, map);
        });
    }).detach();
}

void MainComponent::onNamesFetched (juce::String type, grab::LearnedMap learned)
{
    if (block == nullptr || juce::String (grab::blockTypeName (block->getType())) != type)
        return; // the selected device changed since we asked
    if (learned.empty())
        return;
    learnedNames = std::move (learned);
    if (rowsBuilt)
        rebuildRows(); // re-label items with the learned names
}

void MainComponent::requestConfig()
{
    if (block == nullptr)
        return;
    block->requestFactoryConfigSync();
    configRequested = true;
    rowsBuilt = false;
    configRequestedAtMs = juce::Time::getMillisecondCounterHiRes();
}

void MainComponent::timerCallback()
{
    // Fallback only: normally handleConfigSyncEnded() builds the rows the moment the
    // device signals the config dump is complete. This covers the case where that
    // message never arrives (so the UI still appears after a short wait).
    if (block != nullptr && configRequested && ! rowsBuilt
        && juce::Time::getMillisecondCounterHiRes() - configRequestedAtMs > 2500.0)
    {
        rebuildRows();
        rowsBuilt = true;
    }

    // DNA join/separate doesn't change the block set (both blocks stay present over BLE), so the
    // topology callback won't fire. Poll our stitched link state and re-lay-out when it changes:
    // blocks snap touching when DNA-joined, gap apart when separated.
    if (source != nullptr && detector != nullptr)
    {
        juce::String sig (detector->hasUnseenDnaNeighbour() ? "U;" : "");
        for (auto& l : detector->getDnaLinks()) sig << l.first << "~" << l.second << ";";
        if (sig != lastDnaSig) { lastDnaSig = sig; updateBlockList (source->getCurrentTopology()); }
    }

    checkConnectTimeout();
}

void MainComponent::checkConnectTimeout()
{
    // A port was opened but no BLOCKS topology ever arrived. The usual cause is a
    // non-BLOCKS device that our name match still picked up — a Seaboard RISE/GRAND
    // (different protocol), or a port held by another app. Turn the silent wait into
    // clear guidance + a request to report, instead of hanging on "Connecting…".
    if (source == nullptr || deviceSeen || connectTimeoutShown || sourceActiveAtMs <= 0.0)
        return;
    if (juce::Time::getMillisecondCounterHiRes() - sourceActiveAtMs < 6000.0)
        return;

    connectTimeoutShown = true;

    juce::String name;
    if (detector != nullptr && ! detector->getSelectedPairs().empty())
        name = detector->getSelectedPairs().front().input.name;

    const auto path = logFolder.getFullPathName();

    // RISE and GRAND are excluded from the BLOCKS detector and handled by the RISE page, so a port
    // that opens here and never returns a BLOCKS topology is a device GRAB doesn't model yet, or a
    // port held by another app.
    setStatus ("Opened '" + name + "' but no ROLI BLOCKS device replied. It may be a device GRAB "
               "doesn't support yet, or the port is held by another app - click 'Diagnose...', or try "
               "Advanced -> Help add device.");

    // Full guidance (incl. the log path to attach to a report) goes to the log.
    logger->line ("CONNECT TIMEOUT: opened '" + name + "' but received no BLOCKS topology in 6s.");
    logger->line ("  To report: open an issue at https://github.com/demixme/GRAB/issues and attach the "
                  "'=== MIDI inputs ===' lines above. Log file: " + path);
}

void MainComponent::setListeningBlock (roli::Block::Ptr b)
{
    if (listeningBlock == b)
        return;
    if (listeningBlock != nullptr)
        listeningBlock->removeConfigItemListener (this);
    listeningBlock = b;
    if (listeningBlock != nullptr)
        listeningBlock->addConfigItemListener (this);
}

void MainComponent::handleConfigSyncEnded (roli::Block& b)
{
    // The device finished dumping its config (after requestFactoryConfigSync). Build
    // from the complete, current set — reliable, and picks up any change in which items
    // are active (e.g. switching MIDI Mode changes the available controls).
    if (block == nullptr || &b != block.get())
        return;
    rebuildRows();
    rowsBuilt = true;
    configRequested = false;
}

void MainComponent::handleConfigItemChanged (roli::Block& b, const roli::Block::ConfigMetaData&,
                                             juce::uint32 index)
{
    if (block == nullptr || &b != block.get())
        return;
    if (! rowsBuilt)
        return; // still syncing; handleConfigSyncEnded() builds the full set
    applyLiveConfigChange (block->getLocalConfigMetaData (index), index);
}

bool MainComponent::controlExistsFor (juce::uint32 id) const
{
    for (auto& r : rows)   if (r->getId() == id) return true;
    // A curve owns its sensitivity id AND its min/max end-node ids (50-55). Those have no row
    // of their own, so without this a live-write echo of a min/max value looked like the visible
    // set had changed and triggered a full rebuildRows() mid-drag (jerky handles, flashing Save).
    for (auto* c : curves)
        if (c->getId() == id || c->getMinId() == (int) id || c->getMaxId() == (int) id)
            return true;
    return false;
}

void MainComponent::applyLiveConfigChange (const roli::Block::ConfigMetaData& meta, juce::uint32 index)
{
    if (block == nullptr)
        return;

    // If an item just became active or inactive, the set of visible controls changed
    // (e.g. a mode switch) — rebuild the whole thing so new controls appear / go away.
    if (meta.isActive != controlExistsFor (index))
    {
        rebuildRows();
        rowsBuilt = true;
        return;
    }
    if (! meta.isActive)
        return;

    // Value-only change: mirror it into the control, but never clobber an unsaved edit.
    const auto v   = block->getLocalConfigValue (index);
    const auto dv  = deviceValues.find (index);
    const bool userEdited = dv != deviceValues.end()
                            && pendingValues.count (index)
                            && pendingValues[index] != dv->second;
    deviceValues[index] = v;

    if (! userEdited)
    {
        pendingValues[index] = v;
        for (auto& r : rows)   if (r->getId() == index) { r->setValue (v);       r->setChangedFromDevice (false); }
        for (auto* c : curves) if (c->getId() == index) { c->setSensitivity (v); c->setChanged (false); }
    }
    else
    {
        const bool stillDiffers = pendingValues[index] != v;
        for (auto& r : rows)   if (r->getId() == index) r->setChangedFromDevice (stillDiffers);
        for (auto* c : curves) if (c->getId() == index) c->setChanged (stillDiffers);
    }

    updateSaveEnabled();
    updateLumiKeyboardColours();   // a device-reported change (scale/key/colour/mode) re-lights the graphic
    pushChordGhosts();             // ...and a Chord-mode change re-aims the touch ghosts
}

void MainComponent::rebuildRows()
{
    // Config is back (e.g. the device finished rebooting after a factory reset): clear the overlay.
    resettingSinceMs = 0.0;
    busyLabel.setVisible (false);

    cards.clear();          // un-parents the rows/curves it holds
    rows.clear();
    curveRows.clear();
    curves.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();
    dirtyWrites.clear();
    liveWriter.cancel();

    if (block == nullptr && simConfig.empty())
        return;

    const auto onChange = [this] (juce::uint32 id, juce::int32 v) { onItemChanged (id, v); };

    // Collect active items grouped by their device-reported group, preserving id
    // order within a group and first-seen order of groups.
    struct Item { roli::Block::ConfigMetaData meta; juce::int32 value; };
    std::vector<juce::String> groupOrder;
    std::map<juce::String, std::vector<Item>> groups;

    // One item -> its card/group, after overrides and learned names are applied.
    const auto collect = [&] (roli::Block::ConfigMetaData meta, juce::int32 value)
    {
        if (! meta.isActive)            // only items the device actually reports
            return;
        seaprobe::applyConfigOverrides (meta);        // built-in overrides (e.g. Chord, Scale)
        grab::applyLearnedNames (meta, learnedNames);  // names pulled from the store

        deviceValues[meta.item] = value;
        pendingValues[meta.item] = value;

        auto g = meta.group;
        if (std::find (groupOrder.begin(), groupOrder.end(), g) == groupOrder.end())
            groupOrder.push_back (g);
        groups[g].push_back ({ meta, value });
    };

    if (block != nullptr)
    {
        const auto maxId = block->getMaxConfigIndex();
        for (juce::uint32 id = 0; id < maxId; ++id)
            collect (block->getLocalConfigMetaData (id), block->getLocalConfigValue (id));
    }
    else
    {
        for (const auto& item : simConfig)   // dev preview: synthetic device
            collect (item.first, item.second);
    }

    // Order sections roughly like ROLI Dashboard.
    std::stable_sort (groupOrder.begin(), groupOrder.end(),
                      [] (const juce::String& a, const juce::String& b) { return groupRank (a) < groupRank (b); });

    // The 5D sensitivity config ids -> curve panels (Strike/Glide/Slide/Press/Lift).
    struct Dim { juce::uint32 id; const char* label; juce::uint32 colour; int minId; int maxId; };
    static const Dim dims[] = {
        { 10, "Strike", grab::theme::strike.getARGB(), 50, 51 }, { 11, "Glide", grab::theme::glide.getARGB(), -1, -1 },
        { 12, "Slide",  grab::theme::slide.getARGB(),  -1, -1 }, { 13, "Press", grab::theme::press.getARGB(), 52, 53 },
        { 14, "Lift",   grab::theme::lift.getARGB(),   54, 55 } };

    for (const auto& g : groupOrder)
    {
        auto card = std::make_unique<SectionCard> (g.isEmpty() ? "Other" : g);

        auto emitRow = [&] (const Item& item)
        {
            auto row = std::make_unique<ConfigRow> (item.meta, item.value, onChange);
            card->addItem (*row, rowHeight);
            rows.push_back (std::move (row));
        };

        if (g == "Sensitivity")
        {
            // 5D curves across the top of the card; remaining items (min/max, fixed
            // velocity) as rows below.
            auto cr = std::make_unique<CurveRow>();
            for (const auto& d : dims)
            {
                if (deviceValues.find (d.id) == deviceValues.end())
                    continue;
                const int mn = (d.minId > 0 && deviceValues.count ((juce::uint32) d.minId)) ? deviceValues[(juce::uint32) d.minId] : 0;
                const int mx = (d.maxId > 0 && deviceValues.count ((juce::uint32) d.maxId)) ? deviceValues[(juce::uint32) d.maxId] : 127;
                auto* c = cr->add (std::make_unique<CurveComponent> (d.label, juce::Colour (d.colour),
                                        d.id, deviceValues[d.id], d.minId, mn, d.maxId, mx, onChange));
                curves.push_back (c);
            }
            if (! cr->curves.empty())
            {
                card->addItem (*cr, 130);
                curveRows.push_back (std::move (cr));
            }

            // Min/max are now set by dragging the curve end-nodes, so skip their rows.
            auto isCurveMinMax = [&] (juce::uint32 itemId)
            {
                for (const auto& d : dims)
                    if ((d.minId > 0 && (juce::uint32) d.minId == itemId) || (d.maxId > 0 && (juce::uint32) d.maxId == itemId))
                        return true;
                return false;
            };
            for (const auto& item : groups[g])
                if ((item.meta.item < 10 || item.meta.item > 14) && ! isCurveMinMax (item.meta.item))
                    emitRow (item);
        }
        else
        {
            for (const auto& item : groups[g])
                emitRow (item);
        }

        rowHolder.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }

    resized(); // positions the cards and sizes rowHolder
    saveButton.setEnabled (false);
    updateLumiKeyboardColours();   // (re)light the LUMI/Piano keyboard from the freshly-read config

    // Append the item/section count to the connect status, first stripping any count we appended on
    // an earlier rebuild (rebuildRows can fire several times per connect) so it doesn't accumulate.
    juce::String base = statusLabel.getText();
    const int itemsIdx = base.lastIndexOf (" items in ");
    if (itemsIdx >= 0)
    {
        const int sepIdx = base.substring (0, itemsIdx).lastIndexOf ("   -   ");
        if (sepIdx >= 0) base = base.substring (0, sepIdx);
    }
    setStatus (base + "   -   " + juce::String ((int) rows.size())
               + " items in " + juce::String ((int) groupOrder.size()) + " sections");
}

void MainComponent::onItemChanged (juce::uint32 id, juce::int32 value)
{
    pendingValues[id] = value;
    const bool differs = deviceValues.count (id) && deviceValues[id] != value;
    for (auto& r : rows)
        if (r->getId() == id)
            r->setChangedFromDevice (differs);
    for (auto* c : curves)
        if (c->getId() == id)
            c->setChanged (differs);
    updateSaveEnabled();
    updateLumiKeyboardColours();   // live-light the keyboard as scale/key/colours/mode are edited
    pushChordGhosts();             // live-update the chord ghosts as Chord mode is edited

    // Live-write, Dashboard-style: queue this edit and let the throttle flush it to the
    // device within ~75 ms. A drag coalesces into one burst per window instead of a write
    // per pixel. (Preset loads don't come through here, so they still wait for Save.)
    if (differs)
    {
        dirtyWrites.insert (id);
        liveWriter.nudge();
    }
}

std::vector<roli::Block::Ptr> MainComponent::writeTargets() const
{
    // Fan writes out to every block in the DNA cluster so the shared config stays in sync,
    // the way Dashboard does (edit one, the whole chain mirrors it). A lone block writes once.
    std::vector<roli::Block::Ptr> targets;
    if (source != nullptr)
    {
        const auto topo = source->getCurrentTopology();
        auto it = clusterMembers.find (selectedBlockUid);
        if (it != clusterMembers.end())
            for (auto uid : it->second)
                if (auto b = topo.getBlockWithUID (uid)) targets.push_back (b);
    }
    if (targets.empty() && block != nullptr)
        targets.push_back (block);
    return targets;
}

void MainComponent::flushLiveWrites()
{
    if (dirtyWrites.empty() || block == nullptr)
    {
        dirtyWrites.clear();
        liveWriter.cancel();   // nothing left to write; stop ticking until the next edit
        return;
    }

    auto targets = writeTargets();
    for (auto id : dirtyWrites)
    {
        auto pv = pendingValues.find (id);
        if (pv == pendingValues.end())
            continue;
        for (auto& t : targets)
            t->setLocalConfigValue (id, pv->second);   // setConfig write (persists on fw 1.4.3)
        deviceValues[id] = pv->second;                 // optimistic; a reload re-reads the truth
    }
    dirtyWrites.clear();

    // The device now matches (optimistically), so clear the "changed" markers + Save button.
    for (auto& r : rows)
        r->setChangedFromDevice (deviceValues.count (r->getId())
                                 && pendingValues.count (r->getId())
                                 && deviceValues[r->getId()] != pendingValues[r->getId()]);
    for (auto* c : curves)
        c->setChanged (deviceValues.count (c->getId())
                       && pendingValues.count (c->getId())
                       && deviceValues[c->getId()] != pendingValues[c->getId()]);
    updateSaveEnabled();
}

void MainComponent::updateSaveEnabled()
{
    bool any = false;
    for (auto& kv : pendingValues)
    {
        auto d = deviceValues.find (kv.first);
        if (d != deviceValues.end() && d->second != kv.second)
            any = true;
    }
    saveButton.setEnabled (any);
}

void MainComponent::saveToDevice()
{
    if (block == nullptr)
        return;

    // Save is now a flush-all / force-resync: live-write already sent most edits as you made
    // them, but this still catches anything outstanding (e.g. a just-loaded preset) and then
    // re-reads to confirm. Drain the live queue so the throttle doesn't double-write.
    dirtyWrites.clear();
    liveWriter.cancel();

    auto targets = writeTargets();
    int written = 0;
    for (auto& kv : pendingValues)
    {
        auto it = deviceValues.find (kv.first);
        if (it != deviceValues.end() && it->second != kv.second)
        {
            for (auto& t : targets)
                t->setLocalConfigValue (kv.first, kv.second); // setConfig write (persists on fw 1.4.3)
            ++written;
        }
    }

    setStatus ("Wrote " + juce::String (written) + " item(s)"
               + (targets.size() > 1 ? " to " + juce::String ((int) targets.size()) + " chained blocks" : juce::String())
               + "; re-reading from device...");
    saveButton.setEnabled (false);
    requestConfig(); // re-sync + rebuild confirms the values that stuck
}

void MainComponent::reloadFromDevice()
{
    if (block == nullptr)
    {
        setStatus ("Not connected.");
        return;
    }
    setStatus ("Reloading from device...");
    requestConfig();
}

// ROLI's firmware download URLs are keyed by a product *name*. We only map names we've
// confirmed resolve on my.roli.com (Seaboard Block). Others (Piano/LUMI/Lightpad/GRAND/RISE 2)
// aren't confirmed yet — notably "Lightpad Block" 404s — so they stay unmapped for now.
juce::String MainComponent::roliProductName() const
{
    if (block != nullptr && block->getType() == roli::Block::Type::seaboardBlock)
        return "Seaboard Block";
    if (rise2Page != nullptr)
        return rise2Page->firmwareDownloadProduct();   // "Seaboard RISE" for an original RISE
    return {};
}

// Download-only, never flashes. Opens ROLI's version-robust "update_to" URL in the user's
// browser; their my.roli.com login authenticates it and the latest firmware (.syx.zip) downloads.
// GRAB holds no credentials — the browser session does the auth.
void MainComponent::downloadFirmware()
{
    const auto product = roliProductName();
    if (product.isEmpty())
    {
        // Explain why rather than doing nothing (the menu item is always enabled).
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Download firmware for archiving")
                .withMessage (grab::u8 ("GRAB doesn't have a confirmed firmware download for this device yet.\n\n"
                              "Confirmed: Seaboard Block, and every Seaboard RISE (original 25 & 49, and RISE 2).\n\n"
                              "The Seaboard GRAND is registered with ROLI but has no downloadable firmware build. "
                              "LUMI Keys, Lightpad and ROLI Piano aren't mapped yet.\n\n"
                              "You can still check my.roli.com in a browser, or use Advanced -> Help add device to help."))
                .withButton ("OK"),
            nullptr);
        return;
    }

    // Name-based + any version -> ROLI resolves to the latest build (no hard-coded versionId).
    const auto url = "https://my.roli.com/firmware_versions/update_to/"
                   + juce::URL::addEscapeChars (product, false) + "/1.0.0";  // any version -> ROLI returns latest

    auto opts = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::InfoIcon)
        .withTitle ("Download firmware for archiving")
        .withMessage ("This opens ROLI's download page for your " + product + " in your web browser.\n\n"
                      "You must be signed in to your ROLI account (my.roli.com). It downloads the latest "
                      "firmware as a .syx.zip so you can archive it while ROLI's servers are still up.\n\n"
                      + grab::u8 ("GRAB does NOT flash firmware \xe2\x80\x94 this is download-only."))
        .withButton ("Open in browser")   // button 1: Enter / returns 1
        .withButton ("Cancel");           // button 2: Escape / returns 0
    juce::AlertWindow::showAsync (opts, [url] (int result)
    {
        if (result == 1)
            juce::URL (url).launchInDefaultBrowser();
    });
}

void MainComponent::factoryReset()
{
    if (block == nullptr)
        return;

    // Reset every block in the DNA cluster (they're shown as one device). Uses the library's
    // block->factoryReset(): it restores config defaults and reboots — settings only, firmware
    // untouched, recoverable. Blocks only; the RISE 0x78 reset is unknown and not offered here.
    auto targets = writeTargets();
    const int n = (int) targets.size();
    const juce::String name = blockBox.getText().isNotEmpty() ? blockBox.getText() : juce::String ("this device");

    // JUCE's 2-button AlertWindow gives the FIRST button Enter + return value 1, and the
    // SECOND Escape + return value 0 (and Escape always returns 0). So to keep the panic key
    // (Escape) safe we must make Cancel the second button; the reset fires only on result 1.
    auto opts = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::WarningIcon)
        .withTitle ("Factory reset (presets only)")
        .withMessage ("Reset " + name + (n > 1 ? "  (" + juce::String (n) + " chained blocks)" : juce::String())
                      + juce::String::fromUTF8 (
                            " to factory defaults?\n\n"
                            "This restores every setting to its default and reboots the device. "
                            "It can't be undone \xe2\x80\x94 but it only touches settings; firmware is left alone."))
        .withButton ("Factory reset")     // button 1: the action (Enter); Escape/Cancel both abort
        .withButton ("Cancel");           // button 2: returns 0, bound to Escape

    juce::Component::SafePointer<MainComponent> sp (this);
    juce::AlertWindow::showAsync (opts, [sp, targets, n] (int result)
    {
        if (sp == nullptr || result != 1)   // 1 = "Factory reset"; 0 = Cancel/Escape
            return;

        for (auto& t : targets)
            if (t != nullptr)
                t->factoryReset();

        // Drop any unsaved edits; the device reboots and we re-read fresh defaults on reconnect.
        sp->dirtyWrites.clear();
        sp->liveWriter.cancel();
        sp->pendingValues = sp->deviceValues;

        // The device will drop off the bus and reboot. Show a clear "resetting" overlay so the
        // empty UI isn't mistaken for a disconnect; cleared when config comes back (rebuildRows).
        sp->resettingSinceMs = juce::Time::getMillisecondCounterHiRes();
        sp->busyLabel.setText (grab::u8 ("Resetting \xe2\x80\x94 please wait for the device to reboot..."),
                               juce::dontSendNotification);
        sp->busyLabel.setVisible (true);
        sp->busyLabel.toFront (false);
        sp->setStatus ("Resetting " + juce::String (n) + grab::u8 (" block(s) \xe2\x80\x94 please wait for the device to reboot..."));
        sp->logger->line ("FACTORY RESET sent to " + juce::String (n) + " block(s) (user-confirmed).");
    });
}

void MainComponent::savePreset()   // Export to file...
{
    if (rows.empty())
        return;

    chooser = std::make_unique<juce::FileChooser> ("Export preset", juce::File(), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("tool", "GRAB");
            if (block != nullptr)
            {
                obj->setProperty ("serial", block->serialNumber);
                obj->setProperty ("firmware", block->versionNumber);
            }
            auto* values = new juce::DynamicObject();
            for (auto& kv : pendingValues) // all items incl. the 5D curve dimensions
                values->setProperty (juce::String ((int) kv.first), kv.second);
            obj->setProperty ("values", juce::var (values));

            file.replaceWithText (juce::JSON::toString (juce::var (obj)));
            setStatus ("Preset exported: " + file.getFileName());
        });
}

void MainComponent::loadPreset()   // Import from file...
{
    if (rows.empty())
    {
        setStatus ("Connect and load config before importing a preset.");
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Import preset", juce::File(), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file != juce::File())
                applyUserPreset (file);   // same load + apply path as a saved preset
        });
}

//==============================================================================
// Built-in ROLI presets for the Seaboard Block, transcribed from ROLI Dashboard's
// own .mode files (Seaboard.littlefootModes/*.mode). Values are config id -> value.
// These are factual setting bundles (not ROLI's files), applied via live-write.
namespace
{
    struct BlockPreset { const char* name; std::vector<std::pair<juce::uint32, juce::int32>> values; };

    // Fields common to the three MIDI-mode presets (everything but the mode itself).
    static std::vector<std::pair<juce::uint32, juce::int32>> withMode (juce::int32 midiMode)
    {
        std::vector<std::pair<juce::uint32, juce::int32>> v {
            { 0, 1 }, { 2, midiMode }, { 3, 48 }, { 4, 0 }, { 5, 0 }, { 6, 74 }, { 7, 0 },
            { 10, 127 }, { 11, 102 }, { 12, 127 }, { 13, 127 }, { 14, 127 },
            { 15, 0 }, { 16, 127 }, { 17, 0 }, { 18, 0 }, { 19, 0 }, { 24, 0 },
            { 30, 1 }, { 31, 1 }, { 32, 1 }   // Glide/Slide/Pressure Tracking = Last Played (1)
        };
        return v;
    }

    static const std::vector<BlockPreset>& seaboardPresets()
    {
        static const std::vector<BlockPreset> presets = {
            { "MPE",            withMode (1) },   // id 2: Multi=0, MPE=1, Single=2
            { "Multi Channel",  withMode (0) },
            { "Single Channel", withMode (2) },
            // Piano = Single Channel + Piano Mode on, with Slide/Press/Lift off. ROLI's
            // Piano.mode leaves Glide Sensitivity (id 11) untouched, so we omit it too.
            { "Piano", { { 0, 1 }, { 2, 2 }, { 3, 48 }, { 4, 0 }, { 5, 0 }, { 6, 74 }, { 7, 0 },
                         { 10, 127 }, { 12, 0 }, { 13, 0 }, { 14, 0 },
                         { 15, 0 }, { 16, 127 }, { 17, 1 }, { 18, 0 }, { 19, 0 }, { 24, 0 },
                         { 30, 1 }, { 31, 1 }, { 32, 1 } } }
        };
        return presets;
    }
}

juce::File MainComponent::presetDir() const
{
    // Scoped per device type so a Seaboard preset never shows up for (or is applied to) a LUMI.
    const juce::String type = block != nullptr ? juce::String (grab::blockTypeName (block->getType())) : "block";
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("GRAB").getChildFile ("presets").getChildFile (type);
}

void MainComponent::populatePresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    userPresetFiles.clear();

    // Built-in ROLI presets - Seaboard only for now (other Block types have their own; reports welcome).
    const bool seaboard = block != nullptr && block->getType() == roli::Block::Type::seaboardBlock;
    if (seaboard)
    {
        presetBox.addSectionHeading ("ROLI presets");
        int id = 1;
        for (auto& p : seaboardPresets())
            presetBox.addItem (p.name, id++);
    }

    // The user's own saved presets for this device type.
    auto dir = presetDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        if (! files.isEmpty())
        {
            if (seaboard) presetBox.addSeparator();
            presetBox.addSectionHeading ("My presets");
            for (auto& f : files)
            {
                presetBox.addItem (f.getFileNameWithoutExtension(),
                                   kUserPresetBase + (int) userPresetFiles.size());
                userPresetFiles.push_back (f);
            }
        }
    }

    presetBox.addSeparator();
    presetBox.addItem ("Save current as...",      kPresetSaveAs);
    presetBox.addItem ("Import from file...",     kPresetImport);
    presetBox.addItem ("Export to file...",       kPresetExport);
    presetBox.addItem ("Reveal presets folder...", kPresetReveal);

    presetBox.setSelectedId (0, juce::dontSendNotification);
    resized();
}

// Apply a set of id -> value into the UI (and, when live, queue them to write like any edit).
// Shared by the ROLI presets, the user's saved presets, and file import.
int MainComponent::applyPresetValues (const std::map<juce::uint32, juce::int32>& values, bool live)
{
    if (rows.empty())
        return 0;

    int applied = 0;
    for (auto& [id, v] : values)
    {
        if (pendingValues.find (id) == pendingValues.end())
            continue;   // only items this device actually reports
        pendingValues[id] = v;
        const bool differs = deviceValues.count (id) && deviceValues[id] != v;
        for (auto& r : rows)
            if (r->getId() == id) { r->setValue (v); r->setChangedFromDevice (differs); }
        for (auto* c : curves)
            if (c->getId() == id) { c->setSensitivity (v); c->setChanged (differs); }
        if (live && differs)
            dirtyWrites.insert (id);
        ++applied;
    }

    if (live)
        liveWriter.nudge();   // live-write flushes the changed items to the device
    updateSaveEnabled();
    return applied;
}

void MainComponent::applyBlockPreset (const juce::String& name)
{
    if (block == nullptr)
        return;

    const BlockPreset* preset = nullptr;
    for (auto& p : seaboardPresets())
        if (name == p.name) { preset = &p; break; }
    if (preset == nullptr)
        return;

    std::map<juce::uint32, juce::int32> m;
    for (auto& [id, v] : preset->values)   // later pairs override earlier (none here, but be safe)
        m[id] = v;

    const int applied = applyPresetValues (m, true);
    setStatus ("Applied ROLI preset '" + name + "' (" + juce::String (applied) + " items).");
}

void MainComponent::applyUserPreset (const juce::File& file)
{
    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    const auto* values = parsed.getProperty ("values", juce::var()).getDynamicObject();
    if (values == nullptr)
    {
        setStatus ("Preset has no 'values' object.");
        return;
    }

    std::map<juce::uint32, juce::int32> m;
    for (const auto& prop : values->getProperties())
        m[(juce::uint32) prop.name.toString().getIntValue()] = (juce::int32) (int) prop.value;

    const int applied = applyPresetValues (m, true);
    setStatus ("Applied preset '" + file.getFileNameWithoutExtension() + "' (" + juce::String (applied) + " items).");
}

void MainComponent::saveUserPresetAs()
{
    if (rows.empty())
    {
        setStatus ("Connect and load config before saving a preset.");
        return;
    }

    auto* aw = new juce::AlertWindow ("Save preset",
        "Name this preset. It's saved with GRAB and appears in the Preset menu for this device.",
        juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "My preset");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int result)
        {
            if (result == 1)
            {
                const auto name = juce::File::createLegalFileName (aw->getTextEditorContents ("name").trim());
                if (name.isNotEmpty())
                {
                    auto dir = presetDir();
                    dir.createDirectory();
                    const auto file = dir.getChildFile (name + ".json");

                    auto* obj = new juce::DynamicObject();
                    obj->setProperty ("tool", "GRAB");
                    if (block != nullptr)
                    {
                        obj->setProperty ("serial", block->serialNumber);
                        obj->setProperty ("firmware", block->versionNumber);
                    }
                    auto* values = new juce::DynamicObject();
                    for (auto& kv : pendingValues)   // all items incl. the 5D curve dimensions
                        values->setProperty (juce::String ((int) kv.first), kv.second);
                    obj->setProperty ("values", juce::var (values));
                    file.replaceWithText (juce::JSON::toString (juce::var (obj)));

                    populatePresetBox();
                    setStatus ("Preset saved: " + name);
                }
            }
            delete aw;
        }), false);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Secret dev shortcut: Ctrl+Alt+Shift+R forces a local RISE preview (synthetic data,
    // no hardware). Each press toggles the verified (RISE 2) / unverified (old RISE) view.
    // Sibling shortcuts: +P previews a synthetic ROLI Piano page, +L a Piano M / LUMI Keys,
    // +G a GRAND.
    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() && mods.isAltDown() && mods.isShiftDown())
    {
        switch (key.getKeyCode())
        {
            case 'R': forceRisePreview();  return true;
            case 'P': forcePianoPreview(); return true;
            case 'L': forceLumiPreview();  return true;
            case 'G': forceGrandPreview(); return true;
            case 'B': forceLightpadPreview(); return true;
            default:  break;
        }
    }
    return false;
}

void MainComponent::forceRisePreview()
{
    if (source != nullptr) { source->setActive (false); source.reset(); }
    block = nullptr;
    stopTimer();

    if (rise2Page == nullptr)
    {
        rise2Page = std::make_unique<Rise2Component> (*logger);
        addAndMakeVisible (*rise2Page);
        setBlocksControlsVisible (false);
    }

    keyboardView.setOctaves (risePreviewVerified ? 5 : 2);   // RISE 2 = 5 octaves, original RISE = 2
    keyboardView.setFamily (KeyboardView::Family::rise);
    keyboardView.setBlocks (1);
    keyboardView.setVisible (true);
    rise2Page->previewSyntheticState (risePreviewVerified);
    setStatus (juce::String ("RISE preview - ")
               + (risePreviewVerified ? "verified fw 2.0.2"
                                      : "original RISE fw 1.1.8 (editable; min/max hidden)")
               + ".  Ctrl+Alt+Shift+R to toggle.");
    risePreviewVerified = ! risePreviewVerified;   // next press shows the other
    resized();
}

// Build a synthetic config page for a device we may not have on hand. Metadata comes from the
// vendored library's own default table (authentic names/ranges/types/options), with the items
// a ROLI Piano reports (see PIANO.md) marked active and given representative values.
void MainComponent::forcePianoPreview()
{
    if (! simConfig.empty()) { connect (seaprobe::Transport::Auto); return; }  // toggle off

    if (source != nullptr) { source->setActive (false); source.reset(); }
    block = nullptr;
    stopTimer();
    if (rise2Page != nullptr) { removeChildComponent (rise2Page.get()); rise2Page.reset(); }
    setBlocksControlsVisible (true);
    presetBox.setVisible (false);   // presets are Seaboard-only for now

    using CT = roli::Block::ConfigMetaData::ConfigType;
    const auto mk = [] (int id, juce::int32 val, const char* name, CT type, int mn, int mx,
                        const char* group, std::initializer_list<const char*> opts)
    {
        const char* arr[roli::Block::ConfigMetaData::numOptionNames] = {};
        int i = 0; for (auto* o : opts) if (i < roli::Block::ConfigMetaData::numOptionNames) arr[i++] = o;
        return roli::Block::ConfigMetaData ((juce::uint32) id, val, { mn, mx }, true, name, type, arr, group);
    };

    // The items a Piano 49 reports (PIANO.md), with representative values. Scale (22) and the
    // tracking items get their names from applyConfigOverrides in rebuildRows.
    simConfig.clear();
    auto add = [&] (roli::Block::ConfigMetaData m, juce::int32 v) { simConfig.push_back ({ m, v }); };
    add (mk (2, 1, "MIDI Mode", CT::options, 0, 2, "MIDI Settings", { "Multi Channel", "MPE", "Single Channel" }), 1);
    add (mk (9, 15, "No. MIDI Channels", CT::integer, 1, 15, "MIDI Settings", {}), 15);
    add (mk (3, 48, "Pitch Bend Range", CT::integer, 0, 96, "Pitch", {}), 48);
    add (mk (4, 0, "Octave", CT::integer, -4, 3, "Pitch", {}), 0);
    add (mk (5, 0, "Transpose", CT::integer, -11, 11, "Pitch", {}), 0);
    add (mk (41, 2, "Key Pitch Bend Amount", CT::integer, 1, 12, "Pitch", {}), 2);
    add (mk (10, 70, "Strike Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 70);
    add (mk (11, 64, "Glide Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 64);
    add (mk (13, 90, "Pressure Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 90);
    add (mk (14, 40, "Lift Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 40);
    add (mk (15, 0, "Fixed Velocity", CT::boolean, 0, 1, "Sensitivity", {}), 0);
    add (mk (16, 100, "Fixed Velocity Value", CT::integer, 1, 127, "Sensitivity", {}), 100);
    add (mk (20, 0, "Active Mode", CT::options, 0, 3, "Play mode", { "Mode 1", "Mode 2", "Mode 3", "Mode 4" }), 0);
    add (mk (22, 6 /* Blues: shows the Scale dropdown off its default */, "Scale", CT::integer, 0, 18, "Play mode", {}), 6);
    add (mk (27, 0, "Key", CT::options, 0, 11, "Play mode",
             { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }), 0);
    add (mk (30, 2, "Pitch Bend Tracking", CT::integer, 1, 4, "Play mode", {}), 2);
    add (mk (32, 1, "Pressure Tracking", CT::options, 0, 1, "Play mode", { "Poly Aftertouch", "Channel Pressure" }), 1);
    add (mk (34, (juce::int32) 0xFF3D0707, "Global Key Color", CT::colour, 0, 0, "Colors", {}), (juce::int32) 0xFF3D0707);
    add (mk (35, (juce::int32) 0xFF0A84FF, "Root Key Color", CT::colour, 0, 0, "Colors", {}), (juce::int32) 0xFF0A84FF);
    add (mk (36, 80, "Brightness", CT::integer, 0, 100, "Colors", {}), 80);
    add (mk (47, 64, "Pedal Control Change", CT::integer, 0, 127, "Pedal", {}), 64);
    add (mk (48, 0, "Pedal Type", CT::options, 0, 1, "Pedal", { "Sustain", "Expression" }), 0);
    add (mk (100, 1 /* User: enables scale lighting */, "Color Scheme", CT::options, 0, 4, "Mode 1",
             { "Pro", "User", "Piano", "Stage", "Rainbow" }), 1);
    add (mk (102, 1, "Enable Pitch Bend", CT::boolean, 0, 1, "Mode 1", {}), 1);
    add (mk (103, 1, "Enable Pressure", CT::boolean, 0, 1, "Mode 1", {}), 1);

    keyboardView.setFamily (KeyboardView::Family::block);
    keyboardView.setOctaves (4);   // Piano 49 ≈ 4 octaves
    keyboardView.setBlocks (1);
    keyboardView.setVisible (true);
    setStatus ("ROLI Piano preview (simulated, no hardware).  Ctrl+Alt+Shift+P to exit.");
    if (logger != nullptr) logger->line ("DEV: ROLI Piano preview (" + juce::String ((int) simConfig.size()) + " synthetic items)");
    rebuildRows();
}

// Secret dev shortcut (Ctrl+Alt+Shift+L): a synthetic Piano M / LUMI Keys page. The Piano M is
// the rebranded LUMI Keys and reports the same BLOCKS config items as the ROLI Piano minus Glide
// and the pedal, with four modes' Color Scheme (29 items / 9 sections, per LUMI.md). Layout only,
// no hardware — handy for eyeballing the colour picker and the per-mode cards.
void MainComponent::forceLumiPreview()
{
    if (! simConfig.empty()) { connect (seaprobe::Transport::Auto); return; }  // toggle off

    if (source != nullptr) { source->setActive (false); source.reset(); }
    block = nullptr;
    stopTimer();
    if (rise2Page != nullptr) { removeChildComponent (rise2Page.get()); rise2Page.reset(); }
    setBlocksControlsVisible (true);
    presetBox.setVisible (false);   // presets are Seaboard-only for now

    using CT = roli::Block::ConfigMetaData::ConfigType;
    const auto mk = [] (int id, juce::int32 val, const char* name, CT type, int mn, int mx,
                        const char* group, std::initializer_list<const char*> opts)
    {
        const char* arr[roli::Block::ConfigMetaData::numOptionNames] = {};
        int i = 0; for (auto* o : opts) if (i < roli::Block::ConfigMetaData::numOptionNames) arr[i++] = o;
        return roli::Block::ConfigMetaData ((juce::uint32) id, val, { mn, mx }, true, name, type, arr, group);
    };

    simConfig.clear();
    auto add = [&] (roli::Block::ConfigMetaData m, juce::int32 v) { simConfig.push_back ({ m, v }); };

    add (mk (2, 1, "MIDI Mode", CT::options, 0, 2, "MIDI Settings", { "Multi Channel", "MPE", "Single Channel" }), 1);
    add (mk (9, 15, "No. MIDI Channels", CT::integer, 1, 15, "MIDI Settings", {}), 15);
    add (mk (3, 48, "Pitch Bend Range", CT::integer, 0, 96, "Pitch", {}), 48);
    add (mk (4, 0, "Octave", CT::integer, -4, 5, "Pitch", {}), 0);          // Piano M octave range is -4..5
    add (mk (5, 0, "Transpose", CT::integer, -11, 11, "Pitch", {}), 0);
    add (mk (41, 2, "Key Pitch Bend Amount", CT::integer, 1, 12, "Pitch", {}), 2);
    add (mk (10, 70, "Strike Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 70);  // no Glide (11) on a Piano M
    add (mk (13, 90, "Pressure Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 90);
    add (mk (14, 40, "Lift Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 40);
    add (mk (15, 0, "Fixed Velocity", CT::boolean, 0, 1, "Sensitivity", {}), 0);
    add (mk (16, 100, "Fixed Velocity Value", CT::integer, 1, 127, "Sensitivity", {}), 100);
    add (mk (20, 0, "Active Mode", CT::options, 0, 3, "Play mode", { "Mode 1", "Mode 2", "Mode 3", "Mode 4" }), 0);
    add (mk (22, 6, "Scale", CT::integer, 0, 18, "Play mode", {}), 6);
    add (mk (27, 0, "Key", CT::options, 0, 11, "Play mode",
             { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }), 0);
    add (mk (30, 2, "Pitch Bend Tracking", CT::integer, 1, 4, "Play mode", {}), 2);
    add (mk (32, 1, "Pressure Tracking", CT::options, 0, 1, "Play mode", { "Poly Aftertouch", "Channel Pressure" }), 1);
    add (mk (34, (juce::int32) 0xFF0A84FF, "Global Key Color", CT::colour, 0, 0, "Colors", {}), (juce::int32) 0xFF0A84FF);
    add (mk (35, (juce::int32) 0xFFFF2D55, "Root Key Color", CT::colour, 0, 0, "Colors", {}), (juce::int32) 0xFFFF2D55);
    add (mk (36, 80, "Brightness", CT::integer, 0, 100, "Colors", {}), 80);

    // Four modes, each with its own Color Scheme + Enable Pitch Bend / Pressure (ids +7 per mode).
    // Static group names so the const char* they're passed as stays valid for the whole session.
    static const char* const modeGroups[] = { "Mode 1", "Mode 2", "Mode 3", "Mode 4" };
    for (int m = 0; m < 4; ++m)
    {
        const int base = 100 + 7 * m;
        const char* group = modeGroups[m];
        add (mk (base,     m == 0 ? 1 : 0, "Color Scheme", CT::options, 0, 4, group,
                 { "Pro", "User", "Piano", "Stage", "Rainbow" }), m == 0 ? 1 : 0);
        add (mk (base + 2, 1, "Enable Pitch Bend", CT::boolean, 0, 1, group, {}), 1);
        add (mk (base + 3, 1, "Enable Pressure",   CT::boolean, 0, 1, group, {}), 1);
    }

    keyboardView.setFamily (KeyboardView::Family::lumi);
    keyboardView.setOctaves (2);   // Piano M / LUMI = 24 keys ≈ 2 octaves
    keyboardView.setBlocks (1);
    keyboardView.setVisible (true);
    setStatus ("Piano M / LUMI Keys preview (simulated, no hardware).  Ctrl+Alt+Shift+L to exit.");
    if (logger != nullptr) logger->line ("DEV: Piano M / LUMI preview (" + juce::String ((int) simConfig.size()) + " synthetic items)");
    rebuildRows();
    updateLumiKeyboardColours();   // light the keyboard from the synthetic scale/key/colours
}

// The GRAND is a 0x78 RISE-family device, fully supported (read + write; see RISE-GRAND.md). A real
// GRAND routes to the RISE page from its decoded 0x3a reply. This dev shortcut previews that GRAND
// page (4D, 3 pedals) from a synthetic reply, no hardware.
void MainComponent::forceGrandPreview()
{
    if (grandPreview)   // toggle off -> back to a real device
    {
        grandPreview = false;
        if (rise2Page != nullptr) { removeChildComponent (rise2Page.get()); rise2Page.reset(); }
        setBlocksControlsVisible (true);
        connect (seaprobe::Transport::Auto);
        return;
    }

    if (source != nullptr) { source->setActive (false); source.reset(); }
    block = nullptr;
    simConfig.clear();
    stopTimer();
    if (rise2Page == nullptr)
    {
        rise2Page = std::make_unique<Rise2Component> (*logger);
        addAndMakeVisible (*rise2Page);
    }
    setBlocksControlsVisible (false);
    grandPreview = true;

    keyboardView.setFamily (KeyboardView::Family::grand);
    keyboardView.setOctaves (5);   // GRAND is large
    keyboardView.setBlocks (1);
    keyboardView.setVisible (true);
    rise2Page->previewSyntheticGrand();
    if (logger != nullptr) logger->line ("DEV: Seaboard GRAND preview (RISE page, preliminary 4D/3-pedal)");
    resized();
}

// Secret dev shortcut (Ctrl+Alt+Shift+B): the Lightpad's decoded config page, plus a synthetic DNA
// arrangement (Seaboard + two Lightpad/Control columns) so the topology tiling is visible without
// hardware. Values are from the DB Lightpad profiles; the layout mimics the Dashboard screenshots.
void MainComponent::forceLightpadPreview()
{
    if (! simConfig.empty()) { connect (seaprobe::Transport::Auto); return; }  // toggle off

    if (source != nullptr) { source->setActive (false); source.reset(); }
    block = nullptr;
    stopTimer();
    if (rise2Page != nullptr) { removeChildComponent (rise2Page.get()); rise2Page.reset(); }
    setBlocksControlsVisible (true);
    presetBox.setVisible (false);   // presets are Seaboard-only for now

    using CT = roli::Block::ConfigMetaData::ConfigType;
    const auto mk = [] (int id, juce::int32 val, const char* name, CT type, int mn, int mx,
                        const char* group, std::initializer_list<const char*> opts)
    {
        const char* arr[roli::Block::ConfigMetaData::numOptionNames] = {};
        int i = 0; for (auto* o : opts) if (i < roli::Block::ConfigMetaData::numOptionNames) arr[i++] = o;
        return roli::Block::ConfigMetaData ((juce::uint32) id, val, { mn, mx }, true, name, type, arr, group);
    };

    // The 25 items a Lightpad reports (DB profile #15, fw 1.1.0). Scale (22), Chord (24) and
    // Glide/Slide Tracking (30/31) get their names from applyConfigOverrides in rebuildRows.
    simConfig.clear();
    auto add = [&] (roli::Block::ConfigMetaData m, juce::int32 v) { simConfig.push_back ({ m, v }); };
    add (mk (2, 1, "MIDI Mode", CT::options, 0, 2, "MIDI Settings", { "Multi Channel", "MPE", "Single Channel" }), 1);
    add (mk (3, 48, "Pitch Bend Range", CT::integer, 0, 96, "MIDI Settings", {}), 48);
    add (mk (9, 15, "No. MIDI Channels", CT::integer, 1, 15, "MIDI Settings", {}), 15);
    add (mk (40, 0, "MPE Zone", CT::options, 0, 1, "MIDI Settings", { "Lower Zone", "Upper Zone" }), 0);
    add (mk (4, 0, "Octave", CT::integer, -4, 6, "Pitch", {}), 0);
    add (mk (5, 0, "Transpose", CT::integer, -11, 11, "Pitch", {}), 0);
    add (mk (6, 74, "Slide CC", CT::integer, 0, 127, "Play mode", {}), 74);
    add (mk (7, 2, "Slide Mode", CT::options, 0, 2, "Play mode", { "Absolute", "Relative Unipolar", "Relative Bipolar" }), 2);
    add (mk (17, 0, "Piano Mode", CT::boolean, 0, 1, "Play mode", {}), 0);
    add (mk (18, 0, "Glide Rate", CT::integer, 0, 127, "Play mode", {}), 0);
    add (mk (19, 0, "Glide Lock Enable", CT::boolean, 0, 1, "Play mode", {}), 0);
    add (mk (20, 4, "Mode", CT::integer, 1, 5, "Play mode", {}), 4);
    add (mk (22, 0, "Scale", CT::integer, 0, 18, "Play mode", {}), 0);
    add (mk (23, 0, "Hide Mode", CT::boolean, 0, 1, "Play mode", {}), 0);
    add (mk (24, 0, "Chord", CT::integer, 0, 14, "Play mode", {}), 0);
    add (mk (30, 1, "Glide Tracking", CT::integer, 1, 4, "Play mode", {}), 1);
    add (mk (31, 1, "Slide Tracking", CT::integer, 1, 4, "Play mode", {}), 1);
    add (mk (32, 1, "Pressure Tracking", CT::options, 0, 5, "Play mode",
             { "Poly Aftertouch", "Last Played", "Highest", "Lowest", "Disabled", "Hardest" }), 1);
    add (mk (10, 50, "Strike Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 50);
    add (mk (11, 100, "Glide Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 100);
    add (mk (12, 127, "Slide Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 127);
    add (mk (13, 72, "Pressure Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 72);
    add (mk (14, 127, "Lift Sensitivity", CT::integer, 0, 127, "Sensitivity", {}), 127);
    add (mk (15, 0, "Fixed Velocity", CT::boolean, 0, 1, "Sensitivity", {}), 0);
    add (mk (16, 127, "Fixed Velocity Value", CT::integer, 1, 127, "Sensitivity", {}), 127);

    // Synthetic DNA arrangement: a Seaboard (6x3) beside two Lightpad(2x2)+Control(2x1) columns.
    std::vector<KeyboardView::Tile> tiles;
    auto tile = [] (juce::uint64 uid, KeyboardView::Family fam, float x, float y, float w, float h, int oct)
    {
        KeyboardView::Tile t; t.uid = uid; t.family = fam; t.ux = x; t.uy = y; t.uw = w; t.uh = h; t.octaves = oct; return t;
    };
    tiles.push_back (tile (101, KeyboardView::Family::block,    0, 0, 6, 3, 2));
    tiles.push_back (tile (102, KeyboardView::Family::lightpad, 6, 0, 2, 2, 0));
    tiles.push_back (tile (103, KeyboardView::Family::control,  6, 2, 2, 1, 0));
    tiles.push_back (tile (104, KeyboardView::Family::lightpad, 8, 0, 2, 2, 0));
    tiles.push_back (tile (105, KeyboardView::Family::control,  8, 2, 2, 1, 0));
    keyboardView.setLayout (tiles);
    keyboardView.setSelected ({ 102 });   // highlight the Lightpad, dim the rest (demo the selection look)
    keyboardView.setVisible (true);

    setStatus ("Lightpad + DNA arrangement preview (simulated, no hardware).  Ctrl+Alt+Shift+B to exit.");
    if (logger != nullptr) logger->line ("DEV: Lightpad preview (" + juce::String ((int) simConfig.size()) + " synthetic items, 5-tile layout)");
    rebuildRows();
    resized();
}

void MainComponent::resized()
{
    footer.setBounds (getLocalBounds().removeFromBottom (26));   // full-width strip at the very bottom
    auto r = getLocalBounds().withTrimmedBottom (26).reduced (8);
    statusLabel.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);

    // The keyboard graphic (keyboard devices only) sits above the config content.
    auto placeKeyboard = [this] (juce::Rectangle<int>& area)
    {
        if (keyboardView.isVisible())
        {
            keyboardView.setBounds (area.removeFromTop (108));   // full width; keys are a fixed scale, centred/clipped
            area.removeFromTop (8);
        }
    };

    // The RISE page owns its own reload/save/info/viewport, so when it's shown lay out
    // only the controls that still apply (Diagnose, Transport, Advanced) and hand it the
    // rest of the window. (The BLOCKS-only controls are hidden by setBlocksControlsVisible.)
    if (rise2Page != nullptr)
    {
        auto row = r.removeFromTop (30);
        diagnoseBtn.setBounds (row.removeFromLeft (100).reduced (2, 0));
        row.removeFromLeft (8);
        advancedBtn.setBounds (row.removeFromRight (92).reduced (2, 0));
        row.removeFromRight (10);
        transportLabel.setBounds (row.removeFromLeft (130));
        transportBox.setBounds (row.removeFromLeft (92).reduced (2, 0));
        r.removeFromTop (6);
        placeKeyboard (r);
        rise2Page->setBounds (r);
        return;
    }

    auto buttons = r.removeFromTop (30);
    const int bw = 165;
    reloadButton.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    saveButton  .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    buttons.removeFromLeft (8);
    presetBox.setBounds (buttons.removeFromLeft (190).reduced (2, 0));
    r.removeFromTop (6);

    auto trow = r.removeFromTop (26);
    contributeBtn.setBounds (trow.removeFromRight (100).reduced (2, 0));
    advancedBtn.setBounds (trow.removeFromRight (92).reduced (2, 0));
    trow.removeFromRight (10);
    transportLabel.setBounds (trow.removeFromLeft (130));
    transportBox.setBounds (trow.removeFromLeft (92).reduced (2, 0));
    trow.removeFromLeft (8);
    diagnoseBtn.setBounds (trow.removeFromLeft (100).reduced (2, 0));
    trow.removeFromLeft (10);
    blockLabel.setBounds (trow.removeFromLeft (84));
    blockBox.setBounds (trow.reduced (2, 0));
    r.removeFromTop (6);

    placeKeyboard (r);
    viewport.setBounds (r);
    busyLabel.setBounds (r);   // overlay centred over the content area (shown during a reset)
    layoutCardGrid (viewport, rowHolder, cards, "Sensitivity");
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
