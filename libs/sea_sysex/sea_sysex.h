// sea_sysex — portable (no-JUCE) helpers for ROLI BLOCKS SysEx framing.
//
// Deliberately dependency-free so the same code can:
//   * annotate live traffic inside seaprobe, and
//   * later parse offline hex dumps captured with USBPcap / Wireshark.
//
// Phase 2 scope: framing only (manufacturer, namespace, sysex envelope).
// Full 7-bit bit-unpacking of the 0x77 packed protocol is deferred until we
// have cross-checked it against blocksd and a real capture.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sea
{

// ROLI MIDI SysEx manufacturer ID.
inline constexpr std::uint8_t kManufacturer[3] = { 0x00, 0x21, 0x10 };

// Full header that precedes the main packed protocol body:
//   F0 00 21 10 77 <packed 7-bit body ...> F7
inline constexpr std::uint8_t kRoliHeader[5] = { 0xF0, 0x00, 0x21, 0x10, 0x77 };

// The byte after the manufacturer ID selects a message namespace.
enum class Namespace : std::uint8_t
{
    packedProtocol = 0x77, // topology / config / control / touch (main protocol)
    serialDump     = 0x78, // block serial-number dump request/response
    resetMaster    = 0x49, // factory/reset special (we never send this)
    unknown        = 0x00
};

const char* namespaceName (Namespace ns) noexcept;

struct Framing
{
    bool          isSysex      = false; // starts 0xF0 and ends 0xF7
    bool          isRoli       = false; // manufacturer ID matches ROLI
    Namespace     ns           = Namespace::unknown;
    int           topologyIndex = -1;   // first body byte for packedProtocol, else -1
    std::size_t   bodySize     = 0;     // bytes between header and the trailing 0xF7
};

// Classify a raw MIDI message (as seen on the wire, including F0..F7 for sysex).
Framing classify (const std::uint8_t* data, std::size_t size) noexcept;

// Host->device device-control commands (the byte after the message-type byte in
// an 11-byte "F0 00 21 10 77 <idx> 01 <cmd> 00 <cksum> F7" packet). Values from
// roli BlocksProtocol::DeviceCommands, confirmed against observed traffic.
enum class HostCommand : std::uint8_t
{
    beginAPIMode          = 0x00,
    requestTopology       = 0x01,
    endAPIMode            = 0x02,
    ping                  = 0x03,
    saveProgramAsDefault  = 0x05,
    none                  = 0xFF
};

// If data is a host device-control message, return its command, else none.
HostCommand hostCommand (const std::uint8_t* data, std::size_t size) noexcept;
const char* hostCommandName (HostCommand c) noexcept;

// "F0 00 21 10 77 ..." — space-separated uppercase hex.
std::string toHex (const std::uint8_t* data, std::size_t size);
std::string toHex (const std::vector<std::uint8_t>& bytes);

// One-line human description, e.g.
//   "ROLI sysex ns=0x77 packedProtocol idx=0 body=12B"
//   "non-ROLI MIDI (3B)"
std::string annotate (const std::uint8_t* data, std::size_t size);

// Parse a hex-dump string ("F0 00 21 10 ..." / "f0,00,.." / "0xF0 ..") back to
// bytes, for replaying captured dumps offline. Non-hex separators are ignored.
std::vector<std::uint8_t> fromHex (const std::string& text);

} // namespace sea
