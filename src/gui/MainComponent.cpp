#include "MainComponent.h"

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
    blockLabel.setText ("Block:", juce::dontSendNotification);
    blockLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (blockBox);
    blockBox.onChange = [this]
    {
        const int idx = blockBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) blockOrder.size())
            selectBlock (blockOrder[(size_t) idx]);
    };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    setSize (720, 680);

    const auto logFile = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("logs")
                             .getChildFile ("SeaboardConfig-"
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
    blockOrder.clear();
    blockBox.clear (juce::dontSendNotification);
    rows.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
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
        rowHolder.removeAllChildren();
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
    rowHolder.removeAllChildren();
    deviceValues.clear();
    configRequested = false;
    rowsBuilt = false;
    saveButton.setEnabled (false);

    if (block != nullptr)
    {
        setStatus ("Reading " + block->serialNumber + "...");
        requestConfig();
    }
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
    rowHolder.removeAllChildren();
    deviceValues.clear();

    if (block == nullptr)
        return;

    const auto maxId = block->getMaxConfigIndex();
    for (juce::uint32 id = 0; id < maxId; ++id)
    {
        const auto meta = block->getLocalConfigMetaData (id);
        if (! meta.isActive)            // only items the device actually reports
            continue;

        const auto value = block->getLocalConfigValue (id);
        deviceValues[id] = value;

        auto row = std::make_unique<ConfigRow> (meta, value,
            [this] (juce::uint32 changedId, juce::int32 v)
            {
                auto it = deviceValues.find (changedId);
                const bool differs = (it != deviceValues.end() && it->second != v);
                for (auto& r : rows)
                    if (r->getId() == changedId)
                        r->setChangedFromDevice (differs);
                // Enable Save if any row differs from the device.
                bool anyChanged = false;
                for (auto& r : rows)
                {
                    auto di = deviceValues.find (r->getId());
                    if (di != deviceValues.end() && di->second != r->getValue())
                        anyChanged = true;
                }
                saveButton.setEnabled (anyChanged);
            });

        rowHolder.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }

    rowHolder.setSize (viewport.getWidth() > 0 ? viewport.getWidth() - 12 : 540,
                       (int) rows.size() * rowHeight);
    resized();
    saveButton.setEnabled (false);
    setStatus (statusLabel.getText() + "   -   " + juce::String ((int) rows.size()) + " config items");
}

void MainComponent::saveToDevice()
{
    if (block == nullptr)
        return;

    int written = 0;
    for (auto& r : rows)
    {
        const auto id = r->getId();
        const auto v  = r->getValue();
        auto it = deviceValues.find (id);
        if (it != deviceValues.end() && it->second != v)
        {
            block->setLocalConfigValue (id, v); // setConfig write (persists on fw 1.4.3)
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
            obj->setProperty ("tool", "SeaboardConfig");
            if (block != nullptr)
            {
                obj->setProperty ("serial", block->serialNumber);
                obj->setProperty ("firmware", block->versionNumber);
            }
            auto* values = new juce::DynamicObject();
            for (auto& r : rows)
                values->setProperty (juce::String ((int) r->getId()), r->getValue());
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
            for (auto& r : rows)
            {
                const auto key = juce::String ((int) r->getId());
                if (values->hasProperty (key))
                {
                    const auto v = (juce::int32) (int) values->getProperty (key);
                    r->setValue (v);
                    auto it = deviceValues.find (r->getId());
                    r->setChangedFromDevice (it != deviceValues.end() && it->second != v);
                    ++applied;
                }
            }

            // Enable Save if the loaded preset differs from the device anywhere.
            bool anyChanged = false;
            for (auto& r : rows)
            {
                auto di = deviceValues.find (r->getId());
                if (di != deviceValues.end() && di->second != r->getValue())
                    anyChanged = true;
            }
            saveButton.setEnabled (anyChanged);
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
    transportLabel.setBounds (trow.removeFromLeft (80));
    transportBox.setBounds (trow.removeFromLeft (120).reduced (2, 0));
    trow.removeFromLeft (16);
    blockLabel.setBounds (trow.removeFromLeft (52));
    blockBox.setBounds (trow.reduced (2, 0));
    r.removeFromTop (6);

    viewport.setBounds (r);
    rowHolder.setSize (viewport.getWidth() - 12, (int) rows.size() * rowHeight);
    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, rowHolder.getWidth(), rowHeight);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
