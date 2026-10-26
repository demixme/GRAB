#include "MainComponent.h"
#include "ContributePanel.h"
#include "CapturePanel.h"
#include "ProbePanel.h"
#include "ExplorePanel.h"
#include "Help.h"

#include <algorithm>
#include <map>

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
    if (e.mods.isPopupMenu())   // right-click / ctrl-click: revert to the device value
    {
        setValue (initialValue);
        fireChange();
    }
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

    addAndMakeVisible (helpAddBtn);
    helpAddBtn.onClick = [this] { openDialog (new ExplorePanel(), "Help add this device"); };
    addAndMakeVisible (reloadButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadPresetBtn);
    addAndMakeVisible (savePresetBtn);
    reloadButton.onClick  = [this] { reloadFromDevice(); };
    saveButton.onClick    = [this] { saveToDevice(); };
    loadPresetBtn.onClick = [this] { loadPreset(); };
    savePresetBtn.onClick = [this] { savePreset(); };
    saveButton.setEnabled (false);

    addAndMakeVisible (transportLabel);
    transportLabel.setText ("Transport:", juce::dontSendNotification);
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
        m.addItem (1, "Raw MIDI capture...");
        m.addItem (2, "Active probe...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (advancedBtn),
            [this] (int r)
            {
                if      (r == 1) openDialog (new CapturePanel(), "Raw MIDI capture");
                else if (r == 2) openDialog (new ProbePanel(),  "Active probe");
            });
    };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (footer);

    setSize (920, 810);   // tall enough for the RISE page (its columns run longest)
    setWantsKeyboardFocus (true);   // so the secret RISE-preview shortcut reaches keyPressed
    // Grab focus once the window is on screen, so the shortcut works without clicking first.
    juce::Component::SafePointer<MainComponent> sp (this);
    juce::MessageManager::callAsync ([sp] { if (sp != nullptr) sp->grabKeyboardFocus(); });

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

    deviceList = juce::MidiDeviceListConnection::make ([this]
    {
        if (source == nullptr && rise2Page == nullptr && Rise2Component::isConnected())
            connect (seaprobe::Transport::Auto);
    });
}

MainComponent::~MainComponent()
{
    stopTimer();
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

    // Tear down any existing session.
    stopTimer();
    setListeningBlock (nullptr);
    if (source != nullptr)
    {
        source->removeListener (this);
        source->setActive (false);
        source.reset();
    }
    detector.reset();
    rise2Page.reset();
    setBlocksControlsVisible (true);
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
        resized();
        return;
    }

    if (detector->getSelectedPairs().empty())
    {
        setStatus ("No " + juce::String (seaprobe::transportName (transport)) + " Seaboard found. "
                   "Check the cable / Bluetooth, and that ROLI Dashboard isn't holding the port.");
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
        saveButton.setEnabled (false);
        if (deviceSeen)
            setStatus ("Seaboard disconnected.");
        // else: we never got a device on this connection — keep the "Connecting…"
        // status; checkConnectTimeout() will give specific guidance if it stays empty.
        return;
    }

    updateBlockList (topo);
}

void MainComponent::updateBlockList (const roli::BlockTopology& topo)
{
    blockOrder.clear();
    blockBox.clear (juce::dontSendNotification);

    int selectId = 0;
    for (int i = 0; i < topo.blocks.size(); ++i)
    {
        const auto b = topo.blocks[i];
        blockOrder.push_back (b->uid);
        blockBox.addItem (b->getDeviceDescription() + "  " + b->serialNumber, i + 1);
        if (b->uid == selectedBlockUid)
            selectId = i + 1;
    }

    if (selectId == 0) // first time, or previously-selected block is gone
    {
        selectId = 1;
        selectedBlockUid = topo.blocks.getFirst()->uid;
    }
    blockBox.setSelectedId (selectId, juce::dontSendNotification);

    const bool blockChanged = (block == nullptr) || (block->uid != selectedBlockUid);
    block = topo.getBlockWithUID (selectedBlockUid);
    setListeningBlock (block); // live config updates for the selected block
    deviceSeen = true;             // a real BLOCKS device responded; disarm the connect timeout
    connectTimeoutShown = false;

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

void MainComponent::selectBlock (roli::Block::UID uid)
{
    selectedBlockUid = uid;
    if (source == nullptr)
        return;

    block = source->getCurrentTopology().getBlockWithUID (uid);
    setListeningBlock (block);

    // Reset per-block config state and re-read for the newly selected block.
    rows.clear();
    curveRows.clear();
    curves.clear();
    sectionHeaders.clear();
    layoutOrder.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();
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

    const bool riseGrand = name.containsIgnoreCase ("rise") || name.containsIgnoreCase ("grand");
    const auto path = logFolder.getFullPathName();

    if (riseGrand)
        setStatus ("Found '" + name + "' but it isn't a BLOCKS device. The Seaboard GRAND uses a "
                   "different protocol GRAB doesn't support yet — click 'Help add device…' to pitch in.");
    else
        setStatus ("Opened '" + name + "' but no ROLI BLOCKS device replied. It may be an unsupported "
                   "device, or the port is held by another app — try the other Transport, or 'Help add device…'");

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
    for (auto* c : curves) if (c->getId() == id) return true;
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
    cards.clear();          // un-parents the rows/curves it holds
    rows.clear();
    curveRows.clear();
    curves.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();

    if (block == nullptr)
        return;

    const auto onChange = [this] (juce::uint32 id, juce::int32 v) { onItemChanged (id, v); };

    // Collect active items grouped by their device-reported group, preserving id
    // order within a group and first-seen order of groups.
    struct Item { roli::Block::ConfigMetaData meta; juce::int32 value; };
    std::vector<juce::String> groupOrder;
    std::map<juce::String, std::vector<Item>> groups;

    const auto maxId = block->getMaxConfigIndex();
    for (juce::uint32 id = 0; id < maxId; ++id)
    {
        auto meta = block->getLocalConfigMetaData (id);
        if (! meta.isActive)            // only items the device actually reports
            continue;
        seaprobe::applyConfigOverrides (meta);        // built-in overrides (e.g. Chord)
        grab::applyLearnedNames (meta, learnedNames);  // names pulled from the store

        const auto value = block->getLocalConfigValue (id);
        deviceValues[id] = value;
        pendingValues[id] = value;

        auto g = meta.group;
        if (std::find (groupOrder.begin(), groupOrder.end(), g) == groupOrder.end())
            groupOrder.push_back (g);
        groups[g].push_back ({ meta, value });
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

    int written = 0;
    for (auto& kv : pendingValues)
    {
        auto it = deviceValues.find (kv.first);
        if (it != deviceValues.end() && it->second != kv.second)
        {
            block->setLocalConfigValue (kv.first, kv.second); // setConfig write (persists on fw 1.4.3)
            ++written;
        }
    }

    setStatus ("Wrote " + juce::String (written) + " item(s); re-reading from device...");
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

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Secret dev shortcut: Ctrl+Alt+Shift+R forces a local RISE preview (synthetic data,
    // no hardware). Each press toggles the verified (RISE 2) / unverified (old RISE) view.
    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() && mods.isAltDown() && mods.isShiftDown() && key.getKeyCode() == 'R')
    {
        forceRisePreview();
        return true;
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

    rise2Page->previewSyntheticState (risePreviewVerified);
    setStatus (juce::String ("RISE preview - ")
               + (risePreviewVerified ? "verified fw 2.0.2 (editable)"
                                      : "original RISE fw 1.1.8 (read-only)")
               + ".  Ctrl+Alt+Shift+R to toggle.");
    risePreviewVerified = ! risePreviewVerified;   // next press shows the other
    resized();
}

void MainComponent::resized()
{
    footer.setBounds (getLocalBounds().removeFromBottom (26));   // full-width strip at the very bottom
    auto r = getLocalBounds().withTrimmedBottom (26).reduced (8);
    statusLabel.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);

    // The RISE page owns its own reload/save/info/viewport, so when it's shown lay out
    // only the controls that still apply (Help add, Transport, Advanced) and hand it the
    // rest of the window. (The BLOCKS-only controls are hidden by setBlocksControlsVisible.)
    if (rise2Page != nullptr)
    {
        auto row = r.removeFromTop (30);
        helpAddBtn.setBounds (row.removeFromLeft (150).reduced (2, 0));
        row.removeFromLeft (8);
        advancedBtn.setBounds (row.removeFromRight (92).reduced (2, 0));
        row.removeFromRight (10);
        transportLabel.setBounds (row.removeFromLeft (66));
        transportBox.setBounds (row.removeFromLeft (92).reduced (2, 0));
        r.removeFromTop (6);
        rise2Page->setBounds (r);
        return;
    }

    auto buttons = r.removeFromTop (30);
    // "Help add device..." sits first (the one-click action for unsupported devices).
    helpAddBtn.setBounds (buttons.removeFromLeft (150).reduced (2, 0));
    buttons.removeFromLeft (8);
    const int bw = 137;
    reloadButton .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    saveButton   .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    loadPresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    savePresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (6);

    auto trow = r.removeFromTop (26);
    contributeBtn.setBounds (trow.removeFromRight (100).reduced (2, 0));
    advancedBtn.setBounds (trow.removeFromRight (92).reduced (2, 0));
    trow.removeFromRight (10);
    transportLabel.setBounds (trow.removeFromLeft (66));
    transportBox.setBounds (trow.removeFromLeft (92).reduced (2, 0));
    trow.removeFromLeft (10);
    blockLabel.setBounds (trow.removeFromLeft (84));
    blockBox.setBounds (trow.reduced (2, 0));
    r.removeFromTop (6);

    viewport.setBounds (r);
    layoutCardGrid (viewport, rowHolder, cards, "Sensitivity");
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
