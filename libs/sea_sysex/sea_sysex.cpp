#include "sea_sysex.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace sea
{

const char* namespaceName (Namespace ns) noexcept
{
    switch (ns)
    {
        case Namespace::packedProtocol: return "packedProtocol";
        case Namespace::serialDump:     return "serialDump";
        case Namespace::resetMaster:    return "resetMaster";
        case Namespace::unknown:        break;
    }
    return "unknown";
}

Framing classify (const std::uint8_t* data, std::size_t size) noexcept
{
    Framing f;

    if (data == nullptr || size == 0)
        return f;

    f.isSysex = (size >= 2 && data[0] == 0xF0 && data[size - 1] == 0xF7);

    // ROLI message: F0 00 21 10 <ns> ... F7  (at least header + footer).
    if (f.isSysex
        && size >= 6
        && std::memcmp (data + 1, kManufacturer, sizeof (kManufacturer)) == 0)
    {
        f.isRoli = true;

        const auto nsByte = data[4];
        switch (nsByte)
        {
            case 0x77: f.ns = Namespace::packedProtocol; break;
            case 0x78: f.ns = Namespace::serialDump;     break;
            case 0x49: f.ns = Namespace::resetMaster;    break;
            default:   f.ns = Namespace::unknown;        break;
        }

        // body = everything between the 5-byte header and the trailing 0xF7.
        if (size >= 6)
            f.bodySize = size - 6;

        // For the packed protocol the first body byte is the topology index.
        if (f.ns == Namespace::packedProtocol && f.bodySize >= 1)
            f.topologyIndex = data[5];
    }

    return f;
}

HostCommand hostCommand (const std::uint8_t* data, std::size_t size) noexcept
{
    // F0 00 21 10 77 <idx> 01 <cmd> 00 <cksum> F7   (message type 0x01 = deviceCommand)
    const auto f = classify (data, size);
    if (f.isRoli && f.ns == Namespace::packedProtocol
        && size == 11 && data[6] == 0x01 && data[8] == 0x00)
    {
        switch (data[7])
        {
            case 0x00: return HostCommand::beginAPIMode;
            case 0x01: return HostCommand::requestTopology;
            case 0x02: return HostCommand::endAPIMode;
            case 0x03: return HostCommand::ping;
            case 0x05: return HostCommand::saveProgramAsDefault;
            default:   break;
        }
    }
    return HostCommand::none;
}

const char* hostCommandName (HostCommand c) noexcept
{
    switch (c)
    {
        case HostCommand::beginAPIMode:         return "beginAPIMode";
        case HostCommand::requestTopology:      return "requestTopology";
        case HostCommand::endAPIMode:           return "endAPIMode";
        case HostCommand::ping:                 return "ping (keepalive)";
        case HostCommand::saveProgramAsDefault: return "saveProgramAsDefault";
        case HostCommand::none:                 break;
    }
    return "";
}

std::string toHex (const std::uint8_t* data, std::size_t size)
{
    std::string out;
    out.reserve (size * 3);

    char buf[4];
    for (std::size_t i = 0; i < size; ++i)
    {
        std::snprintf (buf, sizeof (buf), "%02X", data[i]);
        if (i != 0)
            out.push_back (' ');
        out += buf;
    }
    return out;
}

std::string toHex (const std::vector<std::uint8_t>& bytes)
{
    return toHex (bytes.data(), bytes.size());
}

std::string annotate (const std::uint8_t* data, std::size_t size)
{
    const auto f = classify (data, size);

    char buf[128];

    if (f.isRoli)
    {
        const auto cmd = hostCommand (data, size);
        if (cmd != HostCommand::none)
        {
            std::snprintf (buf, sizeof (buf), "ROLI sysex ns=0x77 cmd=%s idx=%d",
                           hostCommandName (cmd), f.topologyIndex);
            return buf;
        }

        if (f.ns == Namespace::packedProtocol)
            std::snprintf (buf, sizeof (buf), "ROLI sysex ns=0x77 %s idx=%d body=%zuB",
                           namespaceName (f.ns), f.topologyIndex, f.bodySize);
        else
            std::snprintf (buf, sizeof (buf), "ROLI sysex ns=0x%02X %s body=%zuB",
                           (f.ns == Namespace::serialDump) ? 0x78
                               : (f.ns == Namespace::resetMaster) ? 0x49 : 0x00,
                           namespaceName (f.ns), f.bodySize);

        return buf;
    }

    if (f.isSysex)
    {
        std::snprintf (buf, sizeof (buf), "non-ROLI sysex (%zuB)", size);
        return buf;
    }

    std::snprintf (buf, sizeof (buf), "non-sysex MIDI (%zuB)", size);
    return buf;
}

//==============================================================================
std::uint8_t checksum (const std::uint8_t* payload, std::size_t size) noexcept
{
    std::uint8_t c = (std::uint8_t) size;
    for (std::size_t i = 0; i < size; ++i)
        c = (std::uint8_t) (c + (c * 2 + payload[i]));
    return c & 0x7f;
}

bool checksumOk (const std::uint8_t* msg, std::size_t size) noexcept
{
    // Layout: [5 header][1 index][payload...][1 checksum][1 F7]
    if (size < 8 || msg[size - 1] != 0xF7)
        return false;
    const std::size_t payloadLen = size - 8; // after index, before checksum
    return checksum (msg + 6, payloadLen) == msg[size - 2];
}

BitReader::BitReader (const std::uint8_t* d, std::size_t numBytes) noexcept
    : data (d), totalBits ((int) numBytes * 7)
{
}

std::uint32_t BitReader::read (int numBits) noexcept
{
    std::uint32_t value = 0;
    int bitsSoFar = 0;

    while (numBits > 0 && remaining() > 0)
    {
        const std::uint32_t valueInCurrentByte = (std::uint32_t) (*data >> bitsReadInCurrentByte);
        const int bitsAvailable = 7 - bitsReadInCurrentByte;

        if (bitsAvailable > numBits)
        {
            value |= ((valueInCurrentByte & (std::uint32_t) ((1 << numBits) - 1)) << bitsSoFar);
            bitsReadInCurrentByte += numBits;
            break;
        }

        value |= (valueInCurrentByte << bitsSoFar);
        numBits -= bitsAvailable;
        bitsSoFar += bitsAvailable;
        bitsReadInCurrentByte = 0;
        ++data;
        totalBits -= 7;
    }

    return value;
}

namespace
{
    const char* deviceCommandName (std::uint32_t c)
    {
        switch (c)
        {
            case 0x00: return "beginAPIMode";
            case 0x01: return "requestTopology";
            case 0x02: return "endAPIMode";
            case 0x03: return "ping";
            case 0x05: return "saveProgramAsDefault";
            default:   return "deviceCommand?";
        }
    }

    const char* configCommandName (std::uint32_t c)
    {
        switch (c)
        {
            case 0x00: return "setConfig";
            case 0x01: return "requestConfig";
            case 0x02: return "requestFactorySync";
            case 0x03: return "requestUserSync";
            case 0x04: return "updateConfig";
            case 0x05: return "updateUserConfig";
            case 0x06: return "setConfigState";
            case 0x07: return "factorySyncEnd";
            case 0x08: return "clusterConfigSync";
            case 0x09: return "factorySyncReset";
            default:   return "configCmd?";
        }
    }

    void line (std::string& out, const char* fmt, ...)
    {
        char buf[256];
        va_list ap;
        va_start (ap, fmt);
        std::vsnprintf (buf, sizeof (buf), fmt, ap);
        va_end (ap);
        out += "    ";
        out += buf;
        out += "\n";
    }

    // Decode one host->device inner message. Returns false to stop.
    bool decodeHostMessage (std::uint32_t mt, BitReader& r, std::string& out)
    {
        if (mt == 0x01) // deviceCommandMessage
        {
            const auto cmd = r.read (9);
            line (out, "deviceCommand: %s (0x%02X)", deviceCommandName (cmd), cmd);
            return true;
        }
        if (mt == 0x10) // configMessage (host)
        {
            const auto cc = r.read (4);
            if (cc == 0x00) { auto it = r.read (8); auto v = r.read (32); line (out, "config setConfig item=%u value=%d", it, (int) v); }
            else if (cc == 0x01) { auto v = r.read (32); auto it = r.read (8); line (out, "config requestConfig item=%u (pad=%u)", it, v); }
            else if (cc == 0x02) { line (out, "config requestFactorySync"); }
            else if (cc == 0x03) { line (out, "config requestUserSync"); }
            else { line (out, "config %s (0x%X) [remaining %d bits]", configCommandName (cc), cc, r.remaining()); return false; }
            return true;
        }
        line (out, "host message type 0x%02X [remaining %d bits]", mt, r.remaining());
        return false;
    }

    // Decode one device->host inner message. Returns false to stop.
    bool decodeDeviceMessage (std::uint32_t mt, BitReader& r, std::string& out)
    {
        if (mt == 0x01 || mt == 0x04) // deviceTopology / extend
        {
            const auto pv = r.read (8);
            const auto nd = r.read (7);
            const auto nc = r.read (8);
            line (out, "topology protoVer=%u devices=%u connections=%u", pv, nd, nc);

            for (std::uint32_t d = 0; d < nd; ++d)
            {
                char serial[17] = {};
                for (int i = 0; i < 16; ++i)
                    serial[i] = (char) r.read (7);
                const auto index = r.read (7);           // topology index of this device
                const auto batt  = r.read (5);
                const auto chg   = r.read (1);
                line (out, "  device[%u] index=%u serial='%s' battery=%u%% charging=%u",
                      d, index, serial, (unsigned) (batt * 100 / 31), chg);
            }
            for (std::uint32_t c = 0; c < nc; ++c)
            {
                const auto i1 = r.read (7); const auto p1 = r.read (5);
                const auto i2 = r.read (7); const auto p2 = r.read (5);
                line (out, "  connection dev%u:port%u <-> dev%u:port%u", i1, p1, i2, p2);
            }
            return true;
        }
        if (mt == 0x18) // configMessage (device)
        {
            const auto cc = r.read (4);
            if (cc == 0x04) { auto it = r.read (8); auto v = r.read (32); auto mn = r.read (32); auto mx = r.read (32);
                              line (out, "config updateConfig item=%u value=%d min=%d max=%d", it, (int) v, (int) mn, (int) mx); }
            else if (cc == 0x00) { auto it = r.read (8); auto v = r.read (32); line (out, "config setConfig item=%u value=%d", it, (int) v); }
            else if (cc == 0x07) { line (out, "config factorySyncEnd"); }    // no payload
            else if (cc == 0x09) { line (out, "config factorySyncReset"); }  // no payload
            else if (cc == 0x08) { line (out, "config clusterConfigSync"); } // no payload
            else { line (out, "config %s (0x%X) [remaining %d bits] (stopping)", configCommandName (cc), cc, r.remaining()); return false; }
            return true;
        }
        if (mt == 0x06 || mt == 0x07) // deviceVersion / deviceName
        {
            const auto idx = r.read (7);
            const auto len = r.read (7);
            std::string s;
            for (std::uint32_t i = 0; i < len && i < 64; ++i)
                s += (char) r.read (7);
            line (out, "%s idx=%u '%s'", mt == 0x06 ? "deviceVersion" : "deviceName", idx, s.c_str());
            return true;
        }
        line (out, "device message type 0x%02X [remaining %d bits] (not decoded)", mt, r.remaining());
        return false;
    }
}

std::string decode (const std::uint8_t* msg, std::size_t size)
{
    const auto f = classify (msg, size);
    if (! f.isRoli)
        return annotate (msg, size);

    std::string out = toHex (msg, size) + "\n";

    const auto idx = msg[5];
    const bool fromDevice = (idx & 0x40) != 0;
    const bool ok = checksumOk (msg, size);

    char hdr[128];
    std::snprintf (hdr, sizeof (hdr), "  %s  index=%u  ns=0x%02X  checksum=%s",
                   fromDevice ? "DEVICE->HOST" : "HOST->DEVICE",
                   (unsigned) (idx & 0x3F), (unsigned) msg[4], ok ? "OK" : "BAD");
    out += hdr;
    out += "\n";

    if (f.ns != Namespace::packedProtocol || size < 8)
        return out;

    BitReader r (msg + 6, size - 8);

    if (fromDevice)
    {
        const auto ts = r.read (32);
        line (out, "packetTimestamp=%u", ts);
    }

    while (r.remaining() >= 7)
    {
        const auto mt = r.read (7);
        if (mt == 0)
            break;
        const bool cont = fromDevice ? decodeDeviceMessage (mt, r, out)
                                     : decodeHostMessage (mt, r, out);
        if (! cont)
            break;
    }

    return out;
}

std::vector<std::vector<std::uint8_t>> splitSysex (const std::uint8_t* data, std::size_t size)
{
    std::vector<std::vector<std::uint8_t>> out;
    std::size_t i = 0;
    while (i < size)
    {
        if (data[i] == 0xF0)
        {
            std::size_t j = i;
            while (j < size && data[j] != 0xF7) ++j;
            if (j < size) // found terminating F7
            {
                out.emplace_back (data + i, data + j + 1);
                i = j + 1;
                continue;
            }
            break; // unterminated
        }
        ++i;
    }
    return out;
}

std::vector<std::vector<std::uint8_t>> reassembleUsbMidiSysex (const std::uint8_t* data, std::size_t size)
{
    // USB-MIDI event packets are 4 bytes: [CN|CIN][b0][b1][b2].
    // The Code Index Number (low nibble of byte 0) says how many MIDI bytes follow
    // and whether this is a sysex start/continue/end.
    std::vector<std::uint8_t> midi;
    midi.reserve (size);

    for (std::size_t i = 0; i + 4 <= size; i += 4)
    {
        const std::uint8_t cin = data[i] & 0x0F;
        const std::uint8_t* b = data + i + 1;

        switch (cin)
        {
            case 0x4:                       midi.insert (midi.end(), b, b + 3); break; // sysex start/continue
            case 0x5: case 0xF:             midi.push_back (b[0]);              break; // 1-byte (sysex end / single)
            case 0x6:                       midi.insert (midi.end(), b, b + 2); break; // sysex end, 2 bytes
            case 0x7:                       midi.insert (midi.end(), b, b + 3); break; // sysex end, 3 bytes
            default: break;                 // ignore non-sysex channel/system messages
        }
    }

    return splitSysex (midi.data(), midi.size());
}

std::vector<std::uint8_t> fromHex (const std::string& text)
{
    std::vector<std::uint8_t> out;
    int hi = -1;

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        // Skip a "0x" / "0X" prefix.
        if (text[i] == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X'))
        {
            ++i;
            continue;
        }

        const auto c = static_cast<unsigned char> (text[i]);
        if (! std::isxdigit (c))
            continue;

        const int nibble = std::isdigit (c) ? (c - '0')
                         : (std::tolower (c) - 'a' + 10);

        if (hi < 0)
        {
            hi = nibble;
        }
        else
        {
            out.push_back (static_cast<std::uint8_t> ((hi << 4) | nibble));
            hi = -1;
        }
    }

    return out;
}

} // namespace sea
