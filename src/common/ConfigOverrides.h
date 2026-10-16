// ConfigOverrides — app-level metadata fixes for config items that the library
// exposes as plain integers but which are really enumerations. roli_blocks_basics
// itself notes "Should be options" for some of these but ships them as integers,
// so we override the ConfigMetaData here rather than patching the vendored library.
//
// Add more entries as the option names are confirmed from ROLI Dashboard.

#pragma once

#include <roli_blocks_basics/roli_blocks_basics.h>

namespace seaprobe
{

/** If `meta` is a config item we have better metadata for, rewrite it in place
    (e.g. integer -> options with named choices). No-op otherwise. */
inline void applyConfigOverrides (roli::Block::ConfigMetaData& meta)
{
    using CT = roli::Block::ConfigMetaData::ConfigType;

    // Chord (id 24): confirmed from ROLI Dashboard. Value 0 = Off.
    static const char* const chordNames[] = {
        "Off", "Octave", "Fifth", "Major", "minor", "Sus4", "Sus2", "7th",
        "Major7", "minor7", "9th", "minor9", "Diminished", "Augmented", "m7b5"
    };

    const char* const* names = nullptr;
    int count = 0;

    switch ((roli::BlocksProtocol::BlockConfigId) meta.item)
    {
        case roli::BlocksProtocol::chord:
            names = chordNames;
            count = (int) (sizeof (chordNames) / sizeof (chordNames[0]));
            break;

        // TODO: scale (id 22) is also "Should be options" in the library — add its
        // names here once confirmed from Dashboard.

        default:
            return;
    }

    meta.type  = CT::options;
    meta.range = { 0, count - 1 };
    for (int i = 0; i < roli::Block::ConfigMetaData::numOptionNames; ++i)
        meta.optionNames[i] = (i < count) ? juce::String (names[i]) : juce::String();
}

} // namespace seaprobe
