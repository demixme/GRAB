// Anonymize: strip what identifies one physical unit from text that can leave the machine
// (log files people attach to issues, and the captures and reports GRAB submits).
//   * The MAC and serial in a ROLI 0x78 identify reply (RISE 2 and original RISE, see
//     Rise2Protocol.h). The bytes become 'X', so the field lengths still show.
//   * The per-unit part of a port identifier: a Windows USB instance id (often the USB
//     serial) and any Bluetooth address. vid / pid and the transport tokens GRAB matches
//     on stay, so a log still shows how a port was classified.
//
// Only used for display and logging. Device matching always uses the real values.

#pragma once

#include "Rise2Protocol.h"

#include <cstdint>
#include <vector>

namespace grab
{

/** A copy of a MIDI message that is safe to log or share. */
inline std::vector<std::uint8_t> anonymize (const std::uint8_t* d, std::size_t n)
{
    std::vector<std::uint8_t> out (d, d + n);

    // An identify reply: F0 00 21 10 78 3A, MAC from 46 and serial up to 78.
    const bool identifyReply = n > (std::size_t) rise2::offset::firstMember
        && d[0] == 0xf0 && d[1] == 0x00 && d[2] == 0x21 && d[3] == 0x10 && d[4] == 0x78 && d[5] == 0x3a;

    if (identifyReply)
    {
        // Known serial + MAC block (RISE 2 / original RISE layout).
        for (int i = rise2::offset::mac; i < rise2::offset::firstMember; ++i)
            if (out[(size_t) i] != 0)
                out[(size_t) i] = 'X';

        // The header region (version slots) holds only a model string (kept, at offset 6) and
        // firmware versions on the RISE — but other models reuse these slots for a hardware/
        // serial code (e.g. the Seaboard GRAND's "SBG-..." at offset 14). Redact any field here
        // that isn't a plain dotted version, so an unknown-layout device can't leak an id.
        auto isVersionByte = [] (std::uint8_t c) { return (c >= '0' && c <= '9') || c == '.'; };
        for (int i = rise2::offset::extraVersion; i < rise2::offset::mac && i < (int) n; )
        {
            int j = i;
            bool looksVersion = true;
            while (j < rise2::offset::mac && j < (int) n && out[(size_t) j] != 0)
            {
                if (! isVersionByte (out[(size_t) j])) looksVersion = false;
                ++j;
            }
            if (j > i && ! looksVersion)              // a non-empty, non-version field -> redact
                for (int k = i; k < j; ++k) out[(size_t) k] = 'X';
            i = (j > i) ? j : i + 1;
            while (i < rise2::offset::mac && i < (int) n && out[(size_t) i] == 0) ++i;  // skip padding
        }
    }

    return out;
}

/** A port identifier that is safe to log or share. */
inline juce::String anonymizeIdentifier (const juce::String& id)
{
    auto s = id;

    // Windows USB: ...usb#vid_2af4&pid_xxxx[&mi_xx]#<instance id>#{class guid}
    const auto usb = s.indexOfIgnoreCase ("usb#");
    if (usb >= 0)
    {
        const auto a = s.indexOfChar (usb + 4, '#');
        const auto b = a >= 0 ? s.indexOfChar (a + 1, '#') : -1;
        if (b > a + 1)
            s = s.replaceSection (a + 1, b - a - 1, "x");
    }

    // Bluetooth address: exactly 12 hex digits in a row (not the last group of a GUID).
    juce::String out;
    for (int i = 0; i < s.length();)
    {
        int j = i;
        while (j < s.length() && juce::CharacterFunctions::getHexDigitValue (s[j]) >= 0)
            ++j;

        if (j - i == 12 && (i == 0 || s[i - 1] != '-'))
            out << juce::String::repeatedString ("x", 12);
        else
            out << s.substring (i, j);

        if (j < s.length())
            out << s[j];
        i = j + 1;
    }
    return out;
}

} // namespace grab
