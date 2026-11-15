// FlashPanel - the guided, BLOCKS-only firmware flash flow (Advanced -> Flash firmware).
//
// This is the first GUI feature that WRITES firmware to a device (the no-flash rule is lifted for the
// orphaned/NOS-device mission). It reuses the path proven byte-for-byte in `seaprobe flash`:
//   pick a source (download latest from ROLI, or a local .syx / .syx.zip)
//     -> parse + validate (grab::loadFirmwareFile)         [no device touched]
//     -> match-before-flash against the connected device   [a read only]
//     -> show the exact first/last packet bytes + manifest  [no device touched]
//     -> explicit brick-warning confirm
//     -> ACK-gated stream of every packet (sendFirmwareUpdatePacket), abort on timeout / ACK change
//     -> device commits + reboots (~30 s); reconnect to confirm the new version.
//
// Safety: it flashes ONLY the one selected block (never the DNA cluster). Once streaming starts, the
// engine runs to completion on its own shared_ptr even if this dialog is closed - a half-written image
// bricks a device, so we never abort a stream just because the window went away.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <roli_blocks_basics/roli_blocks_basics.h>

#include "../common/FirmwareImage.h"
#include "RoliArchivePanel.h"   // connectTokenCandidates() + fetchToFile() (shared with Download firmware)
#include "Theme.h"

#include <atomic>
#include <climits>
#include <memory>
#include <thread>
#include <vector>

// Streams a parsed BLOCKS firmware image to one block, ACK-gated, on the message thread. Held by a
// shared_ptr; in-flight ACK callbacks capture shared_from_this, so the stream completes even if the
// owning panel is destroyed mid-flash. All sendFirmwareUpdatePacket calls happen on the message thread.
class BlocksFlashEngine  : public std::enable_shared_from_this<BlocksFlashEngine>,
                           private juce::Timer
{
public:
    BlocksFlashEngine (roli::Block::Ptr blockToUse, std::vector<std::vector<juce::uint8>> packets)
        : block (std::move (blockToUse)), msgs (std::move (packets)) {}

    ~BlocksFlashEngine() override { stopTimer(); }

    std::function<void (int done, int total)> onProgress;   // message thread; cleared by the panel on close
    std::function<void (bool ok, juce::String message)> onDone;
    std::function<void (juce::String)> onLog;               // incidental notes (e.g. the first ACK code)

    void start()
    {
        next = 0; firstCode = INT_MIN; finished = false;
        startTimer (250);          // timeout watchdog
        sendNext();
    }

    bool isFinished() const { return finished; }

private:
    void sendNext()   // message thread
    {
        if (next >= msgs.size()) { finish (true, "All " + juce::String ((int) msgs.size()) + " packets acknowledged. The device will commit and reboot (~30 s)."); return; }

        const auto& m = msgs[next];
        awaiting = true;
        lastAck.store (INT_MIN);
        sentMs = juce::Time::getMillisecondCounterHiRes();

        auto self = shared_from_this();
        const bool sent = block->sendFirmwareUpdatePacket (
            m.data() + 5, (juce::uint8) (m.size() - 6),
            [self] (juce::uint8 code, juce::uint32 /*detail*/)
            {
                self->lastAck.store ((int) code);
                juce::MessageManager::callAsync ([self] { self->onAckArrived(); });
            });

        if (! sent)
            finish (false, "Failed to send packet " + juce::String ((int) next) + ".");
    }

    void onAckArrived()   // message thread
    {
        if (! awaiting) return;
        const int code = lastAck.load();
        if (code == INT_MIN) return;

        if (firstCode == INT_MIN)
        {
            firstCode = code;
            if (onLog) onLog ("device's first ACK code = " + juce::String (code)
                              + " (recorded so we learn the firmware-ACK semantics on a real flash).");
        }
        else if (code != firstCode)
        {
            finish (false, "The device returned a different ACK code (" + juce::String (code)
                           + ") at packet " + juce::String ((int) next) + " - flash aborted.");
            return;
        }

        awaiting = false;
        ++next;
        if (onProgress) onProgress ((int) next, (int) msgs.size());
        sendNext();
    }

    void timerCallback() override   // message thread
    {
        if (awaiting && juce::Time::getMillisecondCounterHiRes() - sentMs > 5000.0)
            finish (false, "No acknowledgement from the device (timeout) at packet " + juce::String ((int) next)
                           + ". The device may now need a Dashboard/BLE reflash to recover.");
    }

    void finish (bool ok, juce::String message)
    {
        if (finished) return;
        finished = true;
        awaiting = false;
        stopTimer();
        if (onDone) onDone (ok, std::move (message));
    }

    roli::Block::Ptr block;
    std::vector<std::vector<juce::uint8>> msgs;
    size_t next = 0;
    int firstCode = INT_MIN;
    std::atomic<int> lastAck { INT_MIN };
    bool awaiting = false;
    bool finished = false;
    double sentMs = 0.0;
};

//==============================================================================
class FlashPanel  : public juce::Component
{
public:
    // block   = the one selected target (never the whole cluster).
    // product = ROLI download product name for this device (empty => download disabled, local-only).
    // clusterSize = how many blocks are chained (for the "others untouched" note).
    FlashPanel (roli::Block::Ptr blockToUse, juce::String productToUse, int clusterSize)
        : block (std::move (blockToUse)), product (std::move (productToUse)), cluster (clusterSize)
    {
        title.setText ("Flash firmware", juce::dontSendNotification);
        title.setFont (grab::theme::type::fontAt (20.0f, 2));            // match Diagnose connection
        title.setColour (juce::Label::textColourId, grab::theme::accent);
        addAndMakeVisible (title);

        log.setMultiLine (true);
        log.setReadOnly (true);
        log.setScrollbarsShown (true);
        log.setCaretVisible (false);
        log.setFont (grab::theme::type::fontAt (15.0f));                 // match Diagnose connection body
        log.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        log.setColour (juce::TextEditor::textColourId,       grab::theme::textPrimary);
        log.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);
        addAndMakeVisible (log);

        downloadBtn.setButtonText ("Download latest from ROLI");
        downloadBtn.onClick = [this] { startDownload(); };
        downloadBtn.setEnabled (product.isNotEmpty());
        addAndMakeVisible (downloadBtn);

        chooseBtn.setButtonText ("Choose file...");
        chooseBtn.onClick = [this] { chooseLocal(); };
        addAndMakeVisible (chooseBtn);

        overrideToggle.setButtonText ("Flash anyway (image does not match this device)");
        overrideToggle.onClick = [this] { refreshFlashEnabled(); };
        overrideToggle.setVisible (false);
        addAndMakeVisible (overrideToggle);

        progress.setVisible (false);
        addAndMakeVisible (progress);

        flashBtn.setButtonText ("Flash now");
        flashBtn.setEnabled (false);
        flashBtn.onClick = [this] { confirmAndFlash(); };
        addAndMakeVisible (flashBtn);

        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this] { closeSelf(); };
        addAndMakeVisible (closeBtn);

        setSize (640, 540);

        logLine ("Target: " + deviceLabel());
        logLine ("Current firmware: " + (block != nullptr && block->versionNumber.isNotEmpty() ? block->versionNumber : juce::String ("unknown")));
        logLine ("Battery: " + batteryText()
                 + (batteryOkToFlash() ? "" : juce::String ("  - LOW. ROLI needs a higher charge (or a charger) to flash safely.")));
        if (cluster > 1)
            logLine ("Note: " + juce::String (cluster) + " blocks are chained - this flashes ONLY the selected block; the others are left untouched.");
        logLine ("");
        logLine (product.isNotEmpty()
                     ? "Pick a source: download the latest signed image from ROLI (uses your signed-in ROLI Connect), or choose a local .syx / .syx.zip."
                     : "This device has no known ROLI download. Choose a local .syx / .syx.zip to flash.");
    }

    ~FlashPanel() override
    {
        // Detach UI callbacks but DO NOT stop a flash in progress: a half-written image bricks the
        // device, so the engine (kept alive by its own in-flight callback) runs to completion.
        if (engine != nullptr) { engine->onProgress = nullptr; engine->onDone = nullptr; }
        // Detach (not join) so closing mid-download can't hang the UI on the network timeout; the
        // worker captures a SafePointer and value-copies, so it no-ops safely after we're gone.
        if (worker.joinable()) worker.detach();
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (30));
        r.removeFromTop (8);

        auto bottom = r.removeFromBottom (34);
        flashBtn.setBounds (bottom.removeFromRight (120));
        bottom.removeFromRight (8);
        closeBtn.setBounds (bottom.removeFromRight (90));
        r.removeFromBottom (8);

        progress.setBounds (r.removeFromBottom (18));
        r.removeFromBottom (8);
        overrideToggle.setBounds (r.removeFromBottom (24));
        r.removeFromBottom (8);

        auto src = r.removeFromTop (32);
        downloadBtn.setBounds (src.removeFromLeft (220));
        src.removeFromLeft (8);
        chooseBtn.setBounds (src.removeFromLeft (140));
        r.removeFromTop (8);

        log.setBounds (r);
    }

private:
    //== logging ===============================================================
    void logLine (const juce::String& s) { log.moveCaretToEnd(); log.insertTextAtCaret (s + "\n"); }

    juce::String deviceLabel() const
    {
        if (block == nullptr) return "(none)";
        juce::String s = block->serialNumber;
        return s.isNotEmpty() ? s : juce::String ("the selected block");
    }

    //== battery precondition ==================================================
    // ROLI Dashboard refuses to flash a low battery ("Battery too low to update") - a flash that dies
    // partway bricks the device. We mirror that check client-side. Threshold is an estimate (Dashboard's
    // exact cut-off isn't published); charging counts as safe since the device is then externally powered.
    static constexpr float kMinBatteryToFlash = 0.5f;

    float batteryLevel() const { return block != nullptr ? block->getBatteryLevel() : 0.0f; }
    bool  charging()     const { return block != nullptr && block->isBatteryCharging(); }
    bool  batteryOkToFlash() const { return charging() || batteryLevel() >= kMinBatteryToFlash; }
    juce::String batteryText() const
    {
        return juce::String (juce::roundToInt (batteryLevel() * 100.0f)) + "%" + (charging() ? " (charging)" : "");
    }

    //== source: local file ====================================================
    void chooseLocal()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a firmware file",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
                                                       "*.syx;*.zip");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                setBusy (true, "Reading " + f.getFileName() + "...");
                juce::Component::SafePointer<FlashPanel> safe (this);
                startWorker ([safe, f]
                {
                    auto img = grab::loadFirmwareFile (f);
                    juce::MessageManager::callAsync ([safe, img] { if (safe) safe->onImageLoaded (img); });
                });
            });
    }

    //== source: download ======================================================
    void startDownload()
    {
        const auto candidates = RoliArchivePanel::connectTokenCandidates();
        if (candidates.isEmpty())
        {
            logLine ("No signed-in ROLI Connect found. Open ROLI Connect, sign in, then try again (or choose a local file).");
            return;
        }

        const auto url = "https://my.roli.com/firmware_versions/update_to/"
                       + juce::URL::addEscapeChars (product, false) + "/1.0.0";
        const auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile (product.replaceCharacter (' ', '_') + "-firmware.syx.zip");

        setBusy (true, "Downloading " + product + " firmware from ROLI...");
        juce::Component::SafePointer<FlashPanel> safe (this);
        startWorker ([safe, url, dest, candidates]
        {
            bool ok = false;
            for (const auto& tok : candidates)
                if (RoliArchivePanel::fetchToFile (url, dest, tok)) { ok = true; break; }

            grab::FirmwareImage img;
            if (! ok) img.error = "Download failed - open ROLI Connect, sign in, and try again.";
            else      img = grab::loadFirmwareFile (dest);

            juce::MessageManager::callAsync ([safe, img] { if (safe) safe->onImageLoaded (img); });
        });
    }

    //== image loaded ==========================================================
    void onImageLoaded (grab::FirmwareImage img)
    {
        setBusy (false, {});
        image = std::make_unique<grab::FirmwareImage> (std::move (img));

        if (! image->ok())
        {
            logLine ("ERROR: " + image->error);
            image.reset();
            refreshFlashEnabled();
            return;
        }

        logLine ("");
        logLine ("Loaded: " + image->sourceName);
        logLine ("  image id : " + (image->imageId.isNotEmpty() ? image->imageId : juce::String ("(none in manifest)")));
        logLine ("  version  : " + (image->version.isNotEmpty() ? image->version : juce::String ("(none)")));
        logLine ("  packets  : " + juce::String (image->packetCount()) + "   (" + juce::String (image->totalBytes) + " bytes)");
        logLine ("  first    : " + image->messageHex (0));
        logLine ("  last     : " + image->messageHex (image->packetCount() - 1));

        // Match-before-flash.
        expected = block != nullptr ? grab::expectedImageFor (block->getType()) : juce::String();
        matchOk = false;
        if (expected.isEmpty())
            logLine ("  match    : no known image mapping for this device - cannot verify. Flash only if you are certain.");
        else if (image->imageId.isEmpty())
            logLine ("  match    : image has no manifest id - cannot verify against this device.");
        else if (! image->imageId.equalsIgnoreCase (expected))
            logLine ("  match    : MISMATCH - image is '" + image->imageId + "' but this device expects '" + expected + "'.");
        else
        {
            matchOk = true;
            logLine ("  match    : OK - '" + image->imageId + "' matches this device.");
        }

        if (! batteryOkToFlash())
            logLine ("  battery  : " + batteryText() + " - LOW; charge or connect a charger before flashing (ROLI refuses a low battery).");

        overrideToggle.setVisible (! matchOk);
        overrideToggle.setToggleState (false, juce::dontSendNotification);
        refreshFlashEnabled();
    }

    void refreshFlashEnabled()
    {
        const bool haveImage = image != nullptr && image->ok();
        const bool allowed = haveImage && (matchOk || overrideToggle.getToggleState());
        flashBtn.setEnabled (allowed && engine == nullptr);
    }

    //== confirm + flash =======================================================
    void confirmAndFlash()
    {
        if (image == nullptr || ! image->ok() || block == nullptr) return;

        juce::String msg;
        if (! batteryOkToFlash())
            msg << "Battery is low (" << batteryText() << "). ROLI's own updater refuses to flash a low "
                   "battery because it can fail partway and brick the device. Charging above "
                << juce::String (juce::roundToInt (kMinBatteryToFlash * 100.0f))
                << "% (or connecting a charger) is strongly advised.\n\n";

        msg << "About to write " << juce::String (image->packetCount()) << " firmware packets to "
            << deviceLabel() << ".\n\n";
        if (! matchOk)
            msg << "This image does NOT match the connected device.\n\n";
        msg << "Do not disconnect or power off the device until it reboots. Interrupting a flash can "
               "brick the device. Continue?";

        auto opts = juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Flash firmware")
            .withMessage (msg)
            .withButton ("Flash")     // button 1 (Enter)
            .withButton ("Cancel");   // button 2 (Escape -> 0)

        juce::Component::SafePointer<FlashPanel> safe (this);
        juce::AlertWindow::showAsync (opts, [safe] (int result)
        {
            if (safe && result == 1) safe->beginFlash();
        });
    }

    void beginFlash()
    {
        if (image == nullptr || block == nullptr || engine != nullptr) return;

        logLine ("");
        logLine ("Flashing " + juce::String (image->packetCount()) + " packets (ACK-gated). Do NOT disconnect the device...");

        downloadBtn.setEnabled (false);
        chooseBtn.setEnabled (false);
        overrideToggle.setEnabled (false);
        flashBtn.setEnabled (false);
        closeBtn.setEnabled (false);          // re-enabled on completion
        progressValue = 0.0;
        progress.setVisible (true);

        engine = std::make_shared<BlocksFlashEngine> (block, image->messages);
        juce::Component::SafePointer<FlashPanel> safe (this);

        engine->onLog = [safe] (juce::String s) { if (safe) safe->logLine ("  " + s); };
        engine->onProgress = [safe] (int done, int total)
        {
            if (! safe) return;
            safe->progressValue = total > 0 ? (double) done / (double) total : 0.0;
            if (done == 1 || done == total || (done % 200) == 0)
                safe->logLine ("  ... " + juce::String (done) + "/" + juce::String (total) + " acknowledged");
        };
        engine->onDone = [safe] (bool ok, juce::String message)
        {
            if (! safe) return;
            safe->progressValue = ok ? 1.0 : safe->progressValue;
            safe->logLine ("");
            safe->logLine ((ok ? "DONE: " : "FAILED: ") + message);
            if (ok)
                safe->logLine ("When it comes back, reconnect in GRAB to confirm the new firmware version.");
            safe->closeBtn.setEnabled (true);
            // Leave the engine in place (finished) so a second flash needs a fresh image load.
        };

        engine->start();
    }

    //== busy / worker helpers =================================================
    void setBusy (bool busy, const juce::String& note)
    {
        downloadBtn.setEnabled (! busy && product.isNotEmpty());
        chooseBtn.setEnabled (! busy);
        if (busy && note.isNotEmpty()) logLine (note);
        if (busy) { flashBtn.setEnabled (false); }
        else refreshFlashEnabled();
    }

    void startWorker (std::function<void()> fn)
    {
        if (worker.joinable()) worker.join();
        worker = std::thread (std::move (fn));
    }

    void closeSelf()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            juce::MessageManager::callAsync ([dw] { delete dw; });
    }

    //== state =================================================================
    roli::Block::Ptr block;
    juce::String product;
    int cluster = 1;

    std::unique_ptr<grab::FirmwareImage> image;
    juce::String expected;
    bool matchOk = false;

    std::shared_ptr<BlocksFlashEngine> engine;
    double progressValue = 0.0;

    juce::Label title;
    juce::TextEditor log;
    juce::TextButton downloadBtn, chooseBtn, flashBtn, closeBtn;
    juce::ToggleButton overrideToggle;
    juce::ProgressBar progress { progressValue };

    std::unique_ptr<juce::FileChooser> chooser;
    std::thread worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlashPanel)
};
