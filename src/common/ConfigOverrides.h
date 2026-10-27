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

    // Glide/Slide Tracking (ids 30/31): the vendored SDK lists these as
    // { Multi-Channel, Last Played, Highest, Lowest } with min 1, so value 1 wrongly
    // read as "Multi-Channel". The device actually encodes value 1 = Last Played —
    // confirmed against ROLI Dashboard and a Piano-mode read (raw 1 shown as "Last
    // Played"), and matching our RISE note (1 Last Played, 2 Highest, 3 Lowest). The
    // Block adds Multi-Channel as the 4th value. Range (1..4) is kept as the device
    // reports it; combo index = value - range.start, so names are listed for values 1..4.
    static const char* const trackNames[] = { "Last Played", "Highest", "Lowest", "Multi-Channel" };

    switch ((roli::BlocksProtocol::BlockConfigId) meta.item)
    {
        case roli::BlocksProtocol::chord:
        {
            const int count = (int) (sizeof (chordNames) / sizeof (chordNames[0]));
            meta.type  = CT::options;
            meta.range = { 0, count - 1 };
            for (int i = 0; i < roli::Block::ConfigMetaData::numOptionNames; ++i)
                meta.optionNames[i] = (i < count) ? juce::String (chordNames[i]) : juce::String();
            return;
        }

        case roli::BlocksProtocol::xTrackingMode:   // Glide Tracking
        case roli::BlocksProtocol::yTrackingMode:   // Slide Tracking
        {
            meta.type = CT::options;   // keep the device-reported range (1..4)
            const int span = meta.range.getEnd() - meta.range.getStart() + 1;
            const int have = (int) (sizeof (trackNames) / sizeof (trackNames[0]));
            for (int i = 0; i < roli::Block::ConfigMetaData::numOptionNames; ++i)
                meta.optionNames[i] = (i < span && i < have) ? juce::String (trackNames[i]) : juce::String();
            return;
        }

        // TODO: scale (id 22) is also "Should be options" in the library — add its
        // names here once confirmed from Dashboard.

        default:
            return;
    }
}

} // namespace seaprobe
