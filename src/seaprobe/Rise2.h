/*
    Rise2: minimal probe for the Seaboard RISE 2, which does not speak the BLOCKS
    packed protocol. It uses plain ROLI SysEx in the 0x78 namespace plus NRPN.
    Messages are the ones ROLI Dashboard 4.7.2 sends (see RISE-GRAND.md).

      seaprobe rise2 identify                     send the identify request (3f), log the reply
      seaprobe rise2 battery                      read params 28 (battery level 0..7) and 29 (3c)
      seaprobe rise2 set <name> <value> [--commit] one 78 3d parameter write
      seaprobe rise2 octave <n> [--commit]        NRPN 2, n in -4..4
      seaprobe rise2 transpose <n> [--commit]     NRPN 3, n in -12..12
      seaprobe rise2 glide_tracking <n>           NRPN 0 and 7, 1 Last Played, 2 Highest, 3 Lowest
      seaprobe rise2 press_tracking <n>           NRPN 4, 0 Poly Aftertouch, 1 Last Played, 2 Highest, 3 Lowest
      seaprobe rise2 sleep_time <n>               78 3d 26, 14-bit
      seaprobe rise2 mode mpe <lower|upper> <members> <bend>
      seaprobe rise2 mode multi <bend>
      seaprobe rise2 mode single <channel> <bend> 78 3d 0e plus RPN 6 (MPE config) and RPN 0 (bend range)

    Writes are dry-run by default (bytes printed, nothing sent). Only messages that
    ROLI Dashboard itself sends for a settings change are allowed, by name, inside the
    range Dashboard uses. Never sends firmware, factory reset or program upload messages.
*/

#pragma once

#include "LoggingMidi.h"
#include "Rise2Protocol.h"

#include <cstdint>
#include <vector>

namespace seaprobe::rise2
{

struct Param
{
    const char* name;
    std::uint8_t id;
    int min, max;
};

// Parameters observed from Dashboard 4.7.2 and handled by firmware 2.0.2.
inline constexpr Param params[] = {
    { "pedal_cc",             0x08, 0, 127 },
    { "glide_sensitivity",    0x17, 0, 127 },
    { "slide_sensitivity",    0x18, 0, 127 },
    { "press_sensitivity",    0x19, 0, 127 },
    { "strike_sensitivity",   0x1a, 0, 127 },
    { "lift_sensitivity",     0x1b, 0, 127 },
    { "fader_left_cc",        0x1c, 0, 127 },
    { "fader_middle_cc",      0x1d, 0, 127 },
    { "fader_right_cc",       0x1e, 0, 127 },
    { "xy_x_cc",              0x23, 0, 127 },
    { "xy_y_cc",              0x24, 0, 127 },
    { "slide_cc",             0x38, 0, 127 },
    { "slide_mode",           0x39, 0, 2 },
    { "fixed_velocity",       0x3a, 0, 1 },
    { "fixed_velocity_value", 0x3b, 1, 127 },
    { "strike_min",           0x46, 1, 127 },
    { "strike_max",           0x47, 0, 127 },
    { "press_min",            0x48, 0, 127 },
    { "press_max",            0x49, 0, 127 },
    { "lift_min",             0x4a, 0, 127 },
    { "lift_max",             0x4b, 0, 127 },
    { "semitone_legato",      0x4c, 0, 1 },
    { "port_forwarding",      0x4d, 0, 1 },
    { "port_throttle",        0x4e, 0, 3 },
};

inline const Param* findParam (const juce::String& name)
{
    for (auto& p : params)
        if (name.equalsIgnoreCase (p.name))
            return &p;
    return nullptr;
}

//==============================================================================
/** Opens the RISE 2 USB MIDI ports by name and logs everything it hears. */
class Port  : private juce::MidiInputCallback
{
public:
    explicit Port (RawLogger& loggerToUse) : logger (loggerToUse)
    {
        for (auto& d : juce::MidiOutput::getAvailableDevices())
            if (::rise2::isRisePort (d.name))
                out = juce::MidiOutput::openDevice (d.identifier);

        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (::rise2::isRisePort (d.name))
                in = juce::MidiInput::openDevice (d.identifier, this);

        if (in != nullptr)
            in->start();
    }

    ~Port() override
    {
        if (in != nullptr)
            in->stop();
    }

    bool isOpen() const { return out != nullptr; }

    void send (const std::vector<std::uint8_t>& m)
    {
        logger.midi ("OUT", m.data(), m.size(), "rise2");
        out->sendMessageNow (juce::MidiMessage (m.data(), (int) m.size()));
    }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        logger.midi ("IN ", message.getRawData(), (std::size_t) message.getRawDataSize(), "rise2");
    }

    RawLogger& logger;
    std::unique_ptr<juce::MidiOutput> out;
    std::unique_ptr<juce::MidiInput> in;
};

//==============================================================================
inline int run (RawLogger& logger, const juce::StringArray& pos, bool commit)
{
    const auto usage = [&]
    {
        juce::String names;
        for (auto& p : params) names << " " << p.name;
        logger.line ("Usage: seaprobe rise2 identify | battery | set <name> <value> | octave <n> | transpose <n> | glide_tracking <n>"
                     " | press_tracking <n> | sleep_time <n> | mode mpe <lower|upper> <members> <bend>"
                     " | mode multi <bend> | mode single <channel> <bend>  [--commit]");
        logger.line ("Parameters:" + names);
        return 2;
    };

    const auto inRange = [&] (const juce::String& what, int v, int lo, int hi)
    {
        if (v >= lo && v <= hi) return true;
        logger.line ("REFUSED: " + what + " must be in " + juce::String (lo) + ".." + juce::String (hi));
        return false;
    };

    const auto cmd = pos[1];
    namespace p2 = ::rise2;
    p2::Messages msgs;
    bool isWrite = true;

    if (cmd == "identify")
    {
        msgs.push_back (p2::identify());
        isWrite = false;
    }
    else if (cmd == "battery")
    {
        // The only two parameters the firmware answers. Dashboard polls 28 once a minute.
        msgs.push_back (p2::readParam (0x28));
        msgs.push_back (p2::readParam (0x29));
        isWrite = false;
    }
    else if (cmd == "set" && pos.size() >= 4)
    {
        const auto* p = findParam (pos[2]);
        const auto v = pos[3].getIntValue();
        if (p == nullptr)                  { logger.line ("Unknown parameter '" + pos[2] + "'."); return usage(); }
        if (v < p->min || v > p->max)      { logger.line ("REFUSED: " + juce::String (p->name) + " must be in "
                                                          + juce::String (p->min) + ".." + juce::String (p->max)); return 2; }
        msgs.push_back (p2::setParam (p->id, v));
    }
    else if ((cmd == "octave" || cmd == "transpose") && pos.size() >= 3)
    {
        const auto v = pos[2].getIntValue();
        const auto range = cmd == "octave" ? 4 : 12;
        if (v < -range || v > range)       { logger.line ("REFUSED: " + cmd + " must be in "
                                                          + juce::String (-range) + ".." + juce::String (range)); return 2; }
        msgs = cmd == "octave" ? p2::octave (v) : p2::transpose (v);
    }
    else if ((cmd == "glide_tracking" || cmd == "press_tracking" || cmd == "sleep_time") && pos.size() >= 3)
    {
        const auto v = pos[2].getIntValue();
        const auto lo = cmd == "glide_tracking" ? 1 : 0;
        const auto hi = cmd == "sleep_time" ? 16383 : 3;
        if (! inRange (cmd, v, lo, hi)) return 2;

        msgs = cmd == "glide_tracking" ? p2::glideSlideTracking (v)
             : cmd == "press_tracking" ? p2::pressTracking (v)
                                       : p2::sleepTime (v);
    }
    else if (cmd == "mode" && pos.size() >= 4)
    {
        const auto kind = pos[2];
        const auto bend = pos[pos.size() - 1].getIntValue();
        if (! inRange ("bend", bend, 1, 96)) return 2;

        if (kind == "mpe" && pos.size() >= 6)
        {
            const auto upper = pos[3] == "upper";
            const auto members = pos[4].getIntValue();
            if ((! upper && pos[3] != "lower") || ! inRange ("members", members, 1, 15)) return usage();
            msgs = p2::midiMode (p2::Mode::mpe, upper, members, 1, bend);
        }
        else if (kind == "multi" && pos.size() == 4)
        {
            msgs = p2::midiMode (p2::Mode::multi, false, 15, 1, bend);
        }
        else if (kind == "single" && pos.size() >= 5)
        {
            const auto ch = pos[3].getIntValue();
            if (! inRange ("channel", ch, 1, 16)) return 2;
            msgs = p2::midiMode (p2::Mode::single, false, 1, ch, bend);
        }
        else
        {
            return usage();
        }
    }
    else
    {
        return usage();
    }

    if (isWrite && ! commit)
    {
        for (auto& m : msgs)
            logger.line ("DRY " + juce::String (sea::toHex (m.data(), m.size())) + "   (NOT SENT)");
        logger.line ("Re-run with --commit to transmit. Close ROLI Dashboard first so it does not overwrite the change.");
        return 0;
    }

    Port port (logger);
    if (! port.isOpen())
    {
        logger.line ("No 'Seaboard RISE' MIDI output found.");
        return 1;
    }

    for (auto& m : msgs)
        port.send (m);

    // Collect echoes and replies.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
    return 0;
}

} // namespace seaprobe::rise2
