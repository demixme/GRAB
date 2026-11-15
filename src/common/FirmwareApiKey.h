// FirmwareApiKey - the account-free firmware-download credential, kept OUT of the git-tracked source.
//
// ROLI Dashboard authenticates the firmware endpoints (my.roli.com / api.roli.com) with a static
// `X-API-Key` (a UUID, User-Agent "juce"). It is NOT a per-user login: with only this header, the
// firmware catalogue downloads with no ROLI account at all - which is the whole point for orphaned /
// NOS devices whose owner never had, or can no longer reach, an account, and for the window before
// ROLI's servers go dark. GRAB uses it ONLY as a fallback, after any logged-in ROLI Connect session.
//
// The key is ROLI's property. To honour "never commit the X-API-Key", the raw value does NOT live in
// this tracked header - it lives in `FirmwareApiKey.local.h` (gitignored), which this file includes if
// present. So a local build / release binary carries the key, but the repository never does. Copy
// `FirmwareApiKey.local.h.example` to `FirmwareApiKey.local.h` and paste the key to enable it. With no
// local header, firmwareApiKey() returns "" and GRAB simply falls back to session-only (account) auth.

#pragma once

#include <juce_core/juce_core.h>

#if __has_include("FirmwareApiKey.local.h")
 #include "FirmwareApiKey.local.h"
#endif

namespace grab
{
    // ROLI Dashboard's static firmware X-API-Key, or "" if this build wasn't given one.
    inline juce::String firmwareApiKey()
    {
       #ifdef GRAB_FIRMWARE_API_KEY
        return juce::String (GRAB_FIRMWARE_API_KEY).trim();
       #else
        return {};
       #endif
    }
}
