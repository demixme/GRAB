// DeviceProfile — build the anonymized device-profile JSON described in
// DEVICE-PROFILES.md from a connected block. Shared so the GUI (and later the
// CLI) can both produce it. The serial is hashed, never sent raw.

#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "ConfigOverrides.h"

namespace grab
{

inline juce::String profileSchemaVersion() { return "1"; }
inline juce::String toolName()             { return "GRAB"; }
inline juce::String toolVersion()          { return "0.1.0"; }

inline juce::String hashSerial (const juce::String& serial)
{
    // Truncated SHA-256: stable per device, not sent raw. Enough to dedupe
    // submissions without identifying the owner.
    const auto hex = juce::SHA256 (serial.toUTF8()).toHexString();
    return "sha256:" + hex.substring (0, 16);
}

inline const char* blockTypeName (roli::Block::Type t)
{
    switch (t)
    {
        case roli::Block::unknown:               return "unknown";
        case roli::Block::lightPadBlock:         return "Lightpad";
        case roli::Block::liveBlock:             return "Live";
        case roli::Block::loopBlock:             return "Loop";
        case roli::Block::developerControlBlock: return "DeveloperControl";
        case roli::Block::touchBlock:            return "Touch";
        case roli::Block::seaboardBlock:         return "Seaboard";
        case roli::Block::lumiKeysBlock:         return "LUMI Keys";
        case roli::Block::lightpadProBlock:      return "Lightpad Pro";
        case roli::Block::airwave:               return "Airwave";
        case roli::Block::roliPiano49:           return "ROLI Piano 49";
    }
    return "?";
}

inline const char* configTypeName (roli::Block::ConfigMetaData::ConfigType t)
{
    using CT = roli::Block::ConfigMetaData::ConfigType;
    switch (t)
    {
        case CT::integer:  return "int";
        case CT::floating: return "float";
        case CT::boolean:  return "bool";
        case CT::colour:   return "colour";
        case CT::options:  return "options";
    }
    return "?";
}

/** Build the device-profile `var` for one block. `block` is non-const because the
    config accessors are non-const. `transport` is "USB"/"BLE". */
inline juce::var buildDeviceProfile (roli::Block& block, const juce::String& transport,
                                     const juce::String& notes = {})
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", 1);
    root->setProperty ("submittedAt", juce::Time::getCurrentTime().toISO8601 (true));

    auto* tool = new juce::DynamicObject();
    tool->setProperty ("name", toolName());
    tool->setProperty ("version", toolVersion());
    root->setProperty ("tool", juce::var (tool));

    auto* dev = new juce::DynamicObject();
    dev->setProperty ("family", "BLOCKS");
    dev->setProperty ("type", blockTypeName (block.getType()));
    dev->setProperty ("description", block.getDeviceDescription());
    dev->setProperty ("firmware", block.versionNumber);
    dev->setProperty ("transport", transport);
    dev->setProperty ("serialHash", hashSerial (block.serialNumber));
    dev->setProperty ("isMaster", block.isMasterBlock());
    root->setProperty ("device", juce::var (dev));

    juce::Array<juce::var> items;
    juce::Array<juce::var> unknown;

    const auto maxId = block.getMaxConfigIndex();
    for (juce::uint32 id = 0; id < maxId; ++id)
    {
        auto meta = block.getLocalConfigMetaData (id);
        if (! meta.isActive)
            continue;
        seaprobe::applyConfigOverrides (meta);

        auto* it = new juce::DynamicObject();
        it->setProperty ("id", (int) meta.item);
        it->setProperty ("name", meta.name);
        it->setProperty ("value", block.getLocalConfigValue (id));
        it->setProperty ("min", meta.range.getStart());
        it->setProperty ("max", meta.range.getEnd());
        it->setProperty ("type", configTypeName (meta.type));
        it->setProperty ("group", meta.group);
        it->setProperty ("active", meta.isActive);

        if (meta.type == roli::Block::ConfigMetaData::ConfigType::options)
        {
            juce::Array<juce::var> opts;
            for (int o = 0; o < roli::Block::ConfigMetaData::numOptionNames; ++o)
                if (meta.optionNames[o].isNotEmpty())
                    opts.add (meta.optionNames[o]);
            it->setProperty ("options", opts);
        }

        items.add (juce::var (it));
        if (meta.name.isEmpty())
            unknown.add ((int) meta.item);
    }

    root->setProperty ("configItems", items);
    root->setProperty ("unknownConfigIds", unknown);
    root->setProperty ("notes", notes);

    return juce::var (root);
}

inline juce::String profileToJson (const juce::var& profile)
{
    return juce::JSON::toString (profile, false); // pretty
}

} // namespace grab
