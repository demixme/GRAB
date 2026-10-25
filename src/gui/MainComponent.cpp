#include "MainComponent.h"
#include "ContributePanel.h"

#include <algorithm>
#include <map>

//==============================================================================
ConfigRow::ConfigRow (const roli::Block::ConfigMetaData& meta, juce::int32 value, ChangeFn onChange)
    : id (meta.item), type (meta.type), range (meta.range), change (std::move (onChange))
{
    nameLabel.setText (meta.name, juce::dontSendNotification);
    nameLabel.setTooltip ("id " + juce::String ((int) id) + "  (" + meta.group + ")");
    addAndMakeVisible (nameLabel);

    switch (type)
    {
        case ConfigType::boolean:
            toggle.setToggleState (value != 0, juce::dontSendNotification);
            toggle.onClick = [this] { fireChange(); };
            control = &toggle;
            break;

        case ConfigType::options:
            for (int i = 0; i < roli::Block::ConfigMetaData::numOptionNames; ++i)
                if (meta.optionNames[i].isNotEmpty())
                    combo.addItem (meta.optionNames[i], i + 1); // itemId = optionIndex + 1
            combo.setSelectedId ((value - range.getStart()) + 1, juce::dontSendNotification);
            combo.onChange = [this] { fireChange(); };
            control = &combo;
            break;

        case ConfigType::integer:
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            slider.setRange ((double) range.getStart(), (double) range.getEnd(), 1.0);
            slider.setValue ((double) value, juce::dontSendNotification);
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
    nameLabel.setBounds (r.removeFromLeft (200));
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

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    setSize (720, 680);

    const auto logFile = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("logs")
                             .getChildFile ("GRAB-"
                                 + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".log");
    logger = std::make_unique<seaprobe::RawLogger> (logFile);

    connect (seaprobe::Transport::Auto);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (source != nullptr)
    {
        source->removeListener (this);
        source->setActive (false);
    }
}

void MainComponent::connect (seaprobe::Transport transport)
{
    // Tear down any existing session.
    stopTimer();
    if (source != nullptr)
    {
        source->removeListener (this);
        source->setActive (false);
        source.reset();
    }
    detector.reset();
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
    saveButton.setEnabled (false);

    setStatus ("Scanning (" + juce::String (seaprobe::transportName (transport)) + ")...");

    detector = std::make_unique<seaprobe::LoggingDeviceDetector> (*logger, transport);

    if (detector->getSelectedPairs().empty())
    {
        setStatus ("No " + juce::String (seaprobe::transportName (transport)) + " Seaboard found. "
                   "Check the cable / Bluetooth, and that ROLI Dashboard isn't holding the port.");
        return;
    }

    setStatus ("Connecting over " + detector->getActiveTransport() + "...");
    source = std::make_unique<roli::PhysicalTopologySource> (*detector);
    source->addListener (this);
    source->setActive (true);
    startTimer (250);
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
        setStatus ("Seaboard disconnected.");
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

void MainComponent::contribute()
{
    if (block == nullptr)
    {
        setStatus ("Connect a device first, then Contribute.");
        return;
    }

    auto* panel = new ContributePanel (block, detector != nullptr ? detector->getActiveTransport()
                                                                   : juce::String ("unknown"));
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (panel);
    o.dialogTitle = "Contribute device profile";
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
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
    if (block != nullptr && configRequested && ! rowsBuilt
        && juce::Time::getMillisecondCounterHiRes() - configRequestedAtMs > 2500.0)
    {
        rebuildRows();
        rowsBuilt = true;
    }
}

void MainComponent::rebuildRows()
{
    rows.clear();
    curveRows.clear();
    curves.clear();
    sectionHeaders.clear();
    layoutOrder.clear();
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
        { 10, "Strike", 0xff4fc3f7, 50, 51 }, { 11, "Glide", 0xff66d36e, -1, -1 },
        { 12, "Slide",  0xffe15fd0, -1, -1 }, { 13, "Press", 0xff4fd6c9, 52, 53 },
        { 14, "Lift",   0xffe6c24a, 54, 55 } };

    auto emitRow = [&] (const Item& item)
    {
        auto row = std::make_unique<ConfigRow> (item.meta, item.value, onChange);
        rowHolder.addAndMakeVisible (*row);
        layoutOrder.push_back ({ row.get(), rowHeight });
        rows.push_back (std::move (row));
    };

    for (const auto& g : groupOrder)
    {
        auto header = std::make_unique<juce::Label>();
        header->setText (g.isEmpty() ? "Other" : g, juce::dontSendNotification);
        header->setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        header->setColour (juce::Label::textColourId, juce::Colour (0xff6fd3d6));
        header->setColour (juce::Label::backgroundColourId, juce::Colour (0x22ffffff));
        header->setJustificationType (juce::Justification::centredLeft);
        rowHolder.addAndMakeVisible (*header);
        layoutOrder.push_back ({ header.get(), sectionHeaderHeight });
        sectionHeaders.push_back (std::move (header));

        if (g == "Sensitivity")
        {
            // 5D curves across the top of the section; remaining items (min/max,
            // fixed velocity) as rows below.
            auto cr = std::make_unique<CurveRow>();
            for (const auto& d : dims)
            {
                if (deviceValues.find (d.id) == deviceValues.end())
                    continue;
                const int mn = (d.minId >= 0 && deviceValues.count ((juce::uint32) d.minId)) ? deviceValues[(juce::uint32) d.minId] : 0;
                const int mx = (d.maxId >= 0 && deviceValues.count ((juce::uint32) d.maxId)) ? deviceValues[(juce::uint32) d.maxId] : 127;
                auto* c = cr->add (std::make_unique<CurveComponent> (d.label, juce::Colour (d.colour),
                                        d.id, deviceValues[d.id], mn, mx, onChange));
                curves.push_back (c);
            }
            if (! cr->curves.empty())
            {
                rowHolder.addAndMakeVisible (*cr);
                layoutOrder.push_back ({ cr.get(), 130 });
                curveRows.push_back (std::move (cr));
            }
            for (const auto& item : groups[g])
                if (item.meta.item < 10 || item.meta.item > 14) // not a curve dimension
                    emitRow (item);
        }
        else
        {
            for (const auto& item : groups[g])
                emitRow (item);
        }
    }

    resized(); // positions the sections/rows and sizes rowHolder
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

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (8);
    statusLabel.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);

    auto buttons = r.removeFromTop (30);
    const int bw = 165;
    reloadButton .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    saveButton   .setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    loadPresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    savePresetBtn.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (6);

    auto trow = r.removeFromTop (26);
    contributeBtn.setBounds (trow.removeFromRight (120).reduced (2, 0));
    trow.removeFromRight (10);
    transportLabel.setBounds (trow.removeFromLeft (72));
    transportBox.setBounds (trow.removeFromLeft (110).reduced (2, 0));
    trow.removeFromLeft (12);
    blockLabel.setBounds (trow.removeFromLeft (90));
    blockBox.setBounds (trow.reduced (2, 0));
    r.removeFromTop (6);

    viewport.setBounds (r);
    const int w = juce::jmax (200, viewport.getWidth() - 12);
    int total = 0;
    for (auto& item : layoutOrder) total += item.second;
    rowHolder.setSize (w, total);
    int y = 0;
    for (auto& item : layoutOrder)
    {
        item.first->setBounds (0, y, w, item.second);
        y += item.second;
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
