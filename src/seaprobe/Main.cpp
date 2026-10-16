/*
    seaprobe — Phase 1 build check.

    This file deliberately does NOTHING to the device. Its only job in Phase 1
    is to force the roli_blocks_basics module to compile and link against
    JUCE 8. No MIDI port is opened and no topology scan is started here.

    Device I/O (port listing, raw SysEx dump, topology detection, keepalive)
    arrives in Phase 2.
*/

#include <juce_core/juce_core.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include <iostream>

int main()
{
    std::cout << "seaprobe (Phase 1 build check)\n";
    std::cout << "JUCE version : " << juce::SystemStats::getJUCEVersion() << "\n";

    // Reference a roli_blocks_basics type at compile time so the module's
    // headers are genuinely exercised. This touches no hardware: parsing a
    // version string is pure value code, not a device call.
    const roli::BlocksVersion version { "1.4.3" };
    std::cout << "roli module  : linked OK (parsed sample version "
              << version.major << "." << version.minor << "." << version.patch << ")\n";

    std::cout << "Phase 1 OK: module compiled and linked. Device untouched.\n";
    return 0;
}
