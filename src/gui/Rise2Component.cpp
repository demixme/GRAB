#include "Rise2Component.h"
#include "MainComponent.h"

#include <functional>
#include <set>

namespace
{
using ConfigType = roli::Block::ConfigMetaData::ConfigType;
namespace off = rise2::offset;

// Control ids, local to this page.
enum : juce::uint32
{
    kMode = 1, kZone, kMembers, kChannel, kBend,
    kGlideTracking, kPressTracking, kPortForwarding, kPortThrottle,
    kOctave, kTranspose, kSemitoneLegato, kFixedVelocity, kFixedVelocityValue, kSlideCc, kSlideMode,
    kStrikeSens, kGlideSens, kSlideSens, kPressSens, kLiftSens,
    kStrikeMin, kStrikeMax, kPressMin, kPressMax, kLiftMin, kLiftMax,
    kFaderLeftCc, kFaderMiddleCc, kFaderRightCc, kXyXCc, kXyYCc, kPedalCc,
    kSleepTime
};

struct Field
{
    juce::uint32 id;
    const char* name;
    const char* group;
    ConfigType type;
    int lo, hi;
    std::vector<const char*> options;           // option i is value lo + i
    std::function<int (const rise2::State&)> read;
    std::function<rise2::Messages (int)> write; // empty for the MIDI mode group
};

std::function<int (const rise2::State&)> byte (int o, int bias = 0)
{
    return [o, bias] (const rise2::State& s) { return s.at (o) + bias; };
}

std::function<rise2::Messages (int)> param (std::uint8_t p)
{
    return [p] (int v) { return rise2::Messages { rise2::setParam (p, v) }; };
}

const std::vector<Field>& fields()
{
    using CT = ConfigType;
    static const std::vector<Field> f = {
        { kMode,     "MIDI Mode",        "MIDI Settings", CT::options, 0, 2, { "MPE", "Multi Channel", "Single Channel" },
          [] (const rise2::State& s) { return (int) s.mode(); }, {} },
        { kZone,     "MPE Zone",         "MIDI Settings", CT::options, 0, 1, { "Lower", "Upper" },
          [] (const rise2::State& s) { return s.upperZone() ? 1 : 0; }, {} },
        { kMembers,  "No. MIDI Channels", "MIDI Settings", CT::integer, 1, 15, {},
          [] (const rise2::State& s) { return s.mode() == rise2::Mode::mpe ? juce::jlimit (1, 15, s.members()) : 15; }, {} },
        { kChannel,  "MIDI Channel",     "MIDI Settings", CT::integer, 1, 16, {},
          [] (const rise2::State& s) { return s.channel(); }, {} },
        { kBend,     "Pitch Bend Range", "MIDI Settings", CT::integer, 1, 96, {}, byte (off::bendRange), {} },
        { kGlideTracking, "Glide & Slide Tracking", "MIDI Settings", CT::options, 1, 3, { "Last Played", "Highest", "Lowest" },
          byte (off::glideTracking), rise2::glideSlideTracking },
        { kPressTracking, "Press Tracking",  "MIDI Settings", CT::options, 0, 3,
          { "Poly Aftertouch", "Last Played (Channel Pressure)", "Highest", "Lowest" },
          byte (off::pressTracking), rise2::pressTracking },
        { kPortForwarding, "MIDI Port Forwarding", "MIDI Settings", CT::boolean, 0, 1, {}, byte (off::portForwarding), param (0x4d) },
        { kPortThrottle,   "MIDI Port Throttle",   "MIDI Settings", CT::options, 0, 3, { "Off", "Half", "Quarter", "Note On/Off" },
          byte (off::portThrottle), param (0x4e) },

        { kOctave,    "Octave",    "Play mode", CT::integer, -4, 4, {},   byte (off::octave, -4), rise2::octave },
        { kTranspose, "Transpose", "Play mode", CT::integer, -12, 12, {}, byte (off::transpose, -64), rise2::transpose },
        { kSemitoneLegato, "Semitone Legato", "Play mode", CT::boolean, 0, 1, {}, byte (off::semitoneLegato), param (0x4c) },
        { kSlideCc,   "Slide CC",   "Play mode", CT::integer, 0, 127, {}, byte (off::slideCc), param (0x38) },
        { kSlideMode, "Slide Mode", "Play mode", CT::options, 0, 2, { "Absolute", "Relative Unipolar", "Relative Bipolar" },
          byte (off::slideMode), param (0x39) },

        // Curves (sensitivities) are drawn as panels, these follow as rows.
        { kStrikeSens, "Strike", "Sensitivity", CT::integer, 0, 127, {}, byte (off::strikeSens), param (0x1a) },
        { kGlideSens,  "Glide",  "Sensitivity", CT::integer, 0, 127, {}, byte (off::glideSens),  param (0x17) },
        { kSlideSens,  "Slide",  "Sensitivity", CT::integer, 0, 127, {}, byte (off::slideSens),  param (0x18) },
        { kPressSens,  "Press",  "Sensitivity", CT::integer, 0, 127, {}, byte (off::pressSens),  param (0x19) },
        { kLiftSens,   "Lift",   "Sensitivity", CT::integer, 0, 127, {}, byte (off::liftSens),   param (0x1b) },
        { kFixedVelocity,      "Fixed Velocity",       "Sensitivity", CT::boolean, 0, 1, {},   byte (off::fixedVelocity), param (0x3a) },
        { kFixedVelocityValue, "Fixed Velocity Value", "Sensitivity", CT::integer, 1, 127, {}, byte (off::fixedVelocityValue), param (0x3b) },
        { kStrikeMin, "Strike Min", "Sensitivity", CT::integer, 1, 127, {}, byte (off::strikeMin), param (0x46) },
        { kStrikeMax, "Strike Max", "Sensitivity", CT::integer, 0, 127, {}, byte (off::strikeMax), param (0x47) },
        { kPressMin,  "Press Min",  "Sensitivity", CT::integer, 0, 127, {}, byte (off::pressMin),  param (0x48) },
        { kPressMax,  "Press Max",  "Sensitivity", CT::integer, 0, 127, {}, byte (off::pressMax),  param (0x49) },
        { kLiftMin,   "Lift Min",   "Sensitivity", CT::integer, 0, 127, {}, byte (off::liftMin),   param (0x4a) },
        { kLiftMax,   "Lift Max",   "Sensitivity", CT::integer, 0, 127, {}, byte (off::liftMax),   param (0x4b) },

        { kFaderLeftCc,   "Left Fader CC",   "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::faderLeftCc),   param (0x1c) },
        { kFaderMiddleCc, "Middle Fader CC", "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::faderMiddleCc), param (0x1d) },
        { kFaderRightCc,  "Right Fader CC",  "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::faderRightCc),  param (0x1e) },
        { kXyXCc,    "XY Pad X CC", "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::xyXCc),   param (0x23) },
        { kXyYCc,    "XY Pad Y CC", "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::xyYCc),   param (0x24) },
        { kPedalCc,  "Pedal CC",    "MIDI CC Assignments", CT::integer, 0, 127, {}, byte (off::pedalCc), param (0x08) },

        // Dashboard steps this by 60. The unit is not confirmed.
        { kSleepTime, "Sleep Time", "Power", CT::integer, 0, 16383, {},
          [] (const rise2::State& s) { return s.sleepTime(); }, rise2::sleepTime },
    };
    return f;
}

const Field* findField (juce::uint32 id)
{
    for (auto& f : fields())
        if (f.id == id)
            return &f;
    return nullptr;
}

bool isModeGroup (juce::uint32 id) { return id >= kMode && id <= kBend; }

struct Dim { juce::uint32 id; const char* label; juce::uint32 colour; juce::uint32 minId, maxId; };
const Dim dims[] = {
    { kStrikeSens, "Strike", 0xff4fc3f7, kStrikeMin, kStrikeMax }, { kGlideSens, "Glide", 0xff66d36e, 0, 0 },
    { kSlideSens,  "Slide",  0xffe15fd0, 0, 0 },                   { kPressSens, "Press", 0xff4fd6c9, kPressMin, kPressMax },
    { kLiftSens,   "Lift",   0xffe6c24a, kLiftMin, kLiftMax } };

bool isCurve (juce::uint32 id) { return id >= kStrikeSens && id <= kLiftSens; }

constexpr int rowHeight = 30;
constexpr int sectionHeaderHeight = 30;
} // namespace

//==============================================================================
Rise2Component::Rise2Component (seaprobe::RawLogger& loggerToUse) : logger (loggerToUse)
{
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (infoLabel);
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (reloadButton);
    addAndMakeVisible (saveButton);
    reloadButton.onClick = [this] { setStatus ("Reloading from device..."); requestState(); };
    saveButton.onClick   = [this] { saveToDevice(); };
    saveButton.setEnabled (false);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);

    openPorts();
    requestState();

    deviceList = juce::MidiDeviceListConnection::make ([this] { devicesChanged(); });
}

Rise2Component::~Rise2Component()
{
    stopTimer();
    closePorts();
}

bool Rise2Component::isConnected()
{
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (rise2::isRisePort (d.name))
            return true;
    return false;
}

void Rise2Component::openPorts()
{
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (rise2::isRisePort (d.name) && midiOut == nullptr)
        {
            midiOut = juce::MidiOutput::openDevice (d.identifier);
            portName = d.name;
        }

    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (rise2::isRisePort (d.name) && midiIn == nullptr)
            midiIn = juce::MidiInput::openDevice (d.identifier, this);

    if (midiIn != nullptr)
        midiIn->start();

    logger.line (juce::String ("RISE ports: out=") + (midiOut != nullptr ? "open" : "missing")
                 + " in=" + (midiIn != nullptr ? "open" : "missing"));
}

void Rise2Component::closePorts()
{
    if (midiIn != nullptr)
        midiIn->stop();
    midiIn.reset();
    midiOut.reset();
}

void Rise2Component::devicesChanged()
{
    const bool present = isConnected();
    const bool open = midiOut != nullptr;
    if (present == open)
        return;

    if (! present)
    {
        logger.line ("RISE ports gone");
        closePorts();
        stopTimer();
        awaitingState = false;
        expected.clear();
        saveButton.setEnabled (false);
        reloadButton.setEnabled (false);
        infoLabel.setText ({}, juce::dontSendNotification);
        setStatus ("Seaboard RISE disconnected. Waiting for it to come back...");
        return;
    }

    // A fresh state read replaces the rows, so unsaved edits from before the drop are lost here.
    // Ports opened the moment the device reappears stay silent, so give it time to start.
    logger.line ("RISE ports back");
    setStatus ("Seaboard RISE reconnected, reading settings...");
    juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<Rise2Component> (this)]
    {
        if (safe == nullptr || ! isConnected())
            return;
        safe->closePorts();
        safe->openPorts();
        safe->reloadButton.setEnabled (true);
        safe->requestState();
    });
}

void Rise2Component::send (const rise2::Messages& msgs)
{
    if (midiOut == nullptr)
        return;
    for (auto& m : msgs)
    {
        logger.midi ("OUT", m.data(), m.size(), "rise2");
        midiOut->sendMessageNow (juce::MidiMessage (m.data(), (int) m.size()));
    }
}

void Rise2Component::requestState()
{
    if (midiOut == nullptr || midiIn == nullptr)
    {
        setStatus ("Seaboard RISE MIDI port not available.");
        return;
    }
    awaitingState = true;
    stateRetries = 0;
    send ({ rise2::identify() });
    startTimer (1000);
}

void Rise2Component::timerCallback()
{
    if (! awaitingState)
    {
        stopTimer();
        return;
    }
    // Halfway through, reopen the ports in case they went stale.
    if (++stateRetries == 2)
    {
        logger.line ("No identify reply, reopening RISE ports");
        closePorts();
        openPorts();
    }
    if (stateRetries > 3)
    {
        stopTimer();
        awaitingState = false;
        setStatus ("No reply from the RISE. Check the cable and that no other app holds the port. "
                   "Older RISE models are not supported yet, use Capture... or Report... to help add them.");
        return;
    }
    send ({ rise2::identify() });
}

void Rise2Component::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    const auto* d = message.getRawData();
    const auto size = message.getRawDataSize();
    logger.midi ("IN ", d, (std::size_t) size, "rise2");

    if (! rise2::State::isIdentifyReply (d, size))
        return;

    rise2::State s;
    s.raw.assign (d, d + size);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Rise2Component> (this), s]
    {
        if (safe != nullptr)
            safe->onState (s);
    });
}

void Rise2Component::onState (rise2::State s)
{
    if (! awaitingState)
        return;
    awaitingState = false;
    stopTimer();
    state = std::move (s);

    const auto firmware = state.text (off::firmware, 8);
    infoLabel.setText (portName + "   fw " + firmware
                           + "   battery " + juce::String (state.at (off::battery)) + "/7",
                       juce::dontSendNotification);

    // The state layout is only mapped for RISE 2 firmware 2.x. Don't guess on anything else.
    if (! firmware.startsWith ("2."))
    {
        setStatus ("'" + portName + "' firmware " + firmware + " is not verified yet, so settings are not shown. "
                   "Use Capture... or Report... to help add it.");
        expected.clear();
        return;
    }

    rebuildRows();

    if (expected.empty())
    {
        setStatus ("Read " + juce::String ((int) rows.size() + (int) curves.size()) + " settings from the device.");
        return;
    }

    juce::StringArray mismatched;
    for (auto& kv : expected)
        if (deviceValues[kv.first] != kv.second)
            if (auto* f = findField (kv.first))
                mismatched.add (f->name);
    setStatus (mismatched.isEmpty() ? "Saved " + juce::String ((int) expected.size()) + " setting(s), device confirms."
                                    : "Device did not take: " + mismatched.joinIntoString (", "));
    expected.clear();
}

int Rise2Component::value (juce::uint32 id) const
{
    if (auto p = pendingValues.find (id); p != pendingValues.end()) return p->second;
    if (auto d = deviceValues.find (id); d != deviceValues.end())   return d->second;
    return 0;
}

void Rise2Component::rebuildRows()
{
    rows.clear();
    curveRow.reset();
    curves.clear();
    sectionHeaders.clear();
    layoutOrder.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();

    for (auto& f : fields())
        deviceValues[f.id] = pendingValues[f.id] = f.read (state);

    const auto onChange = [this] (juce::uint32 id, juce::int32 v) { onItemChanged (id, v); };

    const char* groupOrder[] = { "Sensitivity", "MIDI Settings", "Play mode", "MIDI CC Assignments", "Power" };
    for (auto* g : groupOrder)
    {
        auto header = std::make_unique<juce::Label>();
        header->setText (g, juce::dontSendNotification);
        header->setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        header->setColour (juce::Label::textColourId, juce::Colour (0xff6fd3d6));
        header->setColour (juce::Label::backgroundColourId, juce::Colour (0x22ffffff));
        header->setJustificationType (juce::Justification::centredLeft);
        rowHolder.addAndMakeVisible (*header);
        layoutOrder.push_back ({ header.get(), sectionHeaderHeight });
        sectionHeaders.push_back (std::move (header));

        if (juce::String (g) == "Sensitivity")
        {
            curveRow = std::make_unique<CurveRow>();
            for (auto& d : dims)
                curves.push_back (curveRow->add (std::make_unique<CurveComponent> (
                    d.label, juce::Colour (d.colour), d.id, deviceValues[d.id],
                    d.minId != 0 ? deviceValues[d.minId] : 0, d.maxId != 0 ? deviceValues[d.maxId] : 127, onChange,
                    d.minId != 0)));
            rowHolder.addAndMakeVisible (*curveRow);
            layoutOrder.push_back ({ curveRow.get(), 130 });
        }

        for (auto& f : fields())
        {
            if (juce::String (f.group) != g || isCurve (f.id))
                continue;

            roli::Block::ConfigMetaData meta (f.id);
            meta.name = f.name;
            meta.group = f.group;
            meta.type = f.type;
            meta.range = { f.lo, f.hi };
            meta.value = deviceValues[f.id];
            meta.isActive = true;
            for (size_t i = 0; i < f.options.size() && i < (size_t) roli::Block::ConfigMetaData::numOptionNames; ++i)
                meta.optionNames[i] = f.options[i];

            auto row = std::make_unique<ConfigRow> (meta, deviceValues[f.id], onChange);
            rowHolder.addAndMakeVisible (*row);
            layoutOrder.push_back ({ row.get(), rowHeight });
            rows.push_back (std::move (row));
        }
    }

    resized();
    updateEnabled();
}

void Rise2Component::onItemChanged (juce::uint32 id, juce::int32 v)
{
    pendingValues[id] = v;
    const bool differs = deviceValues[id] != v;
    for (auto& r : rows)
        if (r->getId() == id)
            r->setChangedFromDevice (differs);
    for (auto* c : curves)
        if (c->getId() == id)
            c->setChanged (differs);
    updateEnabled();
}

void Rise2Component::updateEnabled()
{
    const auto mode = (rise2::Mode) value (kMode);
    for (auto& r : rows)
    {
        const auto id = r->getId();
        if (id == kZone || id == kMembers) r->setEnabled (mode == rise2::Mode::mpe);
        if (id == kChannel)                r->setEnabled (mode == rise2::Mode::single);
    }

    bool any = false;
    for (auto& kv : pendingValues)
        any = any || deviceValues[kv.first] != kv.second;
    saveButton.setEnabled (any && ! awaitingState && midiOut != nullptr);
}

void Rise2Component::saveToDevice()
{
    rise2::Messages msgs;
    expected.clear();
    bool modeChanged = false;

    for (auto& kv : pendingValues)
    {
        if (deviceValues[kv.first] == kv.second)
            continue;
        expected[kv.first] = kv.second;
        if (isModeGroup (kv.first))
            modeChanged = true;
        else if (auto* f = findField (kv.first); f != nullptr && f->write)
            rise2::append (msgs, f->write (kv.second));
    }

    if (modeChanged)
    {
        const auto mode = (rise2::Mode) value (kMode);
        rise2::Messages m = rise2::midiMode (mode, value (kZone) == 1, value (kMembers), value (kChannel), value (kBend));
        rise2::append (m, msgs);
        msgs = std::move (m);

        // Settings that don't apply to the new mode are not stored by the device.
        if (mode != rise2::Mode::mpe)    { expected.erase (kZone); expected.erase (kMembers); }
        if (mode != rise2::Mode::single) expected.erase (kChannel);
    }

    if (msgs.empty())
        return;

    send (msgs);
    saveButton.setEnabled (false);
    setStatus ("Sent " + juce::String ((int) expected.size()) + " setting(s), re-reading from device...");

    juce::Timer::callAfterDelay (300, [safe = juce::Component::SafePointer<Rise2Component> (this)]
    {
        if (safe != nullptr)
            safe->requestState();
    });
}

void Rise2Component::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

void Rise2Component::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (28);
    infoLabel.setBounds (top.removeFromRight (330));
    statusLabel.setBounds (top);
    r.removeFromTop (4);

    auto buttons = r.removeFromTop (30);
    reloadButton.setBounds (buttons.removeFromLeft (165).reduced (2, 0));
    saveButton  .setBounds (buttons.removeFromLeft (165).reduced (2, 0));
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
