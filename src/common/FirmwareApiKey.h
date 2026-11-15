// FirmwareApiKey - ROLI Dashboard's static firmware X-API-Key (deliberately public).
//
// ROLI Dashboard authenticates the firmware endpoints (my.roli.com / api.roli.com) with this static
// X-API-Key (a UUID, User-Agent "juce"). It is NOT a per-user login: with only this header the firmware
// catalogue downloads with NO ROLI account at all - the point for orphaned / NOS / used devices whose
// owner has no account, and for the window before ROLI's servers shut down. GRAB uses it as a fallback,
// after any logged-in ROLI Connect session.
//
// The key is ROLI's property. It is published here by a deliberate project decision so that every build
// (release binaries AND anyone compiling from source) can archive firmware while the servers are up. If
// you are repurposing this code, review that choice yourself.

#pragma once

#include <juce_core/juce_core.h>

namespace grab
{
    // ROLI Dashboard's static firmware X-API-Key. Account-free; sent as the `X-API-Key` request header.
    inline juce::String firmwareApiKey()
    {
        return "93754126-4b0e-409b-afed-5c90664edb9c";
    }
}
