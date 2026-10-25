// Diagnostics — build an anonymized "connection diagnostic": which MIDI ports were
// seen (USB + BLE), how the connection attempt ended, and the device if one
// connected. For reporting connection problems — especially unsupported devices
// like Seaboard RISE/GRAND — to the store, a file, or a prefilled GitHub issue.

#pragma once

#include <juce_core/juce_core.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "LoggingMidi.h"
#include "DeviceProfile.h"   // hashSerial, blockTypeName, toolName/toolVersion

namespace grab
{

/** Build the connection-diagnostic var. `detector` may be null (nothing scanned),
    `block` non-null only if a BLOCKS device actually connected. */
inline juce::var buildConnectionDiagnostic (const seaprobe::LoggingDeviceDetector* detector,
                                            roli::Block* block,
                                            const juce::String& requestedTransport,
                                            const juce::String& outcome,
                                            const juce::String& notes = {})
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", 1);
    root->setProperty ("kind", "connection-diagnostic");
    root->setProperty ("submittedAt", juce::Time::getCurrentTime().toISO8601 (true));

    auto* tool = new juce::DynamicObject();
    tool->setProperty ("name", toolName());
    tool->setProperty ("version", toolVersion());
    root->setProperty ("tool", juce::var (tool));

    auto* plat = new juce::DynamicObject();
    plat->setProperty ("os", juce::SystemStats::getOperatingSystemName());
    plat->setProperty ("is64bit", juce::SystemStats::isOperatingSystem64Bit());
    root->setProperty ("platform", juce::var (plat));

    auto* session = new juce::DynamicObject();
    session->setProperty ("requestedTransport", requestedTransport);
    session->setProperty ("activeTransport", detector != nullptr ? detector->getActiveTransport() : juce::String());
    session->setProperty ("outcome", outcome);

    juce::Array<juce::var> ins, outs;
    bool riseGrand = false;
    if (detector != nullptr)
    {
        for (const auto& p : detector->getAllPorts())
        {
            auto* pv = new juce::DynamicObject();
            pv->setProperty ("name", p.info.name);
            pv->setProperty ("identifier", p.info.identifier);
            pv->setProperty ("transport", p.transport);
            pv->setProperty ("seaboard", p.looksLikeSeaboard);
            (p.isInput ? ins : outs).add (juce::var (pv));

            if (p.looksLikeSeaboard
                && (p.info.name.containsIgnoreCase ("rise") || p.info.name.containsIgnoreCase ("grand")))
                riseGrand = true;
        }
    }
    session->setProperty ("looksLikeRiseGrand", riseGrand);
    root->setProperty ("session", juce::var (session));

    auto* ports = new juce::DynamicObject();
    ports->setProperty ("inputs", ins);
    ports->setProperty ("outputs", outs);
    root->setProperty ("ports", juce::var (ports));

    if (block != nullptr)
    {
        auto* dev = new juce::DynamicObject();
        dev->setProperty ("family", "BLOCKS");
        dev->setProperty ("type", blockTypeName (block->getType()));
        dev->setProperty ("firmware", block->versionNumber);
        dev->setProperty ("serialHash", hashSerial (block->serialNumber));
        dev->setProperty ("isMaster", block->isMasterBlock());
        root->setProperty ("device", juce::var (dev));
    }

    root->setProperty ("notes", notes);
    return juce::var (root);
}

} // namespace grab
