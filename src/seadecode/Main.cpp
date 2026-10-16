// seadecode — offline decoder for ROLI BLOCKS SysEx.
//
// Turns a hex dump (or a USB-MIDI capture exported from Wireshark/USBPcap) into
// decoded ROLI protocol messages: framing, direction, checksum check, and the
// inner device/config/topology messages. Portable, no JUCE.
//
// Usage:
//   seadecode F0 00 21 10 77 00 01 01 00 5D F7     # decode hex on the command line
//   seadecode --file dump.hex                       # hex from a file
//   seadecode --usbmidi --file usbcapdata.hex       # reassemble USB-MIDI first
//   tshark -r cap.pcapng -Y usb.capdata -T fields -e usb.capdata | seadecode --usbmidi -
//
// Input is parsed leniently: any hex digits are taken as bytes; separators,
// "0x" prefixes and newlines are ignored.

#include "sea_sysex.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::string readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
    {
        std::cerr << "seadecode: cannot open file: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string readStdin()
{
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}
} // namespace

int main (int argc, char* argv[])
{
    bool usbMidi = false;
    bool bleMidi = false;
    std::string hexText;
    bool haveInput = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];

        if (a == "--usbmidi")        { usbMidi = true; }
        else if (a == "--blemidi")   { bleMidi = true; }
        else if (a == "--file")      { if (i + 1 < argc) { hexText += "\n" + readFile (argv[++i]); haveInput = true; } }
        else if (a == "-")           { hexText += "\n" + readStdin(); haveInput = true; }
        else if (a == "--help" || a == "-h")
        {
            std::cout << "Usage: seadecode [--usbmidi | --blemidi] [--file <path> | - | <hex...>]\n"
                         "  --blemidi: each input LINE is one BLE-MIDI characteristic value.\n";
            return 0;
        }
        else                         { hexText += " " + a; haveInput = true; } // treat as hex
    }

    if (! haveInput)
        hexText = readStdin();

    std::vector<std::vector<std::uint8_t>> messages;

    if (bleMidi)
    {
        // Each non-empty line is one BLE-MIDI packet (ATT value).
        std::vector<std::vector<std::uint8_t>> packets;
        std::istringstream lines (hexText);
        std::string ln;
        while (std::getline (lines, ln))
        {
            auto p = sea::fromHex (ln);
            if (! p.empty())
                packets.push_back (std::move (p));
        }
        messages = sea::reassembleBleMidiSysex (packets);
    }
    else
    {
        const auto bytes = sea::fromHex (hexText);
        if (bytes.empty())
        {
            std::cerr << "seadecode: no hex bytes found on input.\n";
            return 1;
        }
        messages = usbMidi ? sea::reassembleUsbMidiSysex (bytes.data(), bytes.size())
                           : sea::splitSysex (bytes.data(), bytes.size());

        if (messages.empty() && ! usbMidi)
            messages.push_back (bytes);
    }

    std::cout << "seadecode: " << messages.size() << " message(s)\n\n";

    int n = 0;
    int bad = 0;
    for (const auto& m : messages)
    {
        std::cout << "[" << n++ << "] " << sea::decode (m.data(), m.size()) << "\n";
        if (! sea::checksumOk (m.data(), m.size()))
            ++bad;
    }

    if (bad > 0)
        std::cout << "(" << bad << " message(s) failed checksum)\n";

    return 0;
}
