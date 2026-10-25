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
#include "ConfigOverrides.h"
#include "Rise2.h"

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

        blocks = topology.blocks;
        for (auto& block : blocks)
            logBlock (block);

        if (blocksMode && ! blocks.isEmpty() && ! configRequested)
        {
            for (auto& block : blocks)
            {
                logger.line ("Requesting factory config sync for " + block->serialNumber + "...");
                block->requestFactoryConfigSync();
            }
            configRequested = true;
            configRequestedAtMs = juce::Time::getMillisecondCounterHiRes();
        }
    }

    bool didDumpConfig() const { return configDumped; }
    roli::Block::Ptr getFirstBlock() const { return blocks.isEmpty() ? nullptr : blocks.getFirst(); }

    roli::Block::Ptr getBlockBySerial (const juce::String& serial) const
    {
        for (auto& b : blocks)
            if (b->serialNumber.equalsIgnoreCase (serial))
                return b;
        return nullptr;
    }

    const roli::Block::Array& getBlocks() const { return blocks; }

    bool readyToDumpConfig() const
    {
        return blocksMode && configRequested && ! configDumped
            && juce::Time::getMillisecondCounterHiRes() - configRequestedAtMs > 2500.0;
    }

    void dumpConfig()
    {
        configDumped = true;
        for (auto& block : blocks)
            dumpConfigForBlock (block);
    }

    void dumpConfigForBlock (const roli::Block::Ptr& blockToDump)
    {
        if (blockToDump == nullptr)
            return;

        const auto firstBlock = blockToDump; // keep body below unchanged

        logger.line ("");
        logger.line ("====== CONFIG ITEMS for " + firstBlock->serialNumber
                     + " (" + firstBlock->getDeviceDescription() + ") ======");
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
            auto meta = firstBlock->getLocalConfigMetaData (id);
            seaprobe::applyConfigOverrides (meta); // e.g. name Chord's option values

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

    roli::Block::Array blocks;
    bool configRequested = false;
    bool configDumped = false;
    double configRequestedAtMs = 0.0;
};

//==============================================================================
static int runList (RawLogger& logger, seaprobe::Transport transport)
{
    LoggingDeviceDetector detector (logger, transport); // enumerates + logs in its ctor

    if (detector.getSelectedPairs().empty())
    {
        logger.line ("No Seaboard found for transport '" + juce::String (seaprobe::transportName (transport))
                     + "'. Try --transport usb or --transport ble, or check the connection.");
        return 1;
    }
    return 0;
}

static int runProbe (RawLogger& logger, seaprobe::Transport transport, bool blocksMode, int durationSeconds)
{
    LoggingDeviceDetector detector (logger, transport);

    if (detector.getSelectedPairs().empty())
    {
        logger.line ("No Seaboard found for the selected transport. Nothing to probe.");
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
// Wait (pumping the message loop) until the first block is present and its config
// has been synced, or timeout. Returns the block (or nullptr).
static roli::Block::Ptr waitForBlockAndConfig (Probe& probe, juce::MessageManager* mm, double timeoutMs)
{
    const auto endMs = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    while (juce::Time::getMillisecondCounterHiRes() < endMs)
    {
        mm->runDispatchLoopUntil (100);
        if (probe.readyToDumpConfig())
            break;
    }
    return probe.getFirstBlock();
}

// Single-config write. Dry-run by default (builds + prints the exact bytes, sends
// nothing); only transmits when commit==true. This is the ONLY code path that can
// write to the device, and it writes exactly one config value via setConfig
// (RAM write; it does NOT send the flash-persist setConfigState command).
static int runSet (RawLogger& logger, seaprobe::Transport transport, const juce::String& serial,
                   juce::uint32 id, juce::int32 value, bool commit)
{
    LoggingDeviceDetector detector (logger, transport);
    if (detector.getSelectedPairs().empty())
    {
        logger.line ("No Seaboard found for the selected transport. Nothing to write.");
        return 1;
    }

    roli::PhysicalTopologySource source (detector);
    Probe probe (source, logger, /*blocksMode*/ true);
    source.addListener (&probe);
    source.setActive (true);

    auto* mm = juce::MessageManager::getInstance();
    waitForBlockAndConfig (probe, mm, 6000.0);

    // Select the target block: by --serial if given, else the only/first block.
    roli::Block::Ptr block;
    if (serial.isNotEmpty())
    {
        block = probe.getBlockBySerial (serial);
        if (block == nullptr)
            logger.line ("ERROR: no block with serial '" + serial + "'.");
    }
    else
    {
        block = probe.getFirstBlock();
        if (probe.getBlocks().size() > 1)
        {
            juce::String serials;
            for (auto& b : probe.getBlocks()) serials << " " << b->serialNumber;
            logger.line ("NOTE: " + juce::String (probe.getBlocks().size())
                         + " blocks connected; writing to the first (" + (block != nullptr ? block->serialNumber : "")
                         + "). Use --serial <S> to target another. Connected:" + serials);
        }
    }

    int rc = 0;
    if (block == nullptr)
    {
        logger.line ("ERROR: no block/config available; aborting write.");
        rc = 1;
    }
    else
    {
        const auto meta   = block->getLocalConfigMetaData (id);
        const auto before = block->getLocalConfigValue (id);
        const juce::String name (meta.name.isNotEmpty() ? meta.name : juce::String ("<id ") + juce::String ((int) id) + ">");

        logger.line ("");
        logger.line ("---- CONFIG WRITE " + juce::String (commit ? "(COMMIT)" : "(DRY-RUN)") + " ----");
        logger.line ("  block serial : " + block->serialNumber + "  fw " + block->versionNumber);
        logger.line ("  item         : id=" + juce::String ((int) id) + "  '" + name + "'  group=" + meta.group);
        logger.line ("  range        : [" + juce::String (meta.range.getStart()) + ".." + juce::String (meta.range.getEnd()) + "]");
        logger.line ("  current value: " + juce::String (before));
        logger.line ("  new value    : " + juce::String (value));

        if (value < meta.range.getStart() || value > meta.range.getEnd())
        {
            logger.line ("  REFUSED: new value is outside the device-reported range.");
            rc = 2;
        }
        else if (auto* conn = detector.getLastConnection())
        {
            if (! commit)
            {
                // Build the real packet but suppress the physical send.
                conn->setSuppressSends (true);
                block->setLocalConfigValue (id, value);
                mm->runDispatchLoopUntil (150);
                conn->setSuppressSends (false);

                const auto& bytes = conn->getLastSuppressed();
                logger.line ("  EXACT BYTES  : " + juce::String (sea::toHex (bytes)) + "   (NOT SENT)");
                logger.line ("  Re-run with --commit to transmit these bytes.");
            }
            else
            {
                logger.line ("  transmitting setConfig ...");
                block->setLocalConfigValue (id, value);
                mm->runDispatchLoopUntil (400);

                // Read back by re-syncing factory config.
                block->requestFactoryConfigSync();
                mm->runDispatchLoopUntil (2500);

                const auto after = block->getLocalConfigValue (id);
                logger.line ("  read-back    : " + juce::String (after)
                             + (after == value ? "   (OK, matches)" : "   (MISMATCH!)"));
                rc = (after == value) ? 0 : 3;
            }
        }
        else
        {
            logger.line ("ERROR: no live connection handle; aborting.");
            rc = 1;
        }
    }

    logger.line ("Deactivating topology source.");
    source.removeListener (&probe);
    source.setActive (false);
    mm->runDispatchLoopUntil (200);
    return rc;
}

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    const bool commit = args.contains ("--commit");

    // --transport usb|ble|auto  (default auto: prefer USB, fall back to BLE)
    seaprobe::Transport transport = seaprobe::Transport::Auto;
    {
        const auto ti = args.indexOf ("--transport");
        if (ti >= 0 && ti + 1 < args.size())
            transport = seaprobe::parseTransport (args[ti + 1]);
    }

    // --serial <S>: target a specific block (for 'set' with multiple blocks).
    juce::String serial;
    {
        const auto si = args.indexOf ("--serial");
        if (si >= 0 && si + 1 < args.size())
            serial = args[si + 1];
    }

    // Positional args, with flags (and their values) removed, so --transport/--serial/
    // --commit may appear anywhere on the command line.
    juce::StringArray pos;
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--commit") continue;
        if (args[i] == "--transport" || args[i] == "--serial") { ++i; continue; }
        pos.add (args[i]);
    }

    juce::String mode = pos.isEmpty() ? "blocks" : pos[0];

    const auto logDir = juce::File::getCurrentWorkingDirectory().getChildFile ("logs");
    const auto logFile = logDir.getChildFile ("seaprobe-"
        + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".log");

    RawLogger logger (logFile);
    logger.line ("seaprobe  mode=" + mode + "  transport=" + seaprobe::transportName (transport)
                 + (commit ? "  --commit" : ""));
    logger.line ("JUCE " + juce::SystemStats::getJUCEVersion() + "   log file: " + logFile.getFullPathName());
    logger.line ("");

    int rc = 0;
    if (mode == "list")
    {
        rc = runList (logger, transport);
    }
    else if (mode == "raw" || mode == "blocks")
    {
        int duration = (pos.size() > 1) ? pos[1].getIntValue() : (mode == "raw" ? 15 : 12);
        if (duration <= 0) duration = 12;
        rc = runProbe (logger, transport, /*blocksMode*/ mode == "blocks", duration);
    }
    else if (mode == "set")
    {
        // set <id> <value> [--commit] [--transport usb|ble]
        if (pos.size() < 3)
        {
            logger.line ("Usage: seaprobe set <configId> <value> [--commit] [--transport usb|ble] [--serial <S>]");
            rc = 2;
        }
        else
        {
            const auto id    = (juce::uint32) pos[1].getIntValue();
            const auto value = (juce::int32)  pos[2].getIntValue();
            rc = runSet (logger, transport, serial, id, value, commit);
        }
    }
    else if (mode == "rise2")
    {
        rc = seaprobe::rise2::run (logger, pos, commit);
    }
    else
    {
        std::cerr << "Unknown mode '" << mode << "'. Use: list | raw [s] | blocks [s] | set <id> <value> [--commit] | rise2 ...\n";
        rc = 2;
    }

    logger.line ("seaprobe done (rc=" + juce::String (rc) + ").");
    return rc;
}
