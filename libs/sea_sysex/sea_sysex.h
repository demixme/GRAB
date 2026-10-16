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

//==============================================================================
// Checksum & bit-stream decoding (derived from roli BitPackingUtilities, and
// validated against real captured packets).

// ROLI packet checksum over `payload` (the bytes AFTER the topology-index byte,
// excluding the checksum byte itself). Returns the 7-bit checksum.
std::uint8_t checksum (const std::uint8_t* payload, std::size_t size) noexcept;

// Verify the trailing checksum byte of a full F0..F7 ROLI message.
bool checksumOk (const std::uint8_t* msg, std::size_t size) noexcept;

// LSB-first, 7-bits-per-byte reader (mirror of roli Packed7BitArrayReader).
class BitReader
{
public:
    BitReader (const std::uint8_t* d, std::size_t numBytes) noexcept;
    int remaining() const noexcept { return totalBits - bitsReadInCurrentByte; }
    std::uint32_t read (int numBits) noexcept;

private:
    const std::uint8_t* data;
    int totalBits;
    int bitsReadInCurrentByte = 0;
};

// Full human-readable decode of one raw MIDI message (F0..F7 for ROLI sysex):
// framing, direction, checksum result, and each inner protocol message decoded
// (device commands, config get/set/sync/update, topology w/ serial+battery,
// version/name). Unknown inner messages are reported by type + remaining bits.
std::string decode (const std::uint8_t* msg, std::size_t size);

// Reassemble a stream of 32-bit USB-MIDI event packets (as seen in a USBPcap /
// Wireshark capture of a USB-MIDI device) into complete SysEx messages.
std::vector<std::vector<std::uint8_t>> reassembleUsbMidiSysex (const std::uint8_t* data, std::size_t size);

// Split a flat byte buffer into complete F0..F7 SysEx messages (ignores bytes
// outside a sysex frame). Useful for already-reassembled MIDI dumps.
std::vector<std::vector<std::uint8_t>> splitSysex (const std::uint8_t* data, std::size_t size);

// Reassemble BLE-MIDI packets into SysEx messages. Each element of `packets` is
// one BLE-MIDI characteristic value (ATT write or notification): a header byte
// followed by timestamp-interleaved MIDI. SysEx may span several packets. Used
// to decode a BLE capture (the ROLI protocol is identical over BLE and USB).
std::vector<std::vector<std::uint8_t>>
    reassembleBleMidiSysex (const std::vector<std::vector<std::uint8_t>>& packets);

} // namespace sea
