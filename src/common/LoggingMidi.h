// LoggingMidi — a custom roli DeviceDetector/DeviceConnection that:
//   * lets US choose the MIDI ports (USB Seaboard only; BLE deliberately skipped),
//   * logs every byte in and out, with timestamps, to console + a log file,
//   * otherwise behaves exactly like the stock MIDIDeviceConnection on the wire
//     (same ROLI sysex framing), so the library handles handshake / keepalive /
//     topology / config decoding for us.
//
// Read-only by construction: this file never calls setProgram, saveProgramAsDefault,
// factoryReset, sendFirmwareUpdatePacket, or any config *set*. It only forwards the
// library's own read/control traffic and records it.

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include <sea_sysex.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace seaprobe
{

//==============================================================================
/** Thread-safe timestamped logger to both the console and a log file. */
class RawLogger
{
public:
    explicit RawLogger (const juce::File& logFile)
        : startMs (juce::Time::getMillisecondCounterHiRes())
    {
        logFile.getParentDirectory().createDirectory();
        logFile.deleteFile();
        stream = logFile.createOutputStream();
        line ("# seaprobe log  " + juce::Time::getCurrentTime().toString (true, true, true, true));
    }

    void line (const juce::String& text)
    {
        const std::lock_guard<std::mutex> lock (mutex);
        const auto out = stamp() + " " + text;
        std::puts (out.toRawUTF8());
        std::fflush (stdout);
        if (stream != nullptr)
        {
            stream->writeText (out + "\n", false, false, nullptr);
            stream->flush();
        }
    }

    /** Log a raw MIDI message. dir is "IN " or "OUT". note is a short annotation. */
    void midi (const char* dir, const std::uint8_t* data, std::size_t size, const juce::String& note)
    {
        juce::String text;
        text << dir << " "
             << juce::String (sea::toHex (data, size))
             << "   (" << note << ")";
        line (text);
    }

private:
    juce::String stamp()
    {
        const auto rel = juce::Time::getMillisecondCounterHiRes() - startMs;
        return "[" + juce::String (rel, 1).paddedLeft (' ', 9) + "ms]";
    }

    std::mutex mutex;
    std::unique_ptr<juce::FileOutputStream> stream;
    double startMs;
};

//==============================================================================
/** A DeviceConnection that wraps a real MIDI in/out pair and logs all traffic.
    Mirrors the stock MIDIDeviceConnection's ROLI framing precisely.

    Note on keepalive logging: the host->device control commands (beginAPIMode,
    requestTopology, ...) are bit-packed by the library's internal, non-public
    HostPacketBuilder, so we do not reconstruct their exact bytes here. Their
    semantic events are logged by roli_blocks_basics itself via
    LOG_BLOCKS_CONNECTIVITY (e.g. "API Try <uid>" when beginAPIMode is sent),
    and their raw bytes appear in the OUT lines below. */
class LoggingDeviceConnection  : public roli::PhysicalTopologySource::DeviceConnection,
                                 private juce::MidiInputCallback
{
public:
    LoggingDeviceConnection (RawLogger& loggerToUse,
                             std::unique_ptr<juce::MidiOutput> out)
        : logger (loggerToUse), midiOut (std::move (out))
    {
    }

    /** Opens the MIDI input with this connection as the callback. Call once,
        right after construction. Returns false if the port could not be opened. */
    bool openInput (const juce::String& identifier)
    {
        midiIn = juce::MidiInput::openDevice (identifier, this);
        return midiIn != nullptr;
    }

    ~LoggingDeviceConnection() override
    {
        // Stop inbound callbacks before the base (and its handleMessageFromDevice)
        // are torn down, so no callback can run against a dangling handler.
        if (midiIn != nullptr)
            midiIn->stop();
    }

    /** When true, outbound messages are logged and captured but NOT transmitted.
        Used to show the exact bytes of a write before it is approved. */
    void setSuppressSends (bool shouldSuppress) { suppressSends = shouldSuppress; }

    /** The bytes of the most recent suppressed (dry-run) send. */
    const std::vector<std::uint8_t>& getLastSuppressed() const { return lastSuppressed; }

    bool sendMessageToDevice (const void* data, std::size_t dataSize) override
    {
        // Start listening lazily on the first outbound message. By this point the
        // library has already installed handleMessageFromDevice, so there is no
        // race between an early inbound packet and the handler being set.
        if (! started.exchange (true) && midiIn != nullptr)
            midiIn->start();

        const auto* bytes = static_cast<const std::uint8_t*> (data);
        const juce::String note (sea::annotate (bytes, dataSize));

        if (suppressSends)
        {
            lastSuppressed.assign (bytes, bytes + dataSize);
            logger.midi ("DRY", bytes, dataSize, "WOULD SEND (dry-run, NOT transmitted): " + note);
            return true;
        }

        logger.midi ("OUT", bytes, dataSize, note);

        if (midiOut != nullptr)
        {
            midiOut->sendMessageNow (juce::MidiMessage (data, (int) dataSize));
            return true;
        }
        return false;
    }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        const auto* data = message.getRawData();
        const auto size  = (std::size_t) message.getRawDataSize();

        logger.midi ("IN ", data, size, juce::String (sea::annotate (data, size)));

        // Forward ROLI packed-protocol messages (F0 00 21 10 77 ...) to the library,
        // header-stripped and footer-dropped, exactly as the stock connection does.
        if (size > sizeof (sea::kRoliHeader) + 1
            && std::memcmp (data, sea::kRoliHeader, sizeof (sea::kRoliHeader)) == 0)
        {
            if (handleMessageFromDevice != nullptr)
                handleMessageFromDevice (data + sizeof (sea::kRoliHeader),
                                         size - (sizeof (sea::kRoliHeader) + 1));
        }
    }

    RawLogger& logger;
    std::unique_ptr<juce::MidiInput> midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::atomic<bool> started { false };
    std::atomic<bool> suppressSends { false };
    std::vector<std::uint8_t> lastSuppressed;
};

//==============================================================================
/** Which transport to use for the Seaboard. Auto prefers USB, falls back to BLE. */
enum class Transport { Auto, Usb, Ble };

inline const char* transportName (Transport t)
{
    switch (t) { case Transport::Usb: return "USB"; case Transport::Ble: return "BLE"; default: return "Auto"; }
}

inline Transport parseTransport (const juce::String& s)
{
    if (s.equalsIgnoreCase ("usb")) return Transport::Usb;
    if (s.equalsIgnoreCase ("ble")) return Transport::Ble;
    return Transport::Auto;
}

//==============================================================================
/** Enumerates MIDI ports, logs them all, and exposes the selected-transport
    Seaboard pair(s) to the library. Transport is USB, BLE, or Auto (USB first). */
class LoggingDeviceDetector  : public roli::PhysicalTopologySource::DeviceDetector
{
public:
    struct Port
    {
        juce::MidiDeviceInfo info;
        bool isInput = false;
        bool looksLikeSeaboard = false;
        bool looksLikeBle = false;
        juce::String transport;   // "USB" | "BLE" | "other"
    };

    struct Pair
    {
        juce::MidiDeviceInfo input, output;
        juce::String description;
    };

    explicit LoggingDeviceDetector (RawLogger& loggerToUse, Transport transportToUse = Transport::Auto)
        : logger (loggerToUse), transport (transportToUse)
    {
        enumerateAndLog();
    }

    const std::vector<Pair>& getSelectedPairs() const { return selectedPairs; }
    juce::String getActiveTransport() const { return activeTransport; }

    /** Every MIDI port seen at the last enumeration (for diagnostics/reports). */
    const std::vector<Port>& getAllPorts() const { return allPorts; }

    /** The most recently opened connection (owned by the library; valid while the
        topology source is active). */
    LoggingDeviceConnection* getLastConnection() const { return lastConnection; }

    juce::StringArray scanForDevices() override
    {
        juce::StringArray result;
        for (auto& p : selectedPairs)
            result.add (p.description);
        return result;
    }

    roli::PhysicalTopologySource::DeviceConnection* openDevice (int index) override
    {
        if (index < 0 || index >= (int) selectedPairs.size())
            return nullptr;

        const auto& pair = selectedPairs[(std::size_t) index];
        logger.line ("Opening " + activeTransport + " Seaboard: IN='" + pair.input.name + "' [" + pair.input.identifier
                     + "]  OUT='" + pair.output.name + "' [" + pair.output.identifier + "]");

        auto out = juce::MidiOutput::openDevice (pair.output.identifier);
        if (out == nullptr)
        {
            logger.line ("ERROR: could not open MIDI OUTPUT '" + pair.output.name
                         + "'. Another process (ROLI Dashboard/Connect/driver, or a DAW) "
                           "likely holds the port. Not working around it.");
            return nullptr;
        }

        auto conn = std::make_unique<LoggingDeviceConnection> (logger, std::move (out));

        if (! conn->openInput (pair.input.identifier))
        {
            logger.line ("ERROR: could not open MIDI INPUT '" + pair.input.name
                         + "'. Another process likely holds the port, or it is a BLE port. "
                           "Not working around it.");
            return nullptr;
        }

        lastConnection = conn.get();
        return conn.release();
    }

    bool isLockedFromOutside() const override { return false; }

private:
    // Classify by IDENTIFIER, which is reliable; names are not (e.g. the BLE
    // Seaboard is just "Seaboard Block 3XU9" with no "BLE" in the name, and a
    // loose "ble" substring would wrongly match "GS Wavetable Synth").
    // The Seaboard RISE does not speak the BLOCKS protocol, it has its own page (Rise2Protocol.h).
    static bool isSeaboard (const juce::MidiDeviceInfo& d)
    {
        return (d.name.containsIgnoreCase ("seaboard") && ! d.name.containsIgnoreCase ("seaboard rise"))
            || d.identifier.containsIgnoreCase ("seaboardblock");
    }

    static bool isBle (const juce::MidiDeviceInfo& d)
    {
        return d.identifier.containsIgnoreCase ("blemidi")
            || d.identifier.containsIgnoreCase ("bthle")
            || d.identifier.containsIgnoreCase ("bthenum")
            || d.name.containsIgnoreCase ("bluetooth");
    }

    static bool isUsb (const juce::MidiDeviceInfo& d)
    {
       #if JUCE_WINDOWS
        // Windows MIDI identifiers encode the bus: USB endpoints look like usb#/vid_.
        return d.identifier.containsIgnoreCase ("usb#")
            || d.identifier.containsIgnoreCase ("vid_");
       #else
        // macOS CoreMIDI / Linux ALSA identifiers don't name the bus (they're a UID),
        // so there's no positive "USB" marker to match. A Seaboard that isn't BLE is
        // reached over USB. BLE is still detected positively by isBle().
        return ! isBle (d);
       #endif
    }

    static const char* transportTag (const juce::MidiDeviceInfo& d)
    {
        if (isBle (d)) return "BLE";
        if (isUsb (d)) return "USB";
        return "other";
    }

    void enumerateAndLog()
    {
        const auto inputs  = juce::MidiInput::getAvailableDevices();
        const auto outputs = juce::MidiOutput::getAvailableDevices();

        allPorts.clear();
        logger.line ("=== MIDI inputs (" + juce::String (inputs.size()) + ") ===");
        for (auto& d : inputs)
        {
            allPorts.push_back ({ d, true, isSeaboard (d), isBle (d), transportTag (d) });
            logger.line ("  IN   [" + juce::String (transportTag (d)).paddedRight (' ', 5) + "] name='"
                         + d.name + "'  id=[" + d.identifier + "]"
                         + (isSeaboard (d) ? "  <seaboard>" : ""));
        }

        logger.line ("=== MIDI outputs (" + juce::String (outputs.size()) + ") ===");
        for (auto& d : outputs)
        {
            allPorts.push_back ({ d, false, isSeaboard (d), isBle (d), transportTag (d) });
            logger.line ("  OUT  [" + juce::String (transportTag (d)).paddedRight (' ', 5) + "] name='"
                         + d.name + "'  id=[" + d.identifier + "]"
                         + (isSeaboard (d) ? "  <seaboard>" : ""));
        }

        // Build candidate pairs for each transport (in/out share an identifier).
        const auto usbPairs = buildPairs (inputs, outputs, /*wantBle*/ false);
        const auto blePairs = buildPairs (inputs, outputs, /*wantBle*/ true);

        // Choose per the requested transport; Auto prefers USB, falls back to BLE.
        if (transport == Transport::Usb)
        {
            selectedPairs = usbPairs; activeTransport = "USB";
        }
        else if (transport == Transport::Ble)
        {
            selectedPairs = blePairs; activeTransport = "BLE";
        }
        else // Auto
        {
            if (! usbPairs.empty()) { selectedPairs = usbPairs; activeTransport = "USB (auto)"; }
            else                    { selectedPairs = blePairs; activeTransport = "BLE (auto)"; }
        }

        logger.line ("=== Selected " + activeTransport + " Seaboard pairs ("
                     + juce::String ((int) selectedPairs.size()) + ")  [requested: "
                     + transportName (transport) + ";  USB avail=" + juce::String ((int) usbPairs.size())
                     + ", BLE avail=" + juce::String ((int) blePairs.size()) + "] ===");
        for (auto& p : selectedPairs)
            logger.line ("  USE  IN='" + p.input.name + "' [" + p.input.identifier
                         + "]  OUT='" + p.output.name + "' [" + p.output.identifier + "]");
    }

    std::vector<Pair> buildPairs (const juce::Array<juce::MidiDeviceInfo>& inputs,
                                  const juce::Array<juce::MidiDeviceInfo>& outputs,
                                  bool wantBle) const
    {
        std::vector<Pair> pairs;
        for (auto& in : inputs)
        {
            if (! isSeaboard (in) || isBle (in) != wantBle)
                continue;
            if (! wantBle && ! isUsb (in))   // USB must positively look like USB
                continue;

            for (auto& out : outputs)
            {
                if (isBle (out) != wantBle || (! wantBle && ! isUsb (out)))
                    continue;
                if (out.identifier == in.identifier || out.name == in.name)
                {
                    pairs.push_back (Pair { in, out, juce::String (wantBle ? "BLE:" : "USB:") + in.name });
                    break;
                }
            }
        }
        return pairs;
    }

    RawLogger& logger;
    Transport transport = Transport::Auto;
    juce::String activeTransport { "none" };
    std::vector<Pair> selectedPairs;
    std::vector<Port> allPorts;
    LoggingDeviceConnection* lastConnection = nullptr;
};

} // namespace seaprobe
