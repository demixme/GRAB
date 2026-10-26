#include "Rise2Component.h"
#include "MainComponent.h"
#include "SectionCard.h"
#include "FaderPanel.h"
#include "Help.h"

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
    { kStrikeSens, "Strike", grab::theme::strike.getARGB(), kStrikeMin, kStrikeMax }, { kGlideSens, "Glide", grab::theme::glide.getARGB(), 0, 0 },
    { kSlideSens,  "Slide",  grab::theme::slide.getARGB(), 0, 0 },                     { kPressSens, "Press", grab::theme::press.getARGB(), kPressMin, kPressMax },
    { kLiftSens,   "Lift",   grab::theme::lift.getARGB(), kLiftMin, kLiftMax } };

bool isCurve (juce::uint32 id) { return id >= kStrikeSens && id <= kLiftSens; }

// The Strike/Press/Lift min/max — set by dragging the curve end-nodes (fw 2.x), not as rows.
// On the original RISE (fw 1.x) they read back as zeros, so they're hidden there entirely.
bool isCurveMinMax (juce::uint32 id)
{
    switch (id)
    {
        case kStrikeMin: case kStrikeMax: case kPressMin: case kPressMax: case kLiftMin: case kLiftMax:
            return true;
        default:
            return false;
    }
}

// The MIDI CC Assignments that map to a physical fader / XY-pad axis (shown as vertical
// faders). The pedal CC is not a fader, so it stays a normal row.
bool isFaderField (juce::uint32 id)
{
    switch (id)
    {
        case kFaderLeftCc: case kFaderMiddleCc: case kFaderRightCc: case kXyXCc: case kXyYCc:
            return true;
        default:
            return false;
    }
}

// "Left Fader CC" -> "Left", "XY Pad X CC" -> "X" (the full name is long for a narrow fader).
juce::String shortFaderLabel (juce::String n)
{
    return n.replace (" Fader CC", "").replace ("XY Pad ", "").replace (" CC", "");
}

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
    addAndMakeVisible (loadPresetBtn);
    addAndMakeVisible (savePresetBtn);
    reloadButton.onClick = [this] { setStatus ("Reloading from device..."); requestState(); };
    saveButton.onClick   = [this] { saveToDevice(); };
    liveWriter.onTick    = [this] { flushLiveWrites(); };
    loadPresetBtn.onClick = [this] { loadPreset(); };
    savePresetBtn.onClick = [this] { savePreset(); };
    saveButton.setEnabled (false);
    loadPresetBtn.setEnabled (false);   // enabled once settings are shown (a verified read)
    savePresetBtn.setEnabled (false);

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

void Rise2Component::previewSyntheticState (bool verifiedFirmware)
{
    using namespace rise2;

    // No device involved — build a plausible identify reply with plausible values.
    stopTimer();
    closePorts();
    portName = "Seaboard RISE (preview)";

    std::vector<std::uint8_t> raw ((size_t) offset::replySize, 0);
    raw[0] = 0xf0; raw[1] = 0x00; raw[2] = 0x21; raw[3] = 0x10; raw[4] = 0x78; raw[5] = 0x3a;
    raw[(size_t) offset::replySize - 1] = 0xf7;

    auto put = [&raw] (int o, const char* s) { for (int i = 0; s[i] != 0; ++i) raw[(size_t) (o + i)] = (std::uint8_t) s[i]; };
    put (offset::model,    "RISE1.00");
    put (offset::firmware, verifiedFirmware ? "2.0.2" : "1.1.8");
    put (verifiedFirmware ? offset::bleFirmware : offset::extraVersion, verifiedFirmware ? "0.0.2" : "1.1.0");
    put (offset::serial,   "PREVIEW00000000");   // not a real serial

    raw[offset::firstMember]   = 1;   raw[offset::lastMember] = 15;  raw[offset::masterChannel] = 0;
    raw[offset::mpeOn]         = 1;   raw[offset::bendRange]  = 48;  raw[offset::octave]        = 4;  // -> 0
    raw[offset::glideTracking] = 1;   raw[offset::pressTracking] = 1; raw[offset::pedalCc]      = 64;
    raw[offset::faderLeftCc] = 107; raw[offset::faderMiddleCc] = 109; raw[offset::faderRightCc] = 111;
    raw[offset::xyXCc] = 113; raw[offset::xyYCc] = 114;
    raw[offset::glideSens] = 80; raw[offset::slideSens] = 90; raw[offset::pressSens] = 100;
    raw[offset::strikeSens] = 70; raw[offset::liftSens] = 60;
    raw[offset::slideTracking] = 1; raw[offset::slideCc] = 74; raw[offset::slideMode] = 2; // Relative Bipolar
    raw[offset::fixedVelocity] = 0; raw[offset::fixedVelocityValue] = 127;
    raw[offset::strikeMin] = 8; raw[offset::strikeMax] = 127; raw[offset::pressMin] = 0; raw[offset::pressMax] = 127;
    raw[offset::liftMin] = 0; raw[offset::liftMax] = 127;
    raw[offset::transpose] = 64;  // -> 0
    raw[offset::battery]   = 6;

    State s; s.raw = std::move (raw);
    awaitingState = true;   // let onState proceed
    onState (std::move (s));
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
    liveWriter.cancel();
    dirtyWrites.clear();
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
                   "Older RISE models aren't fully mapped yet - use Help add device... to send what we need.");
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

    firmware = state.text (off::firmware, 8);   // stored for preset metadata
    const auto extra    = state.text (off::extraVersion, 8);
    const auto ble      = state.text (off::bleFirmware, 8);
    infoLabel.setText (portName + "   fw " + firmware + (extra.isNotEmpty() ? " (" + extra + ")" : juce::String())
                           + (ble.isNotEmpty() ? "   Bluetooth " + ble : juce::String())
                           + "   battery " + juce::String (state.at (off::battery)) + "/7",
                       juce::dontSendNotification);

    // The write protocol is confirmed on both the RISE 2 (fw 2.x) and the original RISE
    // (fw 1.x) - the original returns the same dump and uses the same 0x78/RPN/NRPN writes
    // (see RISE-GRAND.md), so both are editable. Anything else we don't guess at.
    const bool verified = firmware.startsWith ("2.");
    const bool oldRise  = firmware.startsWith ("1.");   // original RISE 25/49, same 0x78 dump

    if (! verified && ! oldRise)
    {
        setStatus ("'" + portName + "' firmware " + firmware + " is not verified yet, so settings are not shown. "
                   "Use Help add device... (or Advanced... -> Raw MIDI capture) to help add it.");
        expected.clear();
        // Clear any rows a prior read built, so nothing stale shows under the message.
        rows.clear();
        curveRow.reset();
        curves.clear();
        sectionHeaders.clear();
        layoutOrder.clear();
        rowHolder.removeAllChildren();
        saveButton.setEnabled (false);
        loadPresetBtn.setEnabled (false);
        savePresetBtn.setEnabled (false);
        resized();
        return;
    }

    // The original RISE (fw 1.x) uses the same write protocol as the RISE 2 (confirmed from a
    // contributed Dashboard capture), so it is editable too. Its one quirk: the min/max block
    // reads back as zeros, so those stay hidden even though they are writable.
    minMaxReliable = verified;
    rebuildRows();
    loadPresetBtn.setEnabled (true);    // settings are shown now, so presets apply
    savePresetBtn.setEnabled (true);

    if (expected.empty())
    {
        setStatus ("Read " + juce::String ((int) rows.size() + (int) curves.size()) + " settings from the device."
                   + (oldRise ? juce::String ("  Writes enabled (confirmed from a RISE 25 capture); "
                                              "min/max aren't shown - not reported reliably on this firmware.")
                              : juce::String()));
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
    cards.clear();          // un-parents the rows/curves/faders it holds
    faderPanel.reset();
    faders.clear();
    rows.clear();
    curveRow.reset();
    curves.clear();
    rowHolder.removeAllChildren();
    deviceValues.clear();
    pendingValues.clear();
    dirtyWrites.clear();
    liveWriter.cancel();

    for (auto& f : fields())
        deviceValues[f.id] = pendingValues[f.id] = f.read (state);

    const auto onChange = [this] (juce::uint32 id, juce::int32 v) { onItemChanged (id, v); };

    const char* groupOrder[] = { "Sensitivity", "MIDI Settings", "Play mode", "MIDI CC Assignments", "Power" };
    for (auto* g : groupOrder)
    {
        const juce::String group (g);
        auto card = std::make_unique<SectionCard> (group);

        auto addRow = [&] (const Field& f)
        {
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
            card->addItem (*row, rowHeight);
            rows.push_back (std::move (row));
        };

        if (group == "Sensitivity")
        {
            curveRow = std::make_unique<CurveRow>();
            for (auto& d : dims)
            {
                const bool hasMinMax = d.minId != 0 && minMaxReliable;   // old RISE can't read min/max back
                const int minId = hasMinMax ? (int) d.minId : -1;
                const int maxId = hasMinMax ? (int) d.maxId : -1;
                curves.push_back (curveRow->add (std::make_unique<CurveComponent> (
                    d.label, juce::Colour (d.colour), d.id, deviceValues[d.id],
                    minId, hasMinMax ? deviceValues[d.minId] : 0,
                    maxId, hasMinMax ? deviceValues[d.maxId] : 127, onChange)));
            }
            card->addItem (*curveRow, 130);
        }

        if (group == "MIDI CC Assignments")
        {
            // The three touch faders + XY-pad axes as vertical faders; pedal stays a row.
            faderPanel = std::make_unique<FaderPanel>();
            for (auto& f : fields())
                if (juce::String (f.group) == group && isFaderField (f.id))
                {
                    auto* fader = faderPanel->add (std::make_unique<Fader> (
                        f.id, shortFaderLabel (f.name), deviceValues[f.id], f.lo, f.hi, onChange));
                    fader->setTooltip (grab::helpForParameter (f.name));
                    faders.push_back (fader);
                }
            if (! faderPanel->faders.empty())
                card->addItem (*faderPanel, 150);

            for (auto& f : fields())
                if (juce::String (f.group) == group && ! isFaderField (f.id) && ! isCurve (f.id))
                    addRow (f);
        }
        else
        {
            for (auto& f : fields())
                if (juce::String (f.group) == group && ! isCurve (f.id) && ! isCurveMinMax (f.id))
                    addRow (f);
        }

        rowHolder.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
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
    for (auto* f : faders)
        if (f->getId() == id)
            f->setChanged (differs);
    updateEnabled();

    // Live-write, Dashboard-style: queue the edit and let the throttle send it within ~75 ms,
    // coalescing a drag into one burst per window. (Presets load silently and still wait for Save.)
    if (differs && midiOut != nullptr)
    {
        dirtyWrites.insert (id);
        liveWriter.nudge();
    }
}

void Rise2Component::flushLiveWrites()
{
    if (dirtyWrites.empty() || midiOut == nullptr)
    {
        dirtyWrites.clear();
        liveWriter.cancel();   // nothing to write; idle until the next edit
        return;
    }

    rise2::Messages msgs;
    bool modeChanged = false;
    for (auto id : dirtyWrites)
    {
        if (deviceValues[id] == pendingValues[id])
            continue;
        if (isModeGroup (id))
            modeChanged = true;
        else if (auto* f = findField (id); f != nullptr && f->write)
            rise2::append (msgs, f->write (pendingValues[id]));
    }

    if (modeChanged)
    {
        const auto mode = (rise2::Mode) value (kMode);
        rise2::Messages m = rise2::midiMode (mode, value (kZone) == 1, value (kMembers), value (kChannel), value (kBend));
        rise2::append (m, msgs);
        msgs = std::move (m);
    }

    for (auto id : dirtyWrites)
        deviceValues[id] = pendingValues[id];   // optimistic; a reload re-reads the truth
    dirtyWrites.clear();

    if (! msgs.empty())
        send (msgs);

    for (auto& r : rows)   r->setChangedFromDevice (deviceValues[r->getId()] != pendingValues[r->getId()]);
    for (auto* c : curves) c->setChanged (deviceValues[c->getId()] != pendingValues[c->getId()]);
    for (auto* f : faders) f->setChanged (deviceValues[f->getId()] != pendingValues[f->getId()]);
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
    // Save is a flush-all / force-resync now that edits live-write as you make them: it catches
    // anything outstanding (e.g. a just-loaded preset) and re-reads to confirm. Drain the live
    // queue so the throttle doesn't double-send.
    dirtyWrites.clear();
    liveWriter.cancel();

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

void Rise2Component::savePreset()
{
    if (pendingValues.empty())
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
            obj->setProperty ("device", portName);
            obj->setProperty ("firmware", firmware);
            auto* values = new juce::DynamicObject();
            for (auto& kv : pendingValues) // all items incl. the 5D curve dimensions and min/max
                values->setProperty (juce::String ((int) kv.first), kv.second);
            obj->setProperty ("values", juce::var (values));

            file.replaceWithText (juce::JSON::toString (juce::var (obj)));
            setStatus ("Preset saved: " + file.getFileName());
        });
}

void Rise2Component::loadPreset()
{
    if (pendingValues.empty())
    {
        setStatus ("Read the device first, then load a preset.");
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
                {
                    if (c->getId()    == id)        { c->setSensitivity (v); c->setChanged (differs); }
                    if (c->getMinId() == (int) id)  c->setMin (v);
                    if (c->getMaxId() == (int) id)  c->setMax (v);
                }
                for (auto* f : faders)
                    if (f->getId() == id) { f->setValue (v); f->setChanged (differs); }
                ++applied;
            }

            updateEnabled();
            setStatus ("Preset loaded (" + juce::String (applied) + " items). Review, then Save to device.");
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
    reloadButton .setBounds (buttons.removeFromLeft (165).reduced (2, 0));
    saveButton   .setBounds (buttons.removeFromLeft (165).reduced (2, 0));
    loadPresetBtn.setBounds (buttons.removeFromLeft (137).reduced (2, 0));
    savePresetBtn.setBounds (buttons.removeFromLeft (137).reduced (2, 0));
    r.removeFromTop (6);

    viewport.setBounds (r);
    layoutCardGrid (viewport, rowHolder, cards, "Sensitivity");
}
