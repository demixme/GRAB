// Anonymize: strip what identifies one physical unit from text that can leave the machine
// (log files people attach to issues, and the captures and reports GRAB submits).
//   * The MAC and serial in a ROLI 0x78 identify reply (RISE 2 and original RISE, see
//     Rise2Protocol.h). The bytes become 'X', so the field lengths still show.
//   * The per-unit serial in a ROLI 0x77 BLOCKS topology packet (Seaboard Block, Lightpad,
//     ROLI Piano, LUMI...), packed 7-bit in the bitstream. Redacted to 'X' and re-checksummed.
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
        // The fixed MAC+serial block (46..78) is the RISE layout. The GRAND reuses those bytes for
        // config (mode / bend / octave / transpose...) and keeps its serial up at offset 14 (redacted
        // by the version-region pass below), so blanking this block on a GRAND would destroy its config
        // in a shared capture. Only redact it for a RISE-layout reply.
        const bool grandReply = n >= 10 && out[6] == 'G' && out[7] == 'R' && out[8] == 'N' && out[9] == 'D';
        if (! grandReply)
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

    // A BLOCKS packed-protocol topology packet (device->host) carries each unit's 16-char serial in
    // its 7-bit packed bitstream (Seaboard Block, Lightpad, ROLI Piano, LUMI...). Redact those serial
    // bits to 'X' and re-checksum, so a shared capture keeps its structure but not the serial.
    // Frame: F0 00 21 10 77 <index> <packed payload> <checksum> F7; payload = out[6 .. n-3], 7 bits/byte.
    const bool blocksPacked = n >= 8 && d[0] == 0xf0 && d[1] == 0x00 && d[2] == 0x21 && d[3] == 0x10
        && d[4] == 0x77 && d[n - 1] == 0xF7 && (d[5] & 0x40) != 0;   // 0x40 bit = device->host
    if (blocksPacked)
    {
        const std::size_t payloadBytes = n - 8;         // out[6 .. n-3]
        std::uint8_t* p = out.data() + 6;
        const long totalBits = (long) payloadBytes * 7; // each payload byte holds 7 stream bits, LSB first

        auto getBit = [&] (long g) -> int { return (p[g / 7] >> (g % 7)) & 1; };
        auto setBit = [&] (long g, int v)
        {
            std::uint8_t& by = p[(std::size_t) (g / 7)];
            const int b = (int) (g % 7);
            by = (std::uint8_t) ((by & ~(1u << b)) | ((unsigned) (v & 1) << b));
        };
        auto readBits = [&] (long& g, int num) -> std::uint32_t
        {
            std::uint32_t v = 0;
            for (int i = 0; i < num && g < totalBits; ++i, ++g) v |= (std::uint32_t) getBit (g) << i;
            return v;
        };

        long g = 0;
        bool changed = false;
        if (totalBits >= 39)
        {
            g += 32;                                     // packetTimestamp (device messages lead with it)
            while (totalBits - g >= 7)
            {
                const std::uint32_t mt = readBits (g, 7);
                if (mt == 0) break;
                if (mt == 0x01 || mt == 0x04)            // deviceTopology / deviceTopologyExtend
                {
                    readBits (g, 8);                     // protocolVersion
                    const std::uint32_t nd = readBits (g, 7);   // device count
                    const std::uint32_t nc = readBits (g, 8);   // connection count
                    for (std::uint32_t dev = 0; dev < nd && g + 112 <= totalBits; ++dev)
                    {
                        for (int c = 0; c < 16; ++c)     // 16 x 7-bit serial chars -> 'X'
                        {
                            for (int bit = 0; bit < 7; ++bit)
                                setBit (g + bit, ('X' >> bit) & 1);
                            g += 7;
                        }
                        g += 7 + 5 + 1;                  // index(7), battery(5), charging(1)
                        changed = true;
                    }
                    g += (long) nc * 24;                 // connections: dev1(7) port1(5) dev2(7) port2(5)
                }
                else
                {
                    break;                               // unknown message length - stop walking safely
                }
            }
        }

        if (changed)                                     // re-checksum: c=size; c=c+(c*2+byte); &0x7f
        {
            std::uint8_t c = (std::uint8_t) payloadBytes;
            for (std::size_t i = 0; i < payloadBytes; ++i)
                c = (std::uint8_t) (c + (c * 2 + p[i]));
            out[n - 2] = (std::uint8_t) (c & 0x7f);
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
