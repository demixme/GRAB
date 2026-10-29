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
namespace off  = rise2::offset;
namespace goff = rise2::goffset;

// Control ids, local to this page.
enum : juce::uint32
{
    kMode = 1, kZone, kMembers, kChannel, kBend,
    kGlideTracking, kPressTracking, kPortForwarding, kPortThrottle,
    kOctave, kTranspose, kSemitoneLegato, kFixedVelocity, kFixedVelocityValue, kSlideCc, kSlideMode,
    kStrikeSens, kGlideSens, kSlideSens, kPressSens, kLiftSens,
    kStrikeMin, kStrikeMax, kPressMin, kPressMax, kLiftMin, kLiftMax,
    kFaderLeftCc, kFaderMiddleCc, kFaderRightCc, kXyXCc, kXyYCc, kPedalCc,
    kSleepTime,
    // GRAND-only: three piano pedals (params 0x08/0x0A/0x0C — confirmed from a Dashboard capture).
    kPedal1Cc, kPedal2Cc, kPedal3Cc
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

// A field read that uses a different offset on the GRAND (its reply layout differs from the RISE's).
std::function<int (const rise2::State&)> gbyte (int riseOff, int grandOff, int bias = 0)
{
    return [riseOff, grandOff, bias] (const rise2::State& s) { return s.at (s.grand ? grandOff : riseOff) + bias; };
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
        { kMembers,  "No. MIDI Channels", "MIDI Settings", CT::integer, 1, 16, {},
          // MPE member count, or the Multi-channel count - both are lastMember-firstMember+1
          // (Single reports 1). Was wrongly hardcoded to 15 outside MPE - issue #9.
          [] (const rise2::State& s) { return juce::jlimit (1, 16, s.members()); }, {} },
        { kChannel,  "MIDI Channel",     "MIDI Settings", CT::integer, 1, 16, {},
          // MPE master channel, or the Multi/Single START channel (firstMember+1).
          [] (const rise2::State& s) { return s.mode() == rise2::Mode::mpe ? s.channel() : s.startChannel(); }, {} },
        { kBend,     "Pitch Bend Range", "MIDI Settings", CT::integer, 1, 96, {}, gbyte (off::bendRange, goff::bendRange), {} },
        { kGlideTracking, "Glide & Slide Tracking", "MIDI Settings", CT::options, 1, 3, { "Last Played", "Highest", "Lowest" },
          byte (off::glideTracking), rise2::glideSlideTracking },
        { kPressTracking, "Press Tracking",  "MIDI Settings", CT::options, 0, 3,
          { "Poly Aftertouch", "Last Played (Channel Pressure)", "Highest", "Lowest" },
          byte (off::pressTracking), rise2::pressTracking },
        { kPortForwarding, "MIDI Port Forwarding", "MIDI Settings", CT::boolean, 0, 1, {}, byte (off::portForwarding), param (0x4d) },
        { kPortThrottle,   "MIDI Port Throttle",   "MIDI Settings", CT::options, 0, 3, { "Off", "Half", "Quarter", "Note On/Off" },
          byte (off::portThrottle), param (0x4e) },

        { kOctave,    "Octave",    "Play mode", CT::integer, -4, 4, {},   byte (off::octave, -4), rise2::octave },
        { kTranspose, "Transpose", "Play mode", CT::integer, -12, 12, {}, gbyte (off::transpose, goff::transpose, -64), rise2::transpose },
        { kSemitoneLegato, "Semitone Legato", "Play mode", CT::boolean, 0, 1, {}, byte (off::semitoneLegato), param (0x4c) },
        { kSlideCc,   "Slide CC",   "Play mode", CT::integer, 0, 127, {}, byte (off::slideCc), param (0x38) },
        { kSlideMode, "Slide Mode", "Play mode", CT::options, 0, 2, { "Absolute", "Relative Unipolar", "Relative Bipolar" },
          byte (off::slideMode), param (0x39) },

        // Curves (sensitivities) are drawn as panels, these follow as rows.
        { kStrikeSens, "Strike", "Sensitivity", CT::integer, 0, 127, {}, byte (off::strikeSens), param (0x1a) },
        { kGlideSens,  "Glide",  "Sensitivity", CT::integer, 0, 127, {},
          // The GRAND stores glide inverted: writing 0x17=V lands as byte 127-V (confirmed from
          // BabeleDunnit's full Dashboard capture, issue #6), so un-invert it on read. The write
          // (param 0x17 = the displayed value) is already correct, so only the read flips.
          [] (const rise2::State& s) { return s.grand ? 127 - s.at (goff::glideSens) : s.at (off::glideSens); },
          param (0x17) },
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

        // GRAND-only pedals. Params 0x08/0x0A/0x0C and the reply offsets are both confirmed from
        // BabeleDunnit's full Dashboard capture (CC stored as the low byte of a 2-byte pair).
        // Pedal names match ROLI Dashboard's GRAND page (and standard MIDI): CC64 Sustain,
        // CC67 Soft, CC66 Sostenuto. (Pedal 2/3 labels were previously swapped.)
        { kPedal1Cc, "Pedal 1 CC (Sustain)",    "Pedals", CT::integer, 0, 127, {}, byte (goff::pedal1Cc), param (0x08) },
        { kPedal2Cc, "Pedal 2 CC (Soft)",       "Pedals", CT::integer, 0, 127, {}, byte (goff::pedal2Cc), param (0x0a) },
        { kPedal3Cc, "Pedal 3 CC (Sostenuto)",  "Pedals", CT::integer, 0, 127, {}, byte (goff::pedal3Cc), param (0x0c) },
    };
    return f;
}

// The GRAND is a 4D instrument with 3 pedals: no Slide or Strike axis, no touch faders/XY pad,
// and three pedal CCs instead of the RISE's single pedal. These predicates drive which fields
// each page shows. (Provisional — confirmed from a partial Dashboard capture; see RISE-GRAND.md.)
bool isRiseOnlyField (juce::uint32 id)
{
    switch (id)
    {
        case kSlideCc: case kSlideMode: case kSlideSens:
        case kStrikeSens: case kStrikeMin: case kStrikeMax:
        case kFaderLeftCc: case kFaderMiddleCc: case kFaderRightCc: case kXyXCc: case kXyYCc:
        case kPedalCc:
            return true;
        default:
            return false;
    }
}
bool isGrandOnlyField (juce::uint32 id) { return id == kPedal1Cc || id == kPedal2Cc || id == kPedal3Cc; }

// The fields a real GRAND shows. All reply offsets here are confirmed from BabeleDunnit's two full
// Dashboard captures (issue #6): the three pedal CCs, glide sensitivity, transpose, and the MIDI
// block (mode / zone / channel count / start channel / pitch bend range). The pedal writes (3d
// 08/0a/0c) and the MPE on/off flag (3d 0e) are confirmed from the capture too; the mode RPNs and
// bend/transpose NRPN/RPN reuse the RISE's 0x78 scheme (same family) and await a GRAND write test.
bool isGrandField (juce::uint32 id)
{
    // No kZone: the GRAND is a lower-zone-only MPE device (Dashboard shows no Upper option).
    // kMode/kMembers/kChannel/kBend are shown but read-only on the GRAND for now (see updateEnabled):
    // their RISE-style RPN write mis-maps on the GRAND, so GRAB displays them without writing until
    // the GRAND's write protocol is captured (issue #6).
    return id == kGlideSens || id == kPedal1Cc || id == kPedal2Cc || id == kPedal3Cc
        || id == kTranspose
        || id == kMode || id == kMembers || id == kChannel || id == kBend;
}

// Mode-group fields that are display-only on the GRAND (their write isn't mapped yet).
bool isGrandReadOnly (juce::uint32 id)
{
    return id == kMode || id == kMembers || id == kChannel || id == kBend;
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

// Built-in ROLI presets for the Seaboard RISE, transcribed from ROLI Dashboard's own
// Rise.littlefootModes/*.mode files. Values are config id -> value. Curve min/max are
// omitted (set via curve nodes; read back as zeros on the original RISE). Note the RISE
// "Piano" differs from the Block's: it's Single Channel with Glide Sensitivity 0 (the RISE
// has no "Piano Mode" toggle), rather than a Piano-Mode flag with Slide/Press/Lift off.
struct RisePreset { const char* name; std::vector<std::pair<juce::uint32, juce::int32>> values; };

std::vector<std::pair<juce::uint32, juce::int32>> riseBase()
{
    return { { kGlideTracking, 1 }, { kPressTracking, 1 }, { kOctave, 0 }, { kTranspose, 0 },
             { kSemitoneLegato, 1 }, { kFixedVelocity, 0 }, { kSlideCc, 74 }, { kSlideMode, 0 }, { kBend, 48 },
             { kStrikeSens, 101 }, { kGlideSens, 127 }, { kSlideSens, 127 }, { kPressSens, 127 }, { kLiftSens, 127 },
             { kFaderLeftCc, 107 }, { kFaderMiddleCc, 109 }, { kFaderRightCc, 111 },
             { kXyXCc, 113 }, { kXyYCc, 114 }, { kPedalCc, 64 } };
}

std::vector<std::pair<juce::uint32, juce::int32>> withExtra (
    std::vector<std::pair<juce::uint32, juce::int32>> v,
    std::initializer_list<std::pair<juce::uint32, juce::int32>> extra)
{
    for (auto& e : extra) v.push_back (e);   // applied in order, so these override the base
    return v;
}

const std::vector<RisePreset>& risePresets()
{
    static const std::vector<RisePreset> presets = {
        { "MPE",            withExtra (riseBase(), { { kMode, 0 }, { kZone, 0 }, { kMembers, 15 } }) },
        { "Multi Channel",  withExtra (riseBase(), { { kMode, 1 } }) },
        { "Single Channel", withExtra (riseBase(), { { kMode, 2 }, { kChannel, 1 } }) },
        { "Piano",          withExtra (riseBase(), { { kMode, 2 }, { kChannel, 1 }, { kGlideSens, 0 } }) }
    };
    return presets;
}

// Item ids for the unified Preset combo. Factory presets are 1..risePresets().size(); the user's
// saved presets start at kUserPresetBase (index = id - kUserPresetBase); the trailing actions use
// high ids so they never collide.
enum PresetMenuId
{
    kUserPresetBase = 1000,
    kPresetSaveAs   = 90001,
    kPresetImport   = 90002,
    kPresetExport   = 90003,
    kPresetReveal   = 90004
};
} // namespace

//==============================================================================
Rise2Component::Rise2Component (seaprobe::RawLogger& loggerToUse) : logger (loggerToUse)
{
    liveCc.fill (-1);   // no CC data yet
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (infoLabel);
    infoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (reloadButton);
    addAndMakeVisible (saveButton);
    reloadButton.onClick = [this] { setStatus ("Reloading from device..."); requestState(); };
    saveButton.onClick   = [this] { saveToDevice(); };
    liveWriter.onTick    = [this] { flushLiveWrites(); };
    saveButton.setEnabled (false);

    // One "Preset" menu: ROLI's built-in presets and the user's own saved presets in one list,
    // plus Save / Import / Export actions - the way ROLI Dashboard presents presets.
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Preset...");
    presetBox.setEnabled (false);       // enabled after a verified read
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0)
            return;

        if (id >= 1 && id <= (int) risePresets().size())
            applyRisePreset (presetBox.getText());
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

        presetBox.setSelectedId (0, juce::dontSendNotification);
    };
    refreshPresetMenu();

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

void Rise2Component::previewSyntheticGrand()
{
    using namespace rise2;

    stopTimer();
    closePorts();
    portName = "Seaboard GRAND (preview)";
    grand = true;   // rebuildRows() then shows the GRAND field set (4D, 3 pedals)

    // Build a real GRAND-shaped 0x3a reply (87 bytes, goffset layout) with the values from
    // BabeleDunnit's capture, so the preview matches what a real GRAND shows.
    std::vector<std::uint8_t> raw (87, 0);
    raw[0] = 0xf0; raw[1] = 0x00; raw[2] = 0x21; raw[3] = 0x10; raw[4] = 0x78; raw[5] = 0x3a;
    raw[86] = 0xf7;
    auto put = [&raw] (int o, const char* s) { for (int i = 0; s[i] != 0; ++i) raw[(size_t) (o + i)] = (std::uint8_t) s[i]; };
    put (goff::model,    "GRND1.01");
    put (goff::firmware, "0.6.9");
    raw[goff::keyCount] = 62;
    raw[goff::pedal1Cc] = 60; raw[goff::pedal2Cc] = 67; raw[goff::pedal3Cc] = 66;
    raw[goff::bendRange] = 12; raw[goff::transpose] = 64;  // -> 0
    raw[goff::glideSens] = 80; raw[goff::pressSens] = 100;
    // MPE lower zone, 15 members (as the real GRAND in BabeleDunnit's capture reported).
    raw[goff::firstMember] = 1; raw[goff::lastMember] = 15; raw[goff::masterChannel] = 0; raw[goff::mpeOn] = 1;

    State s; s.raw = std::move (raw);
    awaitingState = true;
    onState (std::move (s));
    setStatus (grab::u8 ("Seaboard GRAND preview (simulated, PRELIMINARY).  Ctrl+Alt+Shift+G to exit."));
}

void Rise2Component::openPorts()
{
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (rise2::isRisePort (d.name) && midiOut == nullptr)
        {
            midiOut = juce::MidiOutput::openDevice (d.identifier);
            portName = d.name;
            grand = rise2::isGrandName (d.name);   // provisional; confirmed from the reply's model in onState
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
        setStatus ("RISE/GRAND disconnected. Waiting for it to come back...");
        return;
    }

    // A fresh state read replaces the rows, so unsaved edits from before the drop are lost here.
    // Ports opened the moment the device reappears stay silent, so give it time to start.
    logger.line ("RISE ports back");
    setStatus ("RISE/GRAND reconnected, reading settings...");
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
        setStatus ("RISE/GRAND MIDI port not available.");
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
        setStatus ("No reply from the RISE/GRAND. Check the cable and that no other app holds the port, "
                   "then try Reload from device. If it persists, Advanced -> Help add device sends us what we need.");
        return;
    }
    send ({ rise2::identify() });
}

void Rise2Component::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // Capture CC values for the live fader / XY-pad meters (MIDI thread; single int writes are
    // safe to read unlocked from the UI timer - worst case a one-frame-stale value).
    if (message.isController())
        liveCc[(size_t) juce::jlimit (0, 127, message.getControllerNumber())] = message.getControllerValue();

    const auto* d = message.getRawData();
    const auto size = message.getRawDataSize();

    // Only log SysEx (identify replies etc.) - logging every performance CC/note would flood it.
    if (message.isSysEx())
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

    // The model string is authoritative: a GRAND replies "GRND..." (shorter 0x3a dump). Decide it
    // straight from the reply bytes: State::text()/at() gate on valid(), which can't know the reply
    // size until state.grand is set - so a GRAND's 87-byte reply would fail the RISE 224-byte length
    // check and read as an empty model, leaving every GRAND mis-detected and shown with no settings
    // (the "firmware not verified" bug, issue #6). Read model[6..9] from raw directly, then commit.
    juce::String rawModel;
    for (int i = goff::model; i < goff::model + 4 && (size_t) i < state.raw.size() && state.raw[(size_t) i] != 0; ++i)
        rawModel << (juce::juce_wchar) state.raw[(size_t) i];
    grand = rawModel.startsWithIgnoreCase ("GRND");
    state.grand = grand;

    if (grand)
    {
        firmware = state.text (goff::firmware, 8);   // stored for preset metadata
        infoLabel.setText (portName + "   fw " + firmware + "   " + juce::String (state.at (goff::keyCount)) + " keys"
                               + "   " + grab::u8 ("\xe2\x80\x94") + " GRAND page: pedals & glide are editable; "
                                 "MIDI mode / channel / bend show live but are read-only until the write is mapped",
                           juce::dontSendNotification);
    }
    else
    {
        firmware = state.text (off::firmware, 8);   // stored for preset metadata
        const auto extra    = state.text (off::extraVersion, 8);
        const auto ble      = state.text (off::bleFirmware, 8);
        infoLabel.setText (portName + "   fw " + firmware + (extra.isNotEmpty() ? " (" + extra + ")" : juce::String())
                               + (ble.isNotEmpty() ? "   Bluetooth " + ble : juce::String())
                               + "   battery " + juce::String (state.at (off::battery)) + "/7",
                           juce::dontSendNotification);
    }

    // The write protocol is confirmed on both the RISE 2 (fw 2.x) and the original RISE
    // (fw 1.x) - the original returns the same dump and uses the same 0x78/RPN/NRPN writes
    // (see RISE-GRAND.md), so both are editable. Anything else we don't guess at.
    const bool verified = firmware.startsWith ("2.");
    const bool oldRise  = firmware.startsWith ("1.");   // original RISE 25/49, same 0x78 dump

    if (! verified && ! oldRise && ! grand)   // GRAND is editable via the same 0x78 3D writes
    {
        setStatus ("'" + portName + "' firmware " + firmware + " is not verified yet, so settings are not shown. "
                   "Use Advanced... -> Help add device (or Capture MIDI) to help add it.");
        expected.clear();
        // Clear any rows a prior read built, so nothing stale shows under the message.
        rows.clear();
        curveRow.reset();
        curves.clear();
        sectionHeaders.clear();
        layoutOrder.clear();
        rowHolder.removeAllChildren();
        saveButton.setEnabled (false);
        presetBox.setEnabled (false);
        resized();
        return;
    }

    // The original RISE (fw 1.x) uses the same write protocol as the RISE 2 (confirmed from a
    // contributed Dashboard capture), so it is editable too. Its one quirk: the min/max block
    // reads back as zeros, so those stay hidden even though they are writable.
    minMaxReliable = verified;
    rebuildRows();
    presetBox.setEnabled (true);        // settings are shown now, so presets apply
    refreshPresetMenu();                // (re)build for this device family now that `grand` is known

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

    // GRAND hides the RISE-only controls (slide/strike/faders/XY/single pedal) and shows its
    // three pedals instead; the RISE hides the GRAND-only pedal fields.
    const auto include = [this] (juce::uint32 id)
    { return grand ? isGrandField (id) : ! isGrandOnlyField (id); };

    const char* groupOrder[] = { "Sensitivity", "MIDI Settings", "Play mode", "MIDI CC Assignments", "Pedals", "Power" };
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
                if (! include (d.id)) continue;   // GRAND: no Strike/Slide curve
                const bool hasMinMax = d.minId != 0 && minMaxReliable && ! grand;   // old RISE / GRAND can't read min/max back
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
            // Three touch faders as vertical faders + the XY pad as a 2D control; pedal is a row.
            // Each shows a live meter of its assigned CC (animated from the device's MIDI).
            faderPanel = std::make_unique<FaderPanel>();
            for (auto& f : fields())
                if (juce::String (f.group) == group && isFaderField (f.id) && include (f.id)
                    && f.id != kXyXCc && f.id != kXyYCc)
                {
                    auto* fader = faderPanel->add (std::make_unique<Fader> (
                        f.id, shortFaderLabel (f.name), deviceValues[f.id], f.lo, f.hi, onChange));
                    fader->setTooltip (grab::helpForParameter (f.name));
                    faders.push_back (fader);
                }
            if (include (kXyXCc))
            {
                auto* pad = faderPanel->setXYPad (std::make_unique<XYPad> (
                    kXyXCc, kXyYCc, deviceValues[kXyXCc], deviceValues[kXyYCc], onChange));
                pad->setTooltip (grab::helpForParameter ("XY Pad X CC"));
            }
            faderPanel->setLiveSource (liveCc.data());
            if (! faderPanel->faders.empty() || faderPanel->xy != nullptr)
                card->addItem (*faderPanel, 150);

            for (auto& f : fields())
                if (juce::String (f.group) == group && ! isFaderField (f.id) && ! isCurve (f.id) && include (f.id))
                    addRow (f);
        }
        else
        {
            for (auto& f : fields())
                if (juce::String (f.group) == group && ! isCurve (f.id) && ! isCurveMinMax (f.id) && include (f.id))
                    addRow (f);
        }

        if (! card->isEmpty())   // skip a section with no fields for this device (e.g. Pedals on a RISE)
        {
            rowHolder.addAndMakeVisible (*card);
            cards.push_back (std::move (card));
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

    if (modeChanged && ! grand)   // GRAND's mode/channel write isn't mapped yet - display-only there
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
        // The GRAND's MIDI-settings write isn't mapped yet, so those fields are display-only there.
        if (grand && isGrandReadOnly (id)) { r->setEnabled (false); continue; }
        // MPE Zone: MPE only. No. MIDI Channels (count): MPE members or the Multi channel count.
        // MIDI Channel (start): Single's channel or the Multi start channel. (issue #9: Multi used
        // to grey out both, so start channel + count couldn't be read or set.)
        if (id == kZone)    r->setEnabled (mode == rise2::Mode::mpe);
        if (id == kMembers) r->setEnabled (mode == rise2::Mode::mpe   || mode == rise2::Mode::multi);
        if (id == kChannel) r->setEnabled (mode == rise2::Mode::single || mode == rise2::Mode::multi);
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

    if (modeChanged && ! grand)   // GRAND's mode/channel write isn't mapped yet - display-only there
    {
        const auto mode = (rise2::Mode) value (kMode);
        rise2::Messages m = rise2::midiMode (mode, value (kZone) == 1, value (kMembers), value (kChannel), value (kBend));
        rise2::append (m, msgs);
        msgs = std::move (m);

        // Verify only the fields the mode actually uses: Zone (MPE), channel count (MPE + Multi),
        // start/MIDI channel (Multi + Single). (issue #9)
        if (mode != rise2::Mode::mpe)    expected.erase (kZone);
        if (mode == rise2::Mode::single) expected.erase (kMembers);
        if (mode == rise2::Mode::mpe)    expected.erase (kChannel);
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

// Is this field on the current page? The GRAND hides the RISE-only controls and vice versa, and
// deviceValues/pendingValues hold every field - so a preset must not push a RISE-only value (a
// fader CC, say) into a GRAND, where it would be written to a device that ignores it.
bool Rise2Component::fieldShown (juce::uint32 id) const
{
    return grand ? isGrandField (id) : ! isGrandOnlyField (id);
}

juce::File Rise2Component::presetDir() const
{
    // Scoped per device family so a RISE preset never shows up for (or is applied to) a GRAND.
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("GRAB").getChildFile ("presets")
             .getChildFile (grand ? "grand" : "rise");
}

// Apply a set of id -> value into the UI (and, when live, queue them to write to the device like
// any interactive edit). Shared by the ROLI presets, the user's saved presets, and file import.
int Rise2Component::applyPresetValues (const std::map<juce::uint32, juce::int32>& values, bool live)
{
    if (pendingValues.empty())
        return 0;

    int applied = 0;
    for (auto& [id, v] : values)
    {
        if (pendingValues.find (id) == pendingValues.end())  // device doesn't have this item
            continue;
        if (! fieldShown (id))                               // not on this (RISE/GRAND) page
            continue;
        if (grand && isGrandReadOnly (id))                   // display-only on the GRAND, don't write
            continue;

        pendingValues[id] = v;
        const bool differs = deviceValues[id] != v;
        for (auto& r : rows)
            if (r->getId() == id) { r->setValue (v); r->setChangedFromDevice (differs); }
        for (auto* c : curves)
        {
            if (c->getId()    == id)       { c->setSensitivity (v); c->setChanged (differs); }
            if (c->getMinId() == (int) id) c->setMin (v);
            if (c->getMaxId() == (int) id) c->setMax (v);
        }
        for (auto* f : faders)
            if (f->getId() == id) { f->setValue (v); f->setChanged (differs); }
        if (live && differs)
            dirtyWrites.insert (id);
        ++applied;
    }

    if (live)
        liveWriter.nudge();   // live-writes the changed items (mode group is sent as one midiMode)
    updateEnabled();
    return applied;
}

void Rise2Component::refreshPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    userPresetFiles.clear();

    presetBox.addSectionHeading ("ROLI presets");
    { int id = 1; for (auto& p : risePresets()) presetBox.addItem (p.name, id++); }

    auto dir = presetDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        if (! files.isEmpty())
        {
            presetBox.addSeparator();
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
}

void Rise2Component::applyUserPreset (const juce::File& file)
{
    if (pendingValues.empty())
        return;

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

void Rise2Component::saveUserPresetAs()
{
    if (pendingValues.empty())
    {
        setStatus ("Read the device first, then save a preset.");
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
                    obj->setProperty ("device", portName);
                    obj->setProperty ("firmware", firmware);
                    auto* values = new juce::DynamicObject();
                    for (auto& kv : pendingValues)   // all items incl. the 5D curve dimensions and min/max
                        values->setProperty (juce::String ((int) kv.first), kv.second);
                    obj->setProperty ("values", juce::var (values));
                    file.replaceWithText (juce::JSON::toString (juce::var (obj)));

                    refreshPresetMenu();
                    setStatus ("Preset saved: " + name);
                }
            }
            delete aw;
        }), false);
}

void Rise2Component::savePreset()   // Export to file...
{
    if (pendingValues.empty())
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
            obj->setProperty ("device", portName);
            obj->setProperty ("firmware", firmware);
            auto* values = new juce::DynamicObject();
            for (auto& kv : pendingValues) // all items incl. the 5D curve dimensions and min/max
                values->setProperty (juce::String ((int) kv.first), kv.second);
            obj->setProperty ("values", juce::var (values));

            file.replaceWithText (juce::JSON::toString (juce::var (obj)));
            setStatus ("Preset exported: " + file.getFileName());
        });
}

void Rise2Component::loadPreset()   // Import from file...
{
    if (pendingValues.empty())
    {
        setStatus ("Read the device first, then import a preset.");
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

void Rise2Component::applyRisePreset (const juce::String& name)
{
    const RisePreset* preset = nullptr;
    for (auto& p : risePresets())
        if (name == p.name) { preset = &p; break; }
    if (preset == nullptr)
        return;

    // Build the map in order so a later pair overrides an earlier one (e.g. Piano's GlideSens 0
    // overrides the base 127) - a plain range-construct would keep the first, not the last.
    std::map<juce::uint32, juce::int32> m;
    for (auto& [id, v] : preset->values)
        m[id] = v;

    const int applied = applyPresetValues (m, true);
    setStatus ("Applied ROLI preset '" + name + "' (" + juce::String (applied) + " items).");
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
    buttons.removeFromLeft (8);
    presetBox.setBounds (buttons.removeFromLeft (190).reduced (2, 0));
    r.removeFromTop (6);

    viewport.setBounds (r);
    layoutCardGrid (viewport, rowHolder, cards, "Sensitivity");
}
