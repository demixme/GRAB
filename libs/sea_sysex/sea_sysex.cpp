#include "sea_sysex.h"

#include <cctype>
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
