// Rise2Protocol: Seaboard RISE 2 settings over plain MIDI (no BLOCKS packed protocol).
//
// Every message here is one ROLI Dashboard 4.7.2 sends for a settings change, and every
// state offset was mapped on a RISE 2 running firmware 2.0.2 (see RISE-GRAND.md):
//   F0 00 21 10 78 3f F7         identify. The 3a reply carries the live settings.
//   F0 00 21 10 78 3c <p> F7     read. Only 28 (battery 0..7) and 29 are answered.
//   F0 00 21 10 78 3d <p> <v> F7 set one parameter.
//   NRPN / RPN on the master channel for tracking, octave, transpose and MPE zones.
//
// Nothing here builds firmware, factory reset or program upload messages.

#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace rise2
{

using Message  = std::vector<std::uint8_t>;
using Messages = std::vector<Message>;

inline bool isRisePort (const juce::String& name) { return name.containsIgnoreCase ("Seaboard RISE"); }

inline Message sysex (std::initializer_list<std::uint8_t> body)
{
    Message m { 0xf0, 0x00, 0x21, 0x10, 0x78 };
    m.insert (m.end(), body);
    m.push_back (0xf7);
    return m;
}

inline Message identify()                 { return sysex ({ 0x3f }); }
inline Message readParam (std::uint8_t p) { return sysex ({ 0x3c, p }); }
inline Message setParam (std::uint8_t p, int v) { return sysex ({ 0x3d, p, (std::uint8_t) (v & 0x7f) }); }

inline void append (Messages& to, const Messages& from) { to.insert (to.end(), from.begin(), from.end()); }

// NRPN on channel 1 with data entry LSB 0, as Dashboard sends it.
inline Messages nrpn (std::uint8_t number, int value)
{
    return { { 0xb0, 0x63, 0x00 }, { 0xb0, 0x62, number }, { 0xb0, 0x06, (std::uint8_t) (value & 0x7f) }, { 0xb0, 0x26, 0x00 } };
}

// RPN on a 0-based channel.
inline Messages rpn (int channel, std::uint8_t number, int value)
{
    const auto cc = (std::uint8_t) (0xb0 | (channel & 0x0f));
    return { { cc, 0x65, 0x00 }, { cc, 0x64, number }, { cc, 0x06, (std::uint8_t) (value & 0x7f) }, { cc, 0x26, 0x00 } };
}

inline Messages octave (int o)           { return nrpn (2, o + 4); }
inline Messages transpose (int t)        { return nrpn (3, t + 64); }
inline Messages pressTracking (int v)    { return nrpn (4, v); }
inline Messages sleepTime (int v)        { return { sysex ({ 0x3d, 0x26, (std::uint8_t) (v & 0x7f), (std::uint8_t) ((v >> 7) & 0x7f) }) }; }

// The UI sets glide and slide tracking together.
inline Messages glideSlideTracking (int v)
{
    auto m = nrpn (0, v);
    append (m, nrpn (7, v));
    return m;
}

enum class Mode { mpe, multi, single };

// Mode flag, MPE configuration (RPN 6), then pitch bend range (RPN 0), in Dashboard's order.
// MPE addresses the master and first member channel of the zone.
inline Messages midiMode (Mode mode, bool upperZone, int members, int channel, int bend)
{
    Messages m { setParam (0x0e, mode == Mode::mpe ? 1 : 0) };
    if (mode == Mode::mpe)
    {
        const int a = upperZone ? 14 : 1, b = upperZone ? 15 : 0;
        append (m, rpn (a, 6, members));
        append (m, rpn (b, 6, members));
        append (m, rpn (a, 0, bend));
        append (m, rpn (b, 0, bend));
    }
    else
    {
        const int ch = mode == Mode::multi ? 0 : channel - 1;
        append (m, rpn (ch, 6, mode == Mode::multi ? 16 : 1));
        append (m, rpn (ch, 0, bend));
    }
    return m;
}

//==============================================================================
/** Offsets into the 3a identify reply, counted from F0. It is the device's settings
    struct copied out raw. */
namespace offset
{
    enum : int
    {
        // model is "RISE1.00" on a RISE 2 (family plus format version), not the product name.
        model = 6, firmware = 22, subFirmware = 30, bleFirmware = 38, mac = 46, serial = 63,
        firstMember = 79, lastMember = 80, masterChannel = 81, mpeOn = 82, bendRange = 83,
        octave = 84, glideTracking = 85, pressTracking = 86, pedalCc = 88,
        faderLeftCc = 92, faderMiddleCc = 96, faderRightCc = 100, xyXCc = 102, xyYCc = 103,
        glideSens = 105, slideSens = 106, pressSens = 107, strikeSens = 108,
        sleepLo = 111, sleepHi = 112, liftSens = 148, slideTracking = 149, slideCc = 150,
        slideMode = 151, fixedVelocity = 152, fixedVelocityValue = 153,
        strikeMin = 154, strikeMax = 155, pressMin = 156, pressMax = 157, liftMin = 158, liftMax = 159,
        semitoneLegato = 160, portForwarding = 161, portThrottle = 162, transpose = 207, battery = 209,
        replySize = 224
    };
}

/** A parsed identify reply. */
struct State
{
    std::vector<std::uint8_t> raw;

    static bool isIdentifyReply (const std::uint8_t* d, int size)
    {
        return size >= offset::replySize && d[0] == 0xf0 && d[1] == 0x00 && d[2] == 0x21 && d[3] == 0x10
            && d[4] == 0x78 && d[5] == 0x3a;
    }

    bool valid() const { return raw.size() >= (size_t) offset::replySize; }
    int at (int o) const { return valid() ? raw[(size_t) o] : 0; }

    juce::String text (int o, int maxLen) const
    {
        juce::String s;
        for (int i = o; i < o + maxLen && at (i) != 0; ++i)
            s << (juce::juce_wchar) at (i);
        return s;
    }

    Mode mode() const
    {
        if (at (offset::mpeOn) != 0) return Mode::mpe;
        const auto f = at (offset::firstMember), l = at (offset::lastMember), m = at (offset::masterChannel);
        return (f == l && l == m) ? Mode::single : Mode::multi;
    }

    bool upperZone() const { return at (offset::masterChannel) == 15 && mode() == Mode::mpe; }
    int  members() const   { return at (offset::lastMember) - at (offset::firstMember) + 1; }
    int  channel() const   { return at (offset::masterChannel) + 1; }
    int  sleepTime() const { return at (offset::sleepLo) | (at (offset::sleepHi) << 7); }
};

} // namespace rise2
