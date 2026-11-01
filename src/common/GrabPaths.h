// GrabPaths.h — where GRAB stores its own files (settings, presets, captures, profiles, logs).
#pragma once

#include <juce_core/juce_core.h>

namespace grab
{
    // GRAB's own per-user data folder:
    //   Windows: %APPDATA%\GRAB
    //   macOS:   ~/Library/Application Support/GRAB
    //   Linux:   ~/.config/GRAB
    // JUCE's userApplicationDataDirectory is %APPDATA% on Windows and ~/.config on Linux (both correct),
    // but just ~/Library on macOS - NOT ~/Library/Application Support - so on mac we append it ourselves
    // to land in the Apple-conventional location.
    inline juce::File dataDir()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile ("GRAB");
    }

    // One-time macOS migration: builds up to v0.1.92 wrote GRAB's data to ~/Library/GRAB (the
    // un-adjusted userApplicationDataDirectory). If that legacy folder exists and the new
    // ~/Library/Application Support/GRAB doesn't, move it so upgrading users keep their presets,
    // settings and captures. No-op on Windows/Linux and on every run after the first.
    inline void migrateLegacyDataDir()
    {
       #if JUCE_MAC
        const auto legacy  = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("GRAB");
        const auto current = dataDir();
        if (legacy != current && legacy.isDirectory() && ! current.exists())
        {
            current.getParentDirectory().createDirectory();   // ensure ~/Library/Application Support
            legacy.moveFileTo (current);                       // rename ~/Library/GRAB -> .../Application Support/GRAB
        }
       #endif
    }
}
