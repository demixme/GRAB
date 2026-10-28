#include "MainComponent.h"
#include "ContributePanel.h"
#include "CapturePanel.h"
#include "ProbePanel.h"
#include "ExplorePanel.h"
#include "WelcomePanel.h"
#include "DiagnosePanel.h"
#include "Help.h"

#include <algorithm>
#include <map>

namespace
{
// Rough key-range (octaves) for the keyboard graphic, from the device's port/name.
int keyboardOctavesForName (const juce::String& name)
{
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
    if (changed)
    {
        g.setColour (juce::Colours::orange.withAlpha (0.18f));
        g.fillRect (getLocalBounds());
        g.setColour (juce::Colours::orange);
        g.fillRect (getLocalBounds().removeFromLeft (3));
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
    addAndMakeVisible (loadPresetBtn);
    addAndMakeVisible (savePresetBtn);
    reloadButton.onClick  = [this] { reloadFromDevice(); };
    saveButton.onClick    = [this] { saveToDevice(); };
    liveWriter.onTick     = [this] { flushLiveWrites(); };
    loadPresetBtn.onClick = [this] { loadPreset(); };
    savePresetBtn.onClick = [this] { savePreset(); };
    addChildComponent (presetBox);   // shown only for devices with built-in presets (Seaboard)
    presetBox.setTextWhenNothingSelected ("Apply ROLI preset...");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
        {
            applyBlockPreset (presetBox.getText());
            presetBox.setSelectedId (0, juce::dontSendNotification);   // back to the placeholder
        }
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
            });
    };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (footer);
    addChildComponent (keyboardView);   // shown only for keyboard devices

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
        // Only (re-)scan when fully idle: a non-null source means a connect is already in
        // progress or established, so leave it alone (don't interrupt it or loop).
        if (source != nullptr || block != nullptr || rise2Page != nullptr
            || ! simConfig.empty() || grandPreview)
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
        setStatus ("Seaboard RISE found.");
        rise2Page = std::make_unique<Rise2Component> (*logger);
        addAndMakeVisible (*rise2Page);
        setBlocksControlsVisible (false);

        juce::String riseName = "Seaboard RISE";
        for (auto& d : juce::MidiOutput::getAvailableDevices())
            if (rise2::isRisePort (d.name)) { riseName = d.name; break; }
        keyboardView.setOctaves (keyboardOctavesForName (riseName));
        keyboardView.setFamily (KeyboardView::Family::rise);
        keyboardView.setBlocks (1);   // a RISE is a single keybed (no DNA chaining)
        keyboardView.setVisible (true);

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
    for (auto* c : std::initializer_list<juce::Component*> { &reloadButton, &saveButton, &loadPresetBtn, &savePresetBtn,
                                                            &blockLabel, &blockBox, &contributeBtn, &viewport })
        c->setVisible (visible);
    if (! visible)
        presetBox.setVisible (false);   // on the RISE page; populatePresetBox re-shows it for a Seaboard
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

    setStatus ("Connected (" + (detector != nullptr ? detector->getActiveTransport() : juce::String())
               + "): " + juce::String (topo.blocks.size()) + " block(s)"
               + (block != nullptr ? "   -   " + block->serialNumber + " fw " + block->versionNumber
                                       + " battery " + juce::String (juce::roundToInt (block->getBatteryLevel() * 100.0f)) + "%"
                                     : juce::String()));

    if (block != nullptr)
    {
        const auto type = juce::String (grab::blockTypeName (block->getType()));
        if (learnedForType != type)
            fetchNames ("BLOCKS", type); // self-learn: pull names for this device type
    }

    if (block != nullptr && (blockChanged || ! configRequested))
        requestConfig();
}

void MainComponent::updateKeyboardForBlock()
{
    const bool isSeaboard = block != nullptr && block->getType() == roli::Block::Type::seaboardBlock;
    if (isSeaboard)
    {
        auto it = clusterMembers.find (selectedBlockUid);
        const std::vector<roli::Block::UID> members = (it != clusterMembers.end()) ? it->second
                                                                                   : std::vector<roli::Block::UID> { selectedBlockUid };
        keyboardView.setOctaves (2);                       // a Seaboard Block is 2 octaves
        keyboardView.setFamily  (KeyboardView::Family::block);
        keyboardView.setBlocks  (juce::jmax (1, (int) members.size())); // chained Seaboards draw as one
        keyboardView.setVisible (true);
        setTouchListening (members);                       // live 5D-touch rings
    }
    else
    {
        keyboardView.setVisible (false);
        setTouchListening ({});
    }

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

    const float nx = juce::jlimit (0.0f, 1.0f, t.x / g.w);
    const float ny = juce::jlimit (0.0f, 1.0f, t.y / g.h);
    const float cx = ((float) g.index + nx) / (float) g.count;   // across the whole chain
    const juce::uint64 id = t.blockUID ^ ((juce::uint64) (juce::uint32) t.index * 0x9E3779B97F4A7C15ULL);
    keyboardView.setTouch (id, cx, ny, t.z, t.isTouchEnd);
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

    const bool isGrand   = name.containsIgnoreCase ("grand");
    const bool riseGrand = isGrand || name.containsIgnoreCase ("rise");
    const auto path = logFolder.getFullPathName();

    if (isGrand)
    {
        // The GRAND speaks the RISE 0x78 dialect but doesn't return its settings in the identify
        // reply, so we can't configure it yet — we need a capture. Make sure the owner sees how.
        setStatus ("Seaboard GRAND detected! It speaks the RISE protocol and we're close to supporting "
                   "it - we just need a capture of its settings. See 'How you can help' (opening now, "
                   "also in the Advanced menu), or click 'Help add device...' to send a report.");
        if (! grandHelpShown)
        {
            grandHelpShown = true;
            juce::Component::SafePointer<MainComponent> sp (this);
            juce::MessageManager::callAsync ([sp] { if (sp != nullptr) sp->showHelpWanted(); });
        }
    }
    else if (riseGrand)
        setStatus ("Found '" + name + "' - looks like a Seaboard RISE. If its settings page didn't open, "
                   "try another Connection Type, or click 'Help add device...'.");
    else
        setStatus ("Opened '" + name + "' but no ROLI BLOCKS device replied. It may be an unsupported "
                   "device, or the port is held by another app - click 'Diagnose...', or try 'Help add device...'");

    // Full guidance (incl. the log path to attach to a report) goes to the log.
    logger->line ("CONNECT TIMEOUT: opened '" + name + "' but received no BLOCKS topology in 6s."
                  + juce::String (riseGrand ? "  Name looks like RISE/GRAND (unsupported protocol)." : ""));
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
    setStatus (statusLabel.getText() + "   -   " + juce::String ((int) rows.size())
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
                              "You can still check my.roli.com in a browser, or use \"Help add device...\" to help."))
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

void MainComponent::savePreset()
{
    if (rows.empty())
        return;

    chooser = std::make_unique<juce::FileChooser> ("Save preset", juce::File(), "*.json");
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
            setStatus ("Preset saved: " + file.getFileName());
        });
}

void MainComponent::loadPreset()
{
    if (rows.empty())
    {
        setStatus ("Connect and load config before applying a preset.");
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Load preset", juce::File(), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            const auto parsed = juce::JSON::parse (file.loadFileAsString());
            const auto* values = parsed.getProperty ("values", juce::var()).getDynamicObject();
            if (values == nullptr)
            {
                setStatus ("Preset has no 'values' object.");
                return;
            }

            int applied = 0;
            for (const auto& prop : values->getProperties())
            {
                const auto id = (juce::uint32) prop.name.toString().getIntValue();
                if (pendingValues.find (id) == pendingValues.end())
                    continue; // only items this device actually has
                const auto v = (juce::int32) (int) prop.value;
                pendingValues[id] = v;
                const bool differs = deviceValues.count (id) && deviceValues[id] != v;
                for (auto& r : rows)
                    if (r->getId() == id) { r->setValue (v); r->setChangedFromDevice (differs); }
                for (auto* c : curves)
                    if (c->getId() == id) { c->setSensitivity (v); c->setChanged (differs); }
                ++applied;
            }

            updateSaveEnabled();
            setStatus ("Preset loaded (" + juce::String (applied) + " items). Review, then Save to device.");
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

void MainComponent::populatePresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    // Seaboard only for now — other Block types have their own presets (reports welcome).
    const bool seaboard = block != nullptr && block->getType() == roli::Block::Type::seaboardBlock;
    if (seaboard)
    {
        int id = 1;
        for (auto& p : seaboardPresets())
            presetBox.addItem (p.name, id++);
    }
    presetBox.setSelectedId (0, juce::dontSendNotification);
    presetBox.setVisible (seaboard);
    resized();
}

void MainComponent::applyBlockPreset (const juce::String& name)
{
    if (block == nullptr || rows.empty())
        return;

    const BlockPreset* preset = nullptr;
    for (auto& p : seaboardPresets())
        if (name == p.name) { preset = &p; break; }
    if (preset == nullptr)
        return;

    int applied = 0;
    for (auto& [id, v] : preset->values)
    {
        if (pendingValues.find (id) == pendingValues.end())
            continue;   // only items this device actually reports
        pendingValues[id] = v;
        const bool differs = deviceValues.count (id) && deviceValues[id] != v;
        for (auto& r : rows)
            if (r->getId() == id) { r->setValue (v); r->setChangedFromDevice (differs); }
        for (auto* c : curves)
            if (c->getId() == id) { c->setSensitivity (v); c->setChanged (differs); }
        if (differs)
            dirtyWrites.insert (id);
        ++applied;
    }

    liveWriter.nudge();          // live-write flushes the changed items to the device
    updateSaveEnabled();
    setStatus ("Applied ROLI preset '" + name + "' (" + juce::String (applied) + " items).");
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Secret dev shortcut: Ctrl+Alt+Shift+R forces a local RISE preview (synthetic data,
    // no hardware). Each press toggles the verified (RISE 2) / unverified (old RISE) view.
    // Sibling shortcuts: +P previews a synthetic ROLI Piano page, +G an identity-only GRAND.
    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() && mods.isAltDown() && mods.isShiftDown())
    {
        switch (key.getKeyCode())
        {
            case 'R': forceRisePreview();  return true;
            case 'P': forcePianoPreview(); return true;
            case 'G': forceGrandPreview(); return true;
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

// The GRAND is a 0x78 RISE-family device, but its settings are NOT in its identify reply and
// are still unmapped (see RISE-GRAND.md), so there is no config page to show — this previews
// the provisional config page (4D, 3 pedals) on the RISE component — layout only, not yet
// wired to real GRAND hardware (its 87-byte reply layout is still being decoded).
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
    const int bw = 137;
    reloadButton .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    saveButton   .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    loadPresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    savePresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    if (presetBox.isVisible())
    {
        buttons.removeFromLeft (8);
        presetBox.setBounds (buttons.removeFromLeft (170).reduced (2, 0));
    }
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
