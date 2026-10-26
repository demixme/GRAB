// Help — one short description per parameter, shown in the footer bar on hover. Keyed by the
// control's display name. These are plain-language starters; refine against ROLI Dashboard's
// intended meaning. Unknown names return empty (footer shows nothing).

#pragma once

#include <juce_core/juce_core.h>

#include <map>

namespace grab
{

inline juce::String helpForParameter (const juce::String& name)
{
    static const std::map<juce::String, juce::String> help =
    {
        // 5D touch
        { "Strike", "Strike sensitivity - how hard you press to reach full note velocity." },
        { "Glide",  "Glide sensitivity - how far a horizontal slide bends pitch. 0 = stepped keys, 127 = free continuous pitch." },
        { "Slide",  "Slide sensitivity - how vertical finger movement along a key maps to Slide (CC 74)." },
        { "Press",  "Pressure sensitivity - how much continuous pressure (aftertouch) a given force produces." },
        { "Lift",   "Lift sensitivity - how release speed maps to note-off velocity." },
        { "Fixed Velocity",       "Send one fixed velocity for every note, ignoring how hard you play." },
        { "Fixed Velocity Value", "The velocity sent when Fixed Velocity is on (1-127)." },
        { "Strike Minimum", "Lowest velocity the Strike curve can output." },
        { "Strike Maximum", "Highest velocity the Strike curve can output." },
        { "Press Minimum",  "Lowest value the Press curve can output." },
        { "Press Maximum",  "Highest value the Press curve can output." },
        { "Lift Minimum",   "Lowest value the Lift curve can output." },
        { "Lift Maximum",   "Highest value the Lift curve can output." },

        // MIDI settings
        { "MIDI Mode",          "MPE sends each note on its own channel for per-note expression; Multi/Single Channel use fixed channels." },
        { "MPE Zone",           "Which MPE zone is active - Lower (master channel 1) or Upper (master channel 16)." },
        { "No. MIDI Channels",  "How many member channels the MPE zone spreads notes across (more = more independent notes at once)." },
        { "MIDI Channel",       "The channel used in Single Channel mode." },
        { "MIDI Start Channel", "The first channel used in Multi/Single Channel mode." },
        { "Pitch Bend Range",   "Pitch-bend range in semitones. MPE commonly uses 48." },
        { "MIDI Port Forwarding", "Forward incoming MIDI from other devices through this one." },
        { "MIDI Port Throttling", "Thin dense MIDI output to fit slower connections." },
        { "MIDI Port Throttle",   "Thin dense MIDI output to fit slower connections." },

        // Pitch / play mode
        { "Octave",            "Shift the whole keyboard up or down in octaves." },
        { "Transpose",         "Shift the whole keyboard up or down in semitones." },
        { "Slide CC",          "Which MIDI CC the Slide (Y) dimension sends. 74 is the MPE standard." },
        { "Slide Mode",        "How Slide responds: Absolute (position) or Relative (movement from where you pressed)." },
        { "Piano Mode",        "Disable glide/slide so the surface plays like fixed piano keys." },
        { "Glide Rate",        "How quickly pitch glides between notes (portamento time)." },
        { "Glide Lock Enable", "Lock glide to the start note until released." },
        { "Semitone Legato",   "Quantise glide to whole semitones for legato lines." },
        { "Chord",             "Play a fixed chord shape from each single key press." },
        { "Glide Tracking",        "Which note's glide is sent on the master channel (Multi-Channel keeps it per-note)." },
        { "Slide Tracking",        "Which note's slide is sent on the master channel." },
        { "Pressure Tracking",     "How pressure is sent - poly aftertouch, or channel pressure for the last/highest/lowest note." },
        { "Press Tracking",        "How pressure is sent - poly aftertouch, or channel pressure for the last/highest/lowest note." },
        { "Glide & Slide Tracking","Which note's glide and slide are sent on the master channel." },
        { "Side-Button",       "What the side button switches - presets only, or presets and MIDI mode." },

        // MIDI CC assignments (RISE)
        { "Left Fader CC",   "Which MIDI CC the left touch fader sends." },
        { "Middle Fader CC", "Which MIDI CC the middle touch fader sends." },
        { "Right Fader CC",  "Which MIDI CC the right touch fader sends." },
        { "XY Pad X CC",     "Which MIDI CC the XY pad's horizontal axis sends." },
        { "XY Pad Y CC",     "Which MIDI CC the XY pad's vertical axis sends." },
        { "Pedal CC",        "Which MIDI CC a connected sustain/expression pedal sends." },
        { "Pedal Type",      "Whether the pedal acts as Sustain or Expression." },
        { "Pedal Control Change", "Which MIDI CC a connected pedal sends." },

        // Power / LUMI / Piano extras
        { "Sleep Time",        "Idle time before the device sleeps to save battery." },
        { "Key",               "The root key for scale and colour features." },
        { "Scale",             "The musical scale used by scale-aware modes." },
        { "Active Mode",       "Which of the device's saved modes is active." },
        { "Brightness",        "LED brightness." },
        { "Global Key Color",  "Colour of the non-root keys (32-bit ARGB)." },
        { "Root Key Color",    "Colour of the root-note keys (32-bit ARGB)." },
        { "Key Pitch Bend Amount", "Pitch-bend span for key-based bends." },
    };

    const auto it = help.find (name);
    return it != help.end() ? it->second : juce::String();
}

} // namespace grab
