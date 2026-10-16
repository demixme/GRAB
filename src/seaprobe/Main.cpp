/*
    seaprobe — Phase 2 read-only probe for a USB-connected ROLI Seaboard Block.

    Modes:
      seaprobe list              List every MIDI port (name + identifier), mark the
                                 USB Seaboard and any BLE port, then exit. No port opened.
      seaprobe raw   [seconds]   Open the USB Seaboard, drive the library handshake so
                                 the device streams, and dump every MIDI message as hex
                                 with timestamps. No decoding printed.
      seaprobe blocks [seconds]  As raw, plus decode topology (type, serial, firmware,
                                 battery) and dump every config item. (default mode)

    STRICTLY READ-ONLY. This program never writes a config value, uploads a program,
    saves a default, resets, or sends firmware. It only drives the library's own
    read/control traffic (requestTopology, begin/endAPIMode) and records everything.

    All traffic (in AND out) is logged to console and to logs/seaprobe-<time>.log.
*/

#include <juce_audio_devices/juce_audio_devices.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "LoggingMidi.h"

#include <iostream>

using namespace seaprobe;

//==============================================================================
static const char* blockTypeName (roli::Block::Type t)
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

static const char* configTypeName (roli::Block::ConfigMetaData::ConfigType t)
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

//==============================================================================
class Probe  : public roli::TopologySource::Listener
{
public:
    Probe (roli::PhysicalTopologySource& src, RawLogger& log, bool blocksModeToUse)
        : source (src), logger (log), blocksMode (blocksModeToUse)
    {
    }

    void topologyChanged() override
    {
        const auto topology = source.getCurrentTopology();
        logger.line ("--- topologyChanged: " + juce::String (topology.blocks.size()) + " block(s) ---");

        for (auto& block : topology.blocks)
            logBlock (block);

        if (blocksMode && ! topology.blocks.isEmpty() && ! configRequested)
        {
            firstBlock = topology.blocks.getFirst();
            logger.line ("Requesting factory config sync for '" + firstBlock->name + "'...");
            firstBlock->requestFactoryConfigSync();
            configRequested = true;
            configRequestedAtMs = juce::Time::getMillisecondCounterHiRes();
        }
    }

    bool didDumpConfig() const { return configDumped; }

    bool readyToDumpConfig() const
    {
        return blocksMode && configRequested && ! configDumped
            && juce::Time::getMillisecondCounterHiRes() - configRequestedAtMs > 2500.0;
    }

    void dumpConfig()
    {
        configDumped = true;
        if (firstBlock == nullptr)
            return;

        logger.line ("");
        logger.line ("================ CONFIG ITEMS for '" + firstBlock->name + "' ================");
        logger.line (juce::String ("id").paddedRight (' ', 4)
                     + juce::String ("name").paddedRight (' ', 24)
                     + juce::String ("value").paddedRight (' ', 12)
                     + juce::String ("range").paddedRight (' ', 16)
                     + juce::String ("type").paddedRight (' ', 9)
                     + juce::String ("active").paddedRight (' ', 8)
                     + "group");

        const auto maxId = firstBlock->getMaxConfigIndex();
        for (juce::uint32 id = 0; id < maxId; ++id)
        {
            const auto meta = firstBlock->getLocalConfigMetaData (id);

            // Only show items that exist (named) or that the device reported active.
            if (meta.name.isEmpty() && ! meta.isActive)
                continue;

            juce::String valueStr (meta.value);
            if (meta.type == roli::Block::ConfigMetaData::ConfigType::options)
            {
                const auto opt = meta.value - meta.range.getStart();
                if (opt >= 0 && opt < roli::Block::ConfigMetaData::numOptionNames
                    && meta.optionNames[opt].isNotEmpty())
                    valueStr << " (" << meta.optionNames[opt] << ")";
            }

            logger.line (juce::String ((int) meta.item).paddedRight (' ', 4)
                         + meta.name.paddedRight (' ', 24)
                         + valueStr.paddedRight (' ', 12)
                         + ("[" + juce::String (meta.range.getStart()) + ".." + juce::String (meta.range.getEnd()) + "]").paddedRight (' ', 16)
                         + juce::String (configTypeName (meta.type)).paddedRight (' ', 9)
                         + juce::String (meta.isActive ? "yes" : "no").paddedRight (' ', 8)
                         + meta.group);
        }
        logger.line ("=============================================================");
        logger.line ("NOTE: 'active' = reported by the device in this session. A dedicated");
        logger.line ("flash-persisted flag is not exposed by this API (see PROTOCOL.md).");
    }

private:
    void logBlock (const roli::Block::Ptr& block)
    {
        juce::String s;
        s << "  Block uid=" << juce::String (block->uid)
          << "  type=" << blockTypeName (block->getType())
          << " (" << block->getDeviceDescription() << ")"
          << "  name='" << block->name << "'"
          << "  serial=" << block->serialNumber
          << "  fw=" << block->versionNumber
          << "  battery=" << juce::String (juce::roundToInt (block->getBatteryLevel() * 100.0f)) << "%"
          << (block->isBatteryCharging() ? " (charging)" : "")
          << (block->isMasterBlock() ? "  [master]" : "");
        logger.line (s);
    }

    roli::PhysicalTopologySource& source;
    RawLogger& logger;
    bool blocksMode;

    roli::Block::Ptr firstBlock;
    bool configRequested = false;
    bool configDumped = false;
    double configRequestedAtMs = 0.0;
};

//==============================================================================
static int runList (RawLogger& logger)
{
    LoggingDeviceDetector detector (logger); // enumerates + logs in its ctor

    if (detector.getUsbSeaboardPairs().empty())
    {
        logger.line ("No USB Seaboard found. If it is only paired over Bluetooth, connect it");
        logger.line ("over USB (BLE is deliberately skipped).");
        return 1;
    }
    return 0;
}

static int runProbe (RawLogger& logger, bool blocksMode, int durationSeconds)
{
    LoggingDeviceDetector detector (logger);

    if (detector.getUsbSeaboardPairs().empty())
    {
        logger.line ("No USB Seaboard found (BLE skipped). Nothing to probe.");
        return 1;
    }

    roli::PhysicalTopologySource source (detector);
    Probe probe (source, logger, blocksMode);
    source.addListener (&probe);

    logger.line ("Activating topology source (this sends requestTopology + begins API mode)...");
    source.setActive (true);

    auto* mm = juce::MessageManager::getInstance();
    const auto endMs = juce::Time::getMillisecondCounterHiRes() + durationSeconds * 1000.0;

    // blocks mode: run until the config dump is done, then stop.
    // raw mode: run the whole window so the log captures a representative sample.
    while (juce::Time::getMillisecondCounterHiRes() < endMs)
    {
        mm->runDispatchLoopUntil (100);

        if (probe.readyToDumpConfig())
        {
            probe.dumpConfig();
            break;
        }
    }

    if (blocksMode && ! probe.didDumpConfig())
        logger.line ("WARNING: no config data arrived before timeout. Try a longer duration, "
                     "or check the USB connection.");

    logger.line ("Deactivating topology source.");
    source.removeListener (&probe);
    source.setActive (false);
    mm->runDispatchLoopUntil (200);
    return 0;
}

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String mode = (argc > 1) ? juce::String (argv[1]) : "blocks";
    int duration = (argc > 2) ? juce::String (argv[2]).getIntValue() : (mode == "raw" ? 15 : 12);
    if (duration <= 0) duration = 12;

    const auto logDir = juce::File::getCurrentWorkingDirectory().getChildFile ("logs");
    const auto logFile = logDir.getChildFile ("seaprobe-"
        + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".log");

    RawLogger logger (logFile);
    logger.line ("seaprobe Phase 2  mode=" + mode + "  duration=" + juce::String (duration) + "s");
    logger.line ("JUCE " + juce::SystemStats::getJUCEVersion() + "   log file: " + logFile.getFullPathName());
    logger.line ("");

    int rc = 0;
    if (mode == "list")
        rc = runList (logger);
    else if (mode == "raw")
        rc = runProbe (logger, /*blocksMode*/ false, duration);
    else if (mode == "blocks")
        rc = runProbe (logger, /*blocksMode*/ true, duration);
    else
    {
        std::cerr << "Unknown mode '" << mode << "'. Use: list | raw [s] | blocks [s]\n";
        rc = 2;
    }

    logger.line ("seaprobe done (rc=" + juce::String (rc) + ").");
    return rc;
}
