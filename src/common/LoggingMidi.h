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

#include "Anonymize.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
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
        const auto safe = grab::anonymize (data, size);   // no serial / MAC in the log
        juce::String text;
        text << dir << " "
             << juce::String (sea::toHex (safe.data(), safe.size()))
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
/** Cross-group DNA-link tracker.

    The stock roli library resolves topology connections per-connection-group, so a DNA cluster
    split across transports/radios never resolves (group A reports the connection but only knows
    its own device; group B knows the neighbour but not the connection). We see every incoming
    topology packet across ALL connections, so we parse them ourselves and keep one global map of
    device-index -> serial plus the connections each device reports. A link is "active" when both
    of its endpoints resolve to a known serial. This reflects DNA connect/disconnect in real time.

    Confirmed from hardware captures (2026-10-29): both Seaboards report the SAME connection with
    CONSISTENT global indices, and each reports its own serial - so together we have everything. */
class DnaLinkTracker
{
public:
    // Reads a ROLI packed stream: a sequence of 7-bit source bytes, bits packed LSB-first within and
    // across bytes (mirrors roli's internal Packed7BitArrayReader, which isn't a public symbol).
    struct BitReader
    {
        const std::uint8_t* d; int n, byte = 0, bit = 0;
        BitReader (const std::uint8_t* data, int numBytes) : d (data), n (numBytes) {}
        int remaining() const { return (n - byte) * 7 - bit; }
        juce::uint32 read (int num)
        {
            juce::uint32 v = 0; int sf = 0;
            while (num > 0 && byte < n)
            {
                const juce::uint32 cur = (juce::uint32) (d[byte] >> bit);
                const int av = 7 - bit;
                if (av > num) { v |= (cur & ((1u << num) - 1u)) << sf; bit += num; break; }
                v |= cur << sf; num -= av; sf += av; bit = 0; ++byte;
            }
            return v;
        }
    };

    // Feed a header-stripped device->host message (starts at the device-index byte, ends at the
    // 1-byte checksum - i.e. exactly what LoggingDeviceConnection forwards to the library).
    void feed (const std::uint8_t* d, int size)
    {
        if (size < 3) return;

        const int reporter = d[0] & 63;
        BitReader r (d + 1, size - 2);                  // after device-index, before checksum
        if (r.remaining() < 32) return;
        r.read (32);                                    // timestamp

        while (r.remaining() >= 7)
        {
            const juce::uint32 mt = r.read (7);
            if (mt == 0) break;
            if (mt != 0x01 && mt != 0x04) break;        // only topology (new / extend); unknown lengths -> stop

            r.read (8);                                 // protocol version
            const juce::uint32 nd = r.read (7);
            const juce::uint32 nc = r.read (8);
            if ((juce::uint32) r.remaining() < nd * 125u + nc * 24u) return;  // malformed

            std::map<int, std::string> devs;
            for (juce::uint32 i = 0; i < nd; ++i)
            {
                char s[17] = {};
                for (int j = 0; j < 16; ++j) s[j] = (char) r.read (7);
                const int idx = (int) r.read (7);
                r.read (5); r.read (1);                 // battery, charging
                devs[idx] = std::string (s);
            }
            std::vector<std::pair<int,int>> cons;
            for (juce::uint32 i = 0; i < nc; ++i)
            {
                const int d1 = (int) r.read (7);
                r.read (5);                             // port 1
                const int d2 = (int) r.read (7);
                r.read (5);                             // port 2
                cons.emplace_back (d1, d2);
            }

            const std::lock_guard<std::mutex> lk (mutex);
            for (auto& kv : devs) indexToSerial[kv.first] = kv.second;
            connsByReporter[reporter] = std::move (cons);   // replace (an empty list = disconnected)
            return;
        }
    }

    /** True if a connected device reports a DNA link to a neighbour we can't see on this transport
        (one endpoint resolves to a known serial, the other doesn't) - e.g. a BLE-only Seaboard
        DNA-joined to the USB-cabled one. Tells the user to switch to Auto/Bluetooth to see it. */
    bool hasUnseenNeighbour() const
    {
        const std::lock_guard<std::mutex> lk (mutex);
        for (auto& kv : connsByReporter)
            for (auto& c : kv.second)
                if ((indexToSerial.count (c.first) != 0) != (indexToSerial.count (c.second) != 0))
                    return true;
        return false;
    }

    /** Active DNA links as resolved {serialA, serialB} pairs (both endpoints known), deduped. */
    std::vector<std::pair<juce::String, juce::String>> links() const
    {
        const std::lock_guard<std::mutex> lk (mutex);
        std::vector<std::pair<juce::String, juce::String>> out;
        for (auto& kv : connsByReporter)
            for (auto& c : kv.second)
            {
                auto a = indexToSerial.find (c.first), b = indexToSerial.find (c.second);
                if (a == indexToSerial.end() || b == indexToSerial.end()) continue;
                juce::String s1 (a->second.c_str()), s2 (b->second.c_str());
                if (s2 < s1) std::swap (s1, s2);
                const std::pair<juce::String, juce::String> p { s1, s2 };
                if (std::find (out.begin(), out.end(), p) == out.end()) out.push_back (p);
            }
        return out;
    }

private:
    mutable std::mutex mutex;
    std::map<int, std::string> indexToSerial;
    std::map<int, std::vector<std::pair<int,int>>> connsByReporter;
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
                             std::unique_ptr<juce::MidiOutput> out,
                             DnaLinkTracker* dnaTracker = nullptr)
        : logger (loggerToUse), midiOut (std::move (out)), dna (dnaTracker)
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
            const auto* body = data + sizeof (sea::kRoliHeader);
            const int bodyLen = (int) (size - (sizeof (sea::kRoliHeader) + 1));
            if (dna != nullptr) dna->feed (body, bodyLen);   // stitch DNA links the library can't (cross-group)
            if (handleMessageFromDevice != nullptr)
                handleMessageFromDevice (body, (std::size_t) bodyLen);
        }
    }

    RawLogger& logger;
    std::unique_ptr<juce::MidiInput> midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;
    DnaLinkTracker* dna = nullptr;
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

    /** Active DNA links (resolved {serialA, serialB} pairs) stitched across all open connections. */
    std::vector<std::pair<juce::String, juce::String>> getDnaLinks() const { return dnaTracker.links(); }

    /** True when a connected block reports a DNA neighbour not visible on this transport (use Auto). */
    bool hasUnseenDnaNeighbour() const { return dnaTracker.hasUnseenNeighbour(); }
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
        logger.line ("Opening " + activeTransport + " Seaboard: IN='" + pair.input.name + "' [" + grab::anonymizeIdentifier (pair.input.identifier)
                     + "]  OUT='" + pair.output.name + "' [" + grab::anonymizeIdentifier (pair.output.identifier) + "]");

        auto out = juce::MidiOutput::openDevice (pair.output.identifier);
        if (out == nullptr)
        {
            logger.line ("ERROR: could not open MIDI OUTPUT '" + pair.output.name
                         + "'. Another process (ROLI Dashboard/Connect/driver, or a DAW) "
                           "likely holds the port. Not working around it.");
            return nullptr;
        }

        auto conn = std::make_unique<LoggingDeviceConnection> (logger, std::move (out), &dnaTracker);

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
    // Is this a device GRAB should try to connect to over the BLOCKS protocol
    // (0x77)? The whole ROLI family speaks it — Seaboard Block, Lightpad/Pad/Pro,
    // Live/Loop/Touch, LUMI, Airwave, the ROLI Piano (49) — so match ROLI gear
    // broadly rather than allowlisting each product.
    //
    // The Seaboard RISE is the one exception: it uses its own dialect and has its
    // own page (Rise2Component). Exclude it exactly as rise2::isRisePort() claims
    // it (name contains "Seaboard RISE"), so the two paths never fight over the
    // same device. (The GRAND has no "rise" in its name, so it falls through here,
    // gets tried, and is caught by the connect-timeout "help add it" message.)
    //
    // Match by the ROLI USB vendor id (2af4, shared across the range) on Windows,
    // where identifiers encode vid/pid; elsewhere (macOS/BLE identifiers are an
    // opaque UID) fall back to family names. A loose "ble"-style substring is
    // avoided — these tokens are specific enough not to catch unrelated ports.
    static bool isBlocksDevice (const juce::MidiDeviceInfo& d)
    {
        // The RISE and the GRAND speak the 0x78 dialect, not BLOCKS 0x77 - they're handled by
        // Rise2Component. Excluding them here keeps the BLOCKS detector from grabbing the port and
        // timing out on 0x77 (which is what left a connected GRAND showing no controls).
        if (d.name.containsIgnoreCase ("rise") || d.name.containsIgnoreCase ("grand"))
            return false;

        return d.identifier.containsIgnoreCase ("vid_2af4")        // ROLI USB vendor id (Windows)
            || d.identifier.containsIgnoreCase ("seaboardblock")
            || d.identifier.containsIgnoreCase ("pianofortynine")
            || d.name.containsIgnoreCase ("roli")
            || d.name.containsIgnoreCase ("seaboard")
            || d.name.containsIgnoreCase ("lightpad")
            || d.name.containsIgnoreCase ("lumi")
            || d.name.containsIgnoreCase ("piano")                 // ROLI Piano / Piano M (also id "pianofortynine")
            || d.name.containsIgnoreCase ("airwave")
            || d.name.containsIgnoreCase ("block");                // Lightpad/Live/Loop/Touch "… Block"
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
            allPorts.push_back ({ d, true, isBlocksDevice (d), isBle (d), transportTag (d) });
            logger.line ("  IN   [" + juce::String (transportTag (d)).paddedRight (' ', 5) + "] name='"
                         + d.name + "'  id=[" + grab::anonymizeIdentifier (d.identifier) + "]"
                         + (isBlocksDevice (d) ? "  <blocks>" : ""));
        }

        logger.line ("=== MIDI outputs (" + juce::String (outputs.size()) + ") ===");
        for (auto& d : outputs)
        {
            allPorts.push_back ({ d, false, isBlocksDevice (d), isBle (d), transportTag (d) });
            logger.line ("  OUT  [" + juce::String (transportTag (d)).paddedRight (' ', 5) + "] name='"
                         + d.name + "'  id=[" + grab::anonymizeIdentifier (d.identifier) + "]"
                         + (isBlocksDevice (d) ? "  <blocks>" : ""));
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
        else // Auto: prefer USB, fall back to BLE. (We tried opening both at once to stitch a DNA
             // cluster split across transports, but the stock roli library groups topology per
             // connection and never unifies the connection - reported on one transport - with the
             // neighbour's identity - reported on the other - so it added a wasteful double-open
             // with no benefit. See the Seaboard-BLE memory note.)
        {
            if (! usbPairs.empty()) { selectedPairs = usbPairs; activeTransport = "USB (auto)"; }
            else                    { selectedPairs = blePairs; activeTransport = "BLE (auto)"; }
        }

        logger.line ("=== Selected " + activeTransport + " BLOCKS pairs ("
                     + juce::String ((int) selectedPairs.size()) + ")  [requested: "
                     + transportName (transport) + ";  USB avail=" + juce::String ((int) usbPairs.size())
                     + ", BLE avail=" + juce::String ((int) blePairs.size()) + "] ===");
        for (auto& p : selectedPairs)
            logger.line ("  USE  IN='" + p.input.name + "' [" + grab::anonymizeIdentifier (p.input.identifier)
                         + "]  OUT='" + p.output.name + "' [" + grab::anonymizeIdentifier (p.output.identifier) + "]");
    }

    // Length of the shared leading substring - two endpoints of the SAME physical device share the
    // most of their identifier (vendor/product/instance prefix), diverging only at the in/out tail.
    static int commonPrefixLen (const juce::String& a, const juce::String& b)
    {
        const int n = juce::jmin (a.length(), b.length());
        int i = 0; while (i < n && a[i] == b[i]) ++i;
        return i;
    }

    std::vector<Pair> buildPairs (const juce::Array<juce::MidiDeviceInfo>& inputs,
                                  const juce::Array<juce::MidiDeviceInfo>& outputs,
                                  bool wantBle) const
    {
        std::vector<Pair> pairs;
        juce::StringArray usedOut;   // each output endpoint belongs to one device - claim it once

        for (auto& in : inputs)
        {
            if (! isBlocksDevice (in) || isBle (in) != wantBle)
                continue;
            if (! wantBle && ! isUsb (in))   // USB must positively look like USB
                continue;

            // Pick this input's matching output: prefer an exact identifier, else the unclaimed
            // same-named output whose identifier shares the longest prefix (i.e. the same physical
            // unit). Claiming each output once stops two identically-named USB Seaboards - whose
            // ports are all just "Seaboard Block" - from both binding to the first output, which
            // left only one device ever answering the topology handshake.
            const juce::MidiDeviceInfo* best = nullptr;
            int bestScore = -1;
            for (auto& out : outputs)
            {
                if (isBle (out) != wantBle || (! wantBle && ! isUsb (out)) || usedOut.contains (out.identifier))
                    continue;
                const bool idEq = (out.identifier == in.identifier);
                if (! idEq && out.name != in.name)
                    continue;   // not plausibly the same device
                const int score = (idEq ? (1 << 20) : 0) + commonPrefixLen (in.identifier, out.identifier);
                if (score > bestScore) { bestScore = score; best = &out; }
            }

            if (best != nullptr)
            {
                pairs.push_back (Pair { in, *best, juce::String (wantBle ? "BLE:" : "USB:") + in.name });
                usedOut.add (best->identifier);
            }
        }
        return pairs;
    }

    RawLogger& logger;
    Transport transport = Transport::Auto;
    juce::String activeTransport { "none" };
    std::vector<Pair> selectedPairs;
    DnaLinkTracker dnaTracker;
    std::vector<Port> allPorts;
    LoggingDeviceConnection* lastConnection = nullptr;
};

} // namespace seaprobe
