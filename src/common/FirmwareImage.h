// FirmwareImage - parse / validate / identify a ROLI firmware .syx (or .syx.zip), with no device
// involved. Shared by the seaprobe `flash` tool and the GUI Flash panel so there is ONE source of
// truth for what a valid BLOCKS firmware image is and which device it targets.
//
// A BLOCKS firmware .syx is a run of `F0 00 21 10 47 <op> ... F7` messages. The first message is a
// keyed-ASCII manifest carrying `V:` (version) and `I:` (target image id, e.g. "seaboard"). Each
// message later becomes one `firmwareUpdatePacket` (packetData = the bytes after the 5-byte
// `F0 00 21 10 47` prefix, up to but not including the trailing `F7`). This mapping was confirmed
// byte-for-byte against a BLE capture of ROLI Dashboard (see FIRMWARE.md / seaprobe runFlash).
//
// This header does NOT transmit anything and never touches a device - it only reads bytes.

#pragma once

#include <juce_core/juce_core.h>
#include <roli_blocks_basics/roli_blocks_basics.h>   // roli::Block::Type (for expectedImageFor)

#include <cstdint>
#include <vector>

namespace grab
{
    struct FirmwareImage
    {
        std::vector<std::vector<juce::uint8>> messages;   // each is a full F0..F7 0x47 message
        juce::String imageId;      // manifest "I:" (target image, e.g. "seaboard") - may be empty
        juce::String version;      // manifest "V:" - may be empty
        juce::String sourceName;   // the file (or product) this came from, for display
        juce::String error;        // non-empty => parse/validate failed; `ok()` is false
        juce::int64  totalBytes = 0;

        bool ok() const { return error.isEmpty() && ! messages.empty(); }
        int  packetCount() const { return (int) messages.size(); }

        // Hex of one message (for the "show the exact bytes" preview).
        juce::String messageHex (int index) const
        {
            if (index < 0 || index >= (int) messages.size()) return {};
            const auto& m = messages[(size_t) index];
            juce::String s;
            for (size_t i = 0; i < m.size(); ++i)
                s << juce::String::toHexString ((int) m[i]).paddedLeft ('0', 2).toUpperCase() << (i + 1 < m.size() ? " " : "");
            return s;
        }
    };

    // The image id a given connected BLOCKS device expects (empty = we don't have a mapping for it,
    // so we can't vouch that any image is correct for it). Mirrors seaprobe's expectedImageFor.
    inline juce::String expectedImageFor (roli::Block::Type t)
    {
        switch (t)
        {
            case roli::Block::seaboardBlock:         return "seaboard";
            case roli::Block::liveBlock:
            case roli::Block::loopBlock:
            case roli::Block::touchBlock:
            case roli::Block::developerControlBlock: return "control";
            case roli::Block::lumiKeysBlock:         return "lightkey";   // LUMI Keyboard / Piano M
            case roli::Block::lightPadBlock:         return "padmini";    // Lightpad (Block) M
            default:                                 return {};
        }
    }

    namespace detail
    {
        // Split a raw byte blob into complete F0..F7 messages.
        inline std::vector<std::vector<juce::uint8>> splitSysex (const juce::uint8* d, size_t n)
        {
            std::vector<std::vector<juce::uint8>> out;
            for (size_t i = 0; i < n; )
            {
                if (d[i] != 0xF0) { ++i; continue; }
                size_t e = i + 1;
                while (e < n && d[e] != 0xF7) ++e;
                if (e >= n) break;
                out.emplace_back (d + i, d + e + 1);
                i = e + 1;
            }
            return out;
        }

        // Pull a keyed field (e.g. "V:") out of the first message's ASCII manifest.
        inline juce::String manifestField (const std::vector<juce::uint8>& m0, const juce::String& key)
        {
            juce::String ascii;
            for (size_t b = 6; b + 1 < m0.size(); ++b)
                ascii << (juce::juce_wchar) ((m0[b] >= 32 && m0[b] < 127) ? (int) m0[b] : (int) ' ');
            const auto idx = ascii.indexOf (key);
            if (idx < 0) return {};
            return ascii.substring (idx + key.length()).upToFirstOccurrenceOf (" ", false, false).trim();
        }
    }

    // Parse + validate a raw byte blob as a BLOCKS firmware image. On any problem, `error` is set and
    // `ok()` is false. Validation matches seaprobe exactly: every message must be `F0 00 21 10 47 ...`,
    // packetData must fit the 7-bit size field (<= 127 bytes), and every data byte must be <= 0x7F.
    inline FirmwareImage parseFirmwareBytes (const juce::uint8* data, size_t size, const juce::String& sourceName = {})
    {
        FirmwareImage img;
        img.sourceName = sourceName;

        img.messages = detail::splitSysex (data, size);
        if (img.messages.empty()) { img.error = "No SysEx (F0..F7) messages found."; return img; }

        for (size_t k = 0; k < img.messages.size(); ++k)
        {
            const auto& m = img.messages[k];
            if (m.size() < 7 || m[0] != 0xF0 || m[1] != 0x00 || m[2] != 0x21 || m[3] != 0x10 || m[4] != 0x47)
            { img.error = "Message " + juce::String ((int) k) + " is not a ROLI 0x47 firmware message. This is not a BLOCKS firmware image."; return img; }
            if (m.size() - 6 > 127)
            { img.error = "Message " + juce::String ((int) k) + " packet data (" + juce::String ((int) (m.size() - 6)) + " bytes) exceeds the 127-byte limit."; return img; }
            for (size_t b = 5; b + 1 < m.size(); ++b)
                if (m[b] > 0x7F)
                { img.error = "Message " + juce::String ((int) k) + " contains a data byte > 0x7F (corrupt image)."; return img; }
            img.totalBytes += (juce::int64) m.size();
        }

        img.version = detail::manifestField (img.messages.front(), "V:");
        img.imageId = detail::manifestField (img.messages.front(), "I:");
        return img;
    }

    // Load a .syx, or a .syx.zip (first .syx entry is extracted), then parse + validate it.
    inline FirmwareImage loadFirmwareFile (const juce::File& file)
    {
        FirmwareImage img;
        img.sourceName = file.getFileName();

        if (! file.existsAsFile())
        { img.error = "File not found: " + file.getFullPathName(); return img; }

        // A .syx.zip (what ROLI serves) holds the real .syx compressed - expand it in memory.
        if (file.getFileName().endsWithIgnoreCase (".zip"))
        {
            juce::ZipFile zip (file);
            int entryIndex = -1;
            for (int i = 0; i < zip.getNumEntries(); ++i)
                if (zip.getEntry (i)->filename.endsWithIgnoreCase (".syx")) { entryIndex = i; break; }
            if (entryIndex < 0 && zip.getNumEntries() > 0) entryIndex = 0;   // single-entry zip with an odd name
            if (entryIndex < 0) { img.error = "The zip has no .syx entry."; return img; }

            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (entryIndex));
            if (in == nullptr) { img.error = "Could not read the .syx out of the zip."; return img; }
            juce::MemoryBlock mb;
            in->readIntoMemoryBlock (mb);
            auto parsed = parseFirmwareBytes (static_cast<const juce::uint8*> (mb.getData()), mb.getSize(), file.getFileName());
            return parsed;
        }

        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) { img.error = "Could not read the file."; return img; }
        return parseFirmwareBytes (static_cast<const juce::uint8*> (mb.getData()), mb.getSize(), file.getFileName());
    }
}
