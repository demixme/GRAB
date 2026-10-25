// LearnedNames — the "self-learn" pull. Parses GET /v1/names and applies the
// community-learned name/type/options to config items the tool doesn't have names
// for. Complements the hard-coded ConfigOverrides.h; never overrides a name the
// device/library already provides.

#pragma once

#include <juce_core/juce_core.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include <map>

namespace grab
{

struct LearnedEntry
{
    juce::String      name;
    juce::String      type;     // "int"|"bool"|"options"|...
    juce::StringArray options;
};

using LearnedMap = std::map<juce::uint32, LearnedEntry>;

/** Parse the JSON body of GET /v1/names into a map. Tolerant of junk/empty. */
inline LearnedMap parseNamesJson (const juce::String& json)
{
    LearnedMap out;
    const auto parsed = juce::JSON::parse (json);
    const auto* names = parsed.getProperty ("names", juce::var()).getDynamicObject();
    if (names == nullptr)
        return out;

    for (const auto& prop : names->getProperties())
    {
        const auto id = (juce::uint32) prop.name.toString().getIntValue();
        const auto* e = prop.value.getDynamicObject();
        if (e == nullptr)
            continue;

        LearnedEntry le;
        le.name = e->getProperty ("name").toString();
        le.type = e->getProperty ("type").toString();
        if (auto* opts = e->getProperty ("options").getArray())
            for (const auto& o : *opts)
                le.options.add (o.toString());

        out[id] = le;
    }
    return out;
}

/** Fill in a config item's metadata from learned data, without clobbering what the
    device/library already named. */
inline void applyLearnedNames (roli::Block::ConfigMetaData& meta, const LearnedMap& learned)
{
    using CT = roli::Block::ConfigMetaData::ConfigType;

    const auto it = learned.find (meta.item);
    if (it == learned.end())
        return;
    const auto& le = it->second;

    if (meta.name.isEmpty() && le.name.isNotEmpty())
        meta.name = le.name;

    // Promote to a named dropdown if the community knows the options and we don't.
    if (! le.options.isEmpty() && meta.type != CT::options)
    {
        meta.type  = CT::options;
        meta.range = { 0, le.options.size() - 1 };
        for (int i = 0; i < roli::Block::ConfigMetaData::numOptionNames; ++i)
            meta.optionNames[i] = (i < le.options.size()) ? le.options[i] : juce::String();
    }
}

} // namespace grab
