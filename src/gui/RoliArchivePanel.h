// RoliArchivePanel — "Archive my ROLI downloads" (Advanced ▸). Preservation companion to the
// firmware archiver: while ROLI's servers are up, pull the installers the signed-in account is
// entitled to (apps) plus the connected device's firmware, into a folder the user chooses.
//
// Auth, by the user's choice (issue: archive retrieval):
//   • Reuse Connect's session (default) — GRAB reads the token + product list ROLI Connect already
//     cached on disk (…/ROLI Connect/API Proxy Cache/…), so it never sees a password. Needs Connect
//     installed and signed in.
//   • Browser hand-off — GRAB opens the same ROLI download URLs; the user's logged-in browser
//     authenticates and downloads. GRAB holds no credential.
//
// This only DOWNLOADS (never flashes firmware, never uploads). Everything it fetches is what the
// running account already owns. It can't recreate ROLI Connect's content pipeline, so the big
// Equator 2 / soundpack .rpkg libraries are out of scope (get those via Studio Player); this covers
// the app installers + device firmware.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <thread>
#include <atomic>

#include "Theme.h"

class RoliArchivePanel  : public juce::Component
{
public:
    /** firmwareProduct: ROLI product name of the connected device for firmware (empty if none/unmapped);
        firmwareUrlBase: the my.roli.com update_to base GRAB already uses. */
    explicit RoliArchivePanel (juce::String firmwareProduct = {})
    {
        addAndMakeVisible (title);
        title.setText ("Archive my ROLI downloads", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (19.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (info);
        info.setJustificationType (juce::Justification::topLeft);
        info.setMinimumHorizontalScale (1.0f);

        addAndMakeVisible (listView);
        listView.setViewedComponent (&listBox, false);

        addAndMakeVisible (archiveBtn);
        archiveBtn.setButtonText ("Archive selected to folder...");
        archiveBtn.onClick = [this] { chooseFolderAndArchive(); };

        addAndMakeVisible (browserBtn);
        browserBtn.setButtonText ("Open selected in browser");
        browserBtn.setTooltip ("Opens each selected download in your web browser; your ROLI login there authenticates it.");
        browserBtn.onClick = [this] { openSelectedInBrowser(); };

        addAndMakeVisible (status);
        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, grab::theme::textPrimary.withAlpha (0.8f));

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this]
        {
            cancel = true;
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        buildItems (firmwareProduct);
        layoutList();
        updateInfo();
        fetchProductsAndSounds();   // async: resolve a working token (cache or leveldb), then apps + soundpacks
        setSize (640, 560);
    }

    ~RoliArchivePanel() override { cancel = true; if (worker.joinable()) worker.join(); }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (28));
        r.removeFromTop (4);
        info.setBounds (r.removeFromTop (92));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (34);
        closeBtn.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        browserBtn.setBounds (bottom.removeFromRight (170));
        bottom.removeFromRight (8);
        archiveBtn.setBounds (bottom.removeFromRight (210));
        r.removeFromBottom (6);
        status.setBounds (r.removeFromBottom (24));
        r.removeFromBottom (6);
        listView.setBounds (r);
        layoutList();
    }

private:
    struct Item
    {
        juce::String name, url, filename;
        bool isFirmware = false;
        bool ymlTwoStep = false;     // ROLI Connect: fetch latest.yml, then the named installer
        juce::String ymlFeedBase;    // api.roli.com/.../auto-update/<platform>/
        juce::File   localSource;    // if set, copy this already-downloaded file instead of fetching
        std::unique_ptr<juce::ToggleButton> toggle;
        juce::Label note;
    };

    //==============================================================================
    // Per-user app-data root. JUCE's userApplicationDataDirectory returns %APPDATA% on Windows and
    // ~/.config on Linux (both correct), but just ~/Library on macOS - NOT ~/Library/Application Support -
    // so on mac we append "Application Support" ourselves.
    static juce::File appSupportRoot()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        d = d.getChildFile ("Application Support");
       #endif
        return d;
    }

    // ROLI Connect's Electron userData dir (holds Local Storage/leveldb, Cookies, Cache):
    //   <appSupport>/ROLI Connect
    static juce::File connectElectronDir() { return appSupportRoot().getChildFile ("ROLI Connect"); }

    // Locate ROLI Connect's on-disk API proxy cache:
    //   Windows: %APPDATA%\ROLI\ROLI Connect\API Proxy Cache
    //   macOS:   ~/Library/Application Support/ROLI/ROLI Connect/API Proxy Cache  (verified on a Mac Studio)
    // Some (newer/fresh) Connect installs don't write this at all - then the token comes from leveldb
    // (collectLocalStorageJwts) instead.
    static juce::File connectCacheRoot()
    {
        const auto appData = appSupportRoot();
        const juce::File candidates[] = {
            appData.getChildFile ("ROLI").getChildFile ("ROLI Connect").getChildFile ("API Proxy Cache"),
            appData.getChildFile ("ROLI Connect").getChildFile ("API Proxy Cache")
        };
        for (const auto& c : candidates)
            if (c.isDirectory())
                return c;
        return candidates[0];   // observed layout, so callers still have a sensible default
    }

    // Cached endpoint subfolder names (URL-encoded) under the account dir.
    static constexpr const char* kProductsDir = "%2Fapi%2Fv1%2Fuser%2Fproducts%2F";
    static constexpr const char* kUserDir     = "%2Fapi%2Fv1%2Fuser%2F";

    // The account subfolder holds the cached /user, /user/entitlements and (sometimes) /user/products
    // responses. Connect only caches /user/products transiently - it's usually absent - so we key off
    // /user (present whenever Connect is signed in) and fetch products live instead of needing it on disk.
    static juce::File findAccountDir()
    {
        const auto root = connectCacheRoot();
        if (! root.isDirectory()) return {};
        juce::File first;
        for (const auto& acc : root.findChildFiles (juce::File::findDirectories, false))
        {
            if (first == juce::File()) first = acc;
            if (acc.getChildFile (kUserDir).isDirectory() || acc.getChildFile (kProductsDir).isDirectory())
                return acc;
        }
        return first;   // some account dir, even if we didn't recognise its endpoints
    }

    static juce::File findProductsDir()
    {
        auto pd = findAccountDir().getChildFile (kProductsDir);
        return (pd.isDirectory() && pd.getChildFile ("current").existsAsFile()) ? pd : juce::File();
    }

    static juce::var readCurrentBody (const juce::File& dir)
    {
        if (! dir.isDirectory()) return {};
        const auto cur = dir.getChildFile ("current").loadFileAsString().trim();
        const auto body = dir.getChildFile (cur + ".body");
        return body.existsAsFile() ? juce::JSON::parse (body.loadFileAsString()) : juce::var();
    }

    // The signed-in account's bearer token, read from ANY cached endpoint's response headers
    // (/user is cached whenever Connect is signed in; /user/products often isn't - keying off products
    // alone was why mac/newly-signed-in users got "no token" even though Connect was logged in).
    static juce::String readToken (const juce::File& accountDir)
    {
        if (! accountDir.isDirectory()) return {};
        for (const auto& endpoint : accountDir.findChildFiles (juce::File::findDirectories, false))
            for (const auto& h : endpoint.findChildFiles (juce::File::findFiles, false, "*.headers"))
                for (auto& line : juce::StringArray::fromLines (h.loadFileAsString()))
                    if (line.startsWithIgnoreCase ("x-auth-token:"))
                        return line.fromFirstOccurrenceOf (":", false, false).trim();
        return {};
    }

    // Fallback token source for Connect installs that write no API Proxy Cache (e.g. fresh installs):
    // the session JWT is stored in readable form in Connect's Local Storage (Chromium leveldb). The real
    // auth token's header segment begins "eyJhbGci" (verified against a working cache), so collect every
    // full 3-part JWT with that header. Several may appear (the API caller tries each until one is accepted).
    static juce::StringArray collectLocalStorageJwts()
    {
        juce::StringArray out;
        auto ldb = connectElectronDir().getChildFile ("Local Storage").getChildFile ("leveldb");
        if (! ldb.isDirectory()) return out;

        const auto isTok = [] (char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '-' || c == '_' || c == '.';
        };
        for (const auto& f : ldb.findChildFiles (juce::File::findFiles, false, "*.ldb;*.log"))
        {
            juce::MemoryBlock mb;
            if (! f.loadFileAsData (mb)) continue;
            const auto* p = static_cast<const char*> (mb.getData());
            const int n = (int) mb.getSize();
            for (int i = 0; i + 3 <= n; )
            {
                if (p[i] == 'e' && p[i + 1] == 'y' && p[i + 2] == 'J')
                {
                    int j = i, dots = 0;
                    while (j < n && isTok (p[j])) { if (p[j] == '.') ++dots; ++j; }
                    if (dots >= 2 && j - i > 40)   // a full header.payload.signature JWT
                    {
                        juce::String jwt (p + i, (size_t) (j - i));
                        while (jwt.endsWithChar ('.')) jwt = jwt.dropLastCharacters (1);
                        if (jwt.startsWith ("eyJhbGci"))   // header-first (the auth token), not a bare payload
                            out.addIfNotAlreadyThere (jwt);
                    }
                    i = j + 1;
                }
                else ++i;
            }
        }
        return out;
    }

    static juce::String platformTag()
    {
       #if JUCE_WINDOWS
        return "windows64";   // GRAB ships 64-bit
       #elif JUCE_MAC
        return "osx";
       #else
        return "linux";
       #endif
    }

    // The key ROLI uses for this OS's build inside a product's "builds" object can vary (mac has been
    // seen as osx/macos/darwin), so try the candidates and use whichever the product actually carries;
    // the same key is the build_redirect platform segment.
    static juce::StringArray platformBuildKeys()
    {
       #if JUCE_WINDOWS
        return { "windows64", "windows32", "win64", "win" };
       #elif JUCE_MAC
        return { "osx", "macos", "darwin", "osx64", "mac" };
       #else
        return { "linux", "linux64" };
       #endif
    }

    // ROLI Connect's electron-updater feed names the manifest per platform (confirmed against the live
    // API: osx/latest.yml is rejected as "Incompatible platform and channel file requested"; it must be
    // latest-mac.yml). The feed serves the manifest's `path:` artifact (Win: the .exe installer; mac: the
    // app .zip - the .dmg isn't exposed through the feed, "Build file not found").
    static juce::String updaterYmlName()
    {
       #if JUCE_MAC
        return "latest-mac.yml";
       #elif JUCE_WINDOWS
        return "latest.yml";
       #else
        return "latest-linux.yml";
       #endif
    }

    //==============================================================================
    void buildItems (const juce::String& firmwareProduct)
    {
        const auto accountDir = findAccountDir();
        token = readToken (accountDir);
        haveConnectCache = accountDir.isDirectory();

        // Apps: parse the on-disk /user/products cache if Connect happens to have left one (fast first
        // paint). It's usually absent (Connect caches it only transiently), so fetchProducts() fetches
        // it live from the API - that's the real source, and works wherever Connect is just signed in.
        addProductsFromJson (readCurrentBody (accountDir.getChildFile (kProductsDir)));

        const auto plat = platformTag();

        // ROLI Connect itself — distributed via its electron-updater feed (com.roli.hub), not /products.
        // Two-step: read latest.yml for the installer name, then fetch it (the feed re-signs per file).
        if (plat == "windows64" || plat == "windows32" || plat == "osx")
        {
            auto it = std::make_unique<Item>();
            it->name = "ROLI Connect  (latest installer)";
            it->ymlTwoStep = true;
            it->ymlFeedBase = "https://api.roli.com/api/v1/product/com.roli.hub/auto-update/" + plat + "/";
            it->filename = "ROLI Connect Setup.exe";   // refined from the yml at download time
            items.push_back (std::move (it));
        }

        // Packages ROLI Connect has already downloaded (Studio Player / Drums .rpkg, bundled synths,
        // Equator 2 content, installers). GRAB can't drive Connect's private content pipeline, but it
        // can preserve what's already been fetched - copy them into the archive.
        {
            auto dl = connectCacheRoot().getParentDirectory().getChildFile ("downloads");  // …\ROLI Connect\downloads
            juce::StringArray seen;
            for (const auto& f : dl.findChildFiles (juce::File::findFiles, true, "*.rpkg;*.exe;*.msi;*.dmg;*.pkg"))
            {
                const auto nm = f.getFileName();
                if (nm.startsWithChar ('.') || f.getFullPathName().containsIgnoreCase ("__MACOSX")) continue; // in-progress / junk
                if (f.getSize() < 5 * 1024 * 1024 || seen.contains (nm)) continue;                            // stub / dupe
                seen.add (nm);

                // Skip cached app installers the direct-download items above already cover (Dashboard /
                // Equator / Connect) - this drops ROLI's own duplicates, incl. their misspelled
                // "Dashbaoard..." copy. Keep .rpkg content and the Strobe2/Cypher2 synths (not direct-downloadable).
                const auto low = nm.toLowerCase();
                const bool installer = low.endsWith (".exe") || low.endsWith (".msi")
                                    || low.endsWith (".dmg") || low.endsWith (".pkg");
                if (installer && (low.contains ("dashb") || low.contains ("equatorinstall") || low.contains ("connect"))
                              && ! low.contains ("strobe") && ! low.contains ("cypher"))
                    continue;
                auto it = std::make_unique<Item>();
                it->name = nm + "  (already downloaded via Connect)";
                it->localSource = f;
                it->filename = nm;
                items.push_back (std::move (it));
            }
        }

        // Unify: the connected device's firmware (download-only; .syx.zip), if GRAB has a product name.
        if (firmwareProduct.isNotEmpty())
        {
            auto it = std::make_unique<Item>();
            it->name = firmwareProduct + " firmware  (latest .syx.zip)";
            it->url  = "https://my.roli.com/firmware_versions/update_to/"
                     + juce::URL::addEscapeChars (firmwareProduct, false) + "/1.0.0";
            it->filename = firmwareProduct.replaceCharacter (' ', '_') + "-firmware-latest.syx.zip";
            it->isFirmware = true;
            items.push_back (std::move (it));
        }

        // Make the toggles for any items that don't have one yet (apps/soundpacks added inline
        // already have theirs).
        for (auto& it : items)
            if (it->toggle == nullptr)
                makeToggle (*it);
    }

    // Give an item its toggle + note and show them in the list.
    void makeToggle (Item& it)
    {
        it.toggle = std::make_unique<juce::ToggleButton> (it.name);
        it.toggle->setToggleState (true, juce::dontSendNotification);
        listBox.addAndMakeVisible (*it.toggle);
        it.note.setFont (juce::Font (juce::FontOptions (12.0f)));
        it.note.setColour (juce::Label::textColourId, grab::theme::textPrimary.withAlpha (0.55f));
        listBox.addAndMakeVisible (it.note);
    }

    // Parse a /user/products JSON (from disk cache OR a live fetch) into downloadable app items.
    // De-dupes by product name so the live fetch can safely re-add over a disk-cache first paint.
    void addProductsFromJson (const juce::var& products)
    {
        auto* arr = products.getArray();
        if (arr == nullptr)
            if (auto* o = products.getDynamicObject())   // some shapes wrap it in { products: [...] }
                if (auto p = o->getProperty ("products"); p.isArray()) arr = p.getArray();
        if (arr == nullptr) return;

        const auto keys = platformBuildKeys();
        int added = 0;
        for (auto& pv : *arr)
        {
            if (! (bool) pv.getProperty ("user_owns_product", false)) continue;
            if (! (bool) pv.getProperty ("user_downloadable", false)) continue;
            const auto name = pv.getProperty ("name", {}).toString();
            const auto pid  = pv.getProperty ("id", {}).toString();
            auto builds = pv.getProperty ("builds", {});
            auto* bo = builds.getDynamicObject();
            if (name.isEmpty() || pid.isEmpty() || bo == nullptr) continue;

            // Pick whichever platform key this product actually carries for this OS.
            juce::String plat; juce::var build;
            for (const auto& k : keys)
                if (auto b = bo->getProperty (k); b.isObject()) { plat = k; build = b; break; }
            if (plat.isEmpty()) continue;
            const auto vid = build.getProperty ("_id", {}).toString();
            if (vid.isEmpty()) continue;

            bool dup = false;
            for (auto& ex : items) if (ex->name.startsWith (name + "  (")) { dup = true; break; }
            if (dup) continue;

            // Suggested filename from the build's s3_path if present, else product+platform.
            juce::String fname;
            if (auto inner = build.getProperty ("build", {}); inner.isObject())
                fname = inner.getProperty ("s3_path", {}).toString().fromLastOccurrenceOf ("/", false, false);
            if (fname.isEmpty())
                fname = name.replaceCharacters (" /", "__") + "-" + plat;

            auto it = std::make_unique<Item>();
            it->name = name + "  (" + plat + ")";
            it->url  = "https://api.roli.com/api/legacy/build_redirect/product/" + pid
                     + "/version/" + vid + "/build/" + plat;
            it->filename = fname;
            if (it->toggle == nullptr) makeToggle (*it);   // inline (live path); harmless on the disk path
            items.push_back (std::move (it));
            ++added;
        }
        if (added > 0) { layoutList(); updateInfo(); }
    }

    // Fetch /user/products live (Connect caches it only transiently, so the disk copy is usually
    // missing) and, once a token is confirmed working, the soundpack catalog too. Tries the cache token
    // first, then any JWTs recovered from Connect's Local Storage (for installs with no API Proxy Cache),
    // and keeps whichever the API accepts (200). Background thread so the panel never blocks.
    void fetchProductsAndSounds()
    {
        juce::StringArray candidates;
        if (token.isNotEmpty()) candidates.add (token);
        for (const auto& j : collectLocalStorageJwts()) candidates.addIfNotAlreadyThere (j);
        if (candidates.isEmpty()) return;

        juce::Component::SafePointer<RoliArchivePanel> safe (this);
        std::thread ([safe, candidates]
        {
            for (const auto& tok : candidates)
            {
                juce::WebInputStream web (juce::URL ("https://api.roli.com/api/v1/user/products"), false);
                web.withExtraHeaders ("User-Agent: GRAB\r\nAccept: application/json\r\nx-access-token: " + tok + "\r\nCookie: token=" + tok);
                web.withConnectionTimeout (15000);
                web.withNumRedirectsToFollow (6);
                if (web.connect (nullptr) && web.getStatusCode() == 200)
                {
                    auto body = std::make_shared<juce::var> (juce::JSON::parse (web.readEntireStreamAsString()));
                    juce::MessageManager::callAsync ([safe, body, tok]
                    {
                        if (auto* p = safe.getComponent())
                        {
                            p->token = tok;              // this one works - reuse it for soundpacks + downloads
                            p->addProductsFromJson (*body);
                            p->updateInfo();
                            p->fetchSoundpacks();
                        }
                    });
                    return;
                }
            }
        }).detach();
    }

    void layoutList()
    {
        const int rowH = 26, w = juce::jmax (300, listView.getWidth() - 12);
        int y = 4;
        for (auto& it : items)
        {
            it->toggle->setBounds (6, y, w - 120, rowH);
            it->note.setBounds (w - 120, y, 118, rowH);
            y += rowH;
        }
        listBox.setSize (w, juce::jmax (y + 4, listView.getHeight()));
    }

    void updateInfo()
    {
        if (items.empty())
        {
            info.setText (juce::String ("Nothing to archive was found.\n\n")
                + (haveConnectCache
                   ? "Your ROLI Connect cache was read, but it lists no downloadable installers for this platform."
                   : "GRAB couldn't find a ROLI Connect cache. Install ROLI Connect and sign in (so it stores "
                     "your product list), then reopen this. You can still archive device firmware via the browser."),
                juce::dontSendNotification);
            archiveBtn.setEnabled (false);
            return;
        }
        info.setText (juce::String ("These are downloads the signed-in ROLI account is entitled to. GRAB fetches them for ")
            + "archiving while ROLI's servers are up - it never flashes firmware or uploads anything.\n\n"
            + (token.isNotEmpty()
               ? "Reusing your ROLI Connect session (no password needed). If a download fails with an auth error, "
                 "open Connect to refresh your session, or use \"Open selected in browser\"."
               : "No Connect session token was found: app/firmware downloads need \"Open selected in browser\", "
                 "but already-downloaded packages can still be copied to the archive."),
            juce::dontSendNotification);
        archiveBtn.setEnabled (true);   // local copies work tokenless; direct downloads just report "failed" without one
    }

    //==============================================================================
    struct SPInfo { juce::String name, url, filename; juce::File local; };

    // Owned soundpacks aren't in /user/products; Connect's Sounds tab uses this endpoint, and each
    // entry carries a ready-signed download_url straight to the .rpkg (works free *or* paid - ownership
    // is all that matters; no auth needed on the signed URL itself). Confirmed by inspecting Connect's
    // own traffic. Fetched on a background thread so opening the panel never blocks.
    void fetchSoundpacks()
    {
        if (token.isEmpty()) return;   // need the Connect session to list what the account owns
        setStatusText ("Checking your soundpacks...");
        const auto tok = token;
        const auto dlDir = connectCacheRoot().getParentDirectory().getChildFile ("downloads");
        juce::Component::SafePointer<RoliArchivePanel> safe (this);
        std::thread ([safe, tok, dlDir]
        {
            juce::WebInputStream web (juce::URL ("https://api.roli.com/api/v1/user/products/soundpacks?variant=rpkg"), false);
            web.withExtraHeaders ("User-Agent: GRAB\r\nAccept: application/json\r\nx-access-token: " + tok + "\r\nCookie: token=" + tok);
            web.withConnectionTimeout (15000);
            web.withNumRedirectsToFollow (6);
            auto list = std::make_shared<std::vector<SPInfo>>();
            if (web.connect (nullptr) && web.getStatusCode() == 200)
            {
                const auto json = juce::JSON::parse (web.readEntireStreamAsString());
                if (auto* arr = json.getArray())
                    for (auto& v : *arr)
                    {
                        if (! (bool) v.getProperty ("rpkg_available", false)) continue;
                        const auto nm  = v.getProperty ("name", {}).toString();
                        const auto url = v.getProperty ("download_url", {}).toString();
                        if (nm.isEmpty() || url.isEmpty()) continue;
                        auto fn = url.upToFirstOccurrenceOf ("?", false, false).fromLastOccurrenceOf ("/", false, false);
                        if (fn.isEmpty() || ! fn.endsWithIgnoreCase (".rpkg")) fn = nm.replaceCharacters (" /", "__") + ".rpkg";
                        SPInfo sp { nm, url, fn, {} };
                        for (auto& f : dlDir.findChildFiles (juce::File::findFiles, true, fn))   // already installed?
                            { sp.local = f; break; }
                        list->push_back (sp);
                    }
            }
            juce::MessageManager::callAsync ([safe, list] { if (auto* p = safe.getComponent()) p->addSoundpacks (*list); });
        }).detach();
    }

    void addSoundpacks (const std::vector<SPInfo>& list)
    {
        int added = 0;
        for (auto& sp : list)
        {
            if (sp.local.existsAsFile()) continue;   // already on disk -> the copy-from-cache scan already lists it
            auto it = std::make_unique<Item>();
            it->name = sp.name + "  (soundpack)";
            it->url = sp.url;
            it->filename = sp.filename;
            it->toggle = std::make_unique<juce::ToggleButton> (it->name);
            it->toggle->setToggleState (true, juce::dontSendNotification);
            listBox.addAndMakeVisible (*it->toggle);
            it->note.setFont (juce::Font (juce::FontOptions (12.0f)));
            it->note.setColour (juce::Label::textColourId, grab::theme::textPrimary.withAlpha (0.55f));
            listBox.addAndMakeVisible (it->note);
            items.push_back (std::move (it));
            ++added;
        }
        layoutList();
        archiveBtn.setEnabled (true);
        if (! list.empty())
            setStatusText ("Added " + juce::String (added) + " soundpack(s) to download"
                           + (added < (int) list.size() ? " (" + juce::String ((int) list.size() - added) + " already on disk)." : "."));
    }

    //==============================================================================
    void chooseFolderAndArchive()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder to archive ROLI downloads into",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (dir.isDirectory()) startArchive (dir);
            });
    }

    void startArchive (juce::File dir)
    {
        if (worker.joinable()) { cancel = true; worker.join(); }
        cancel = false;
        archiveBtn.setEnabled (false);
        browserBtn.setEnabled (false);

        // Firmware lives on my.roli.com, which authenticates with the browser's web-session cookie -
        // NOT the ROLI Connect API token GRAB reads (a direct fetch gets "API Key Required"). So open
        // any selected firmware in the browser (user is signed in to my.roli.com there) instead of
        // trying to download it, and leave it out of the worker's download jobs.
        int firmwareOpened = 0;
        for (auto& it : items)
            if (it->toggle->getToggleState() && it->isFirmware && it->url.isNotEmpty())
            {
                juce::URL (it->url).launchInDefaultBrowser();
                it->note.setText ("opened in browser", juce::dontSendNotification);
                ++firmwareOpened;
            }

        // Collect the remaining selected items on the message thread; the worker resolves each
        // (copy / yml / download).
        struct Job { juce::String name; Item* item; };
        auto jobs = std::make_shared<std::vector<Job>>();
        for (auto& it : items)
            if (it->toggle->getToggleState() && ! it->isFirmware)
                jobs->push_back ({ it->name, it.get() });

        if (jobs->empty() && firmwareOpened > 0)
        {
            setStatusText ("Firmware opened in your browser (my.roli.com) - save the .syx.zip into your "
                           "archive folder. Nothing else was selected to download.");
            archiveBtn.setEnabled (true);
            browserBtn.setEnabled (true);
            return;
        }

        const auto tok = token;
        const auto outDir = dir;
        juce::Component::SafePointer<RoliArchivePanel> safe (this);
        worker = std::thread ([safe, jobs, tok, outDir]
        {
            int ok = 0;
            for (auto& j : *jobs)
            {
                auto* self = safe.getComponent();
                if (self == nullptr || self->cancel) break;
                const bool isCopy = j.item->localSource.existsAsFile();
                setNote (safe, j.item, isCopy ? "copying..." : "downloading...");
                setStatus (safe, juce::String (isCopy ? "Copying " : "Downloading ") + j.name + " ...");

                bool good = fetchItem (j.item, outDir, tok, safe);
                setNote (safe, j.item, good ? "saved" : "failed");
                if (good) ++ok;
            }
            const int total = (int) jobs->size();
            juce::MessageManager::callAsync ([safe, ok, total]
            {
                if (auto* s = safe.getComponent())
                {
                    s->setStatusText ("Done - archived " + juce::String (ok) + " of " + juce::String (total) + ".");
                    s->archiveBtn.setEnabled (true);
                    s->browserBtn.setEnabled (true);
                }
            });
        });
    }

    static juce::String authHeaders (const juce::String& token)
    {
        juce::String h = "User-Agent: GRAB";
        if (token.isNotEmpty()) h += "\r\nx-auth-token: " + token + "\r\nCookie: token=" + token;
        return h;
    }

    // Resolve an item to a saved file: copy a local package, or (ROLI Connect) read latest.yml then
    // fetch the named installer, or a plain signed download. All stream with progress + cancel.
    static bool fetchItem (Item* it, const juce::File& dir, const juce::String& token,
                           juce::Component::SafePointer<RoliArchivePanel> safe)
    {
        if (it->localSource.existsAsFile())
            return streamCopy (it->localSource, dir.getChildFile (it->filename), safe);

        juce::String url = it->url, fname = it->filename;
        if (it->ymlTwoStep)
        {
            juce::WebInputStream yml (juce::URL (it->ymlFeedBase + updaterYmlName() + "?flavor=general-edition"), false);
            yml.withExtraHeaders (authHeaders (token));
            yml.withConnectionTimeout (15000);
            yml.withNumRedirectsToFollow (6);
            if (! yml.connect (nullptr)) return false;
            for (auto& line : juce::StringArray::fromLines (yml.readEntireStreamAsString()))
                if (line.startsWithIgnoreCase ("path:")) { fname = line.fromFirstOccurrenceOf (":", false, false).trim(); break; }
            if (fname.isEmpty()) return false;
            url = it->ymlFeedBase + juce::URL::addEscapeChars (fname, false) + "?flavor=general-edition";
        }
        return httpDownload (url, dir.getChildFile (fname), token, safe);
    }

    // Streamed download with the Connect token; follows ROLI's 302 to the signed CDN URL.
    static bool httpDownload (const juce::String& url, const juce::File& dest, const juce::String& token,
                              juce::Component::SafePointer<RoliArchivePanel> safe)
    {
        juce::WebInputStream web (juce::URL (url), false);
        web.withExtraHeaders (authHeaders (token));
        web.withConnectionTimeout (20000);
        web.withNumRedirectsToFollow (6);
        if (! web.connect (nullptr)) return false;
        const int code = web.getStatusCode();
        if (code != 200 && code != 0) return false;   // 0 = some backends report 0 after a redirect

        dest.deleteFile();
        juce::FileOutputStream out (dest);
        if (! out.openedOk()) return false;
        juce::HeapBlock<char> buf (1 << 16);
        const juce::int64 total = web.getTotalLength();
        juce::int64 done = 0;
        while (! web.isExhausted())
        {
            auto* self = safe.getComponent();
            if (self == nullptr || self->cancel) { out.flush(); dest.deleteFile(); return false; }
            const int n = web.read (buf, 1 << 16);
            if (n <= 0) break;
            out.write (buf, (size_t) n);
            done += n;
            if (total > 0) setStatusPct (safe, (int) (100.0 * (double) done / (double) total));
        }
        out.flush();
        return done > 0;
    }

    // Streamed local file copy (for packages Connect already downloaded), with progress + cancel.
    static bool streamCopy (const juce::File& src, const juce::File& dest,
                            juce::Component::SafePointer<RoliArchivePanel> safe)
    {
        if (src == dest) return true;
        juce::FileInputStream in (src);
        if (! in.openedOk()) return false;
        dest.deleteFile();
        juce::FileOutputStream out (dest);
        if (! out.openedOk()) return false;
        juce::HeapBlock<char> buf (1 << 16);
        const juce::int64 total = in.getTotalLength();
        juce::int64 done = 0;
        while (! in.isExhausted())
        {
            auto* self = safe.getComponent();
            if (self == nullptr || self->cancel) { out.flush(); dest.deleteFile(); return false; }
            const int n = in.read (buf, 1 << 16);
            if (n <= 0) break;
            out.write (buf, (size_t) n);
            done += n;
            if (total > 0) setStatusPct (safe, (int) (100.0 * (double) done / (double) total));
        }
        out.flush();
        return done > 0;
    }

    void openSelectedInBrowser()
    {
        int n = 0;
        for (auto& it : items)
            if (it->toggle->getToggleState() && it->url.isNotEmpty())  // local copies / Connect have no direct browser URL
                { juce::URL (it->url).launchInDefaultBrowser(); it->note.setText ("browser", juce::dontSendNotification); ++n; }
        setStatusText (n > 0 ? "Opened " + juce::String (n) + " download(s) in your browser."
                             : "Those items have no direct browser link (use Archive to copy/fetch them).");
    }

    //==============================================================================
    void setStatusText (const juce::String& s) { status.setText (s, juce::dontSendNotification); }

    static void setStatus (juce::Component::SafePointer<RoliArchivePanel> safe, const juce::String& s)
    { juce::MessageManager::callAsync ([safe, s] { if (auto* p = safe.getComponent()) p->setStatusText (s); }); }

    static void setStatusPct (juce::Component::SafePointer<RoliArchivePanel> safe, int pct)
    { juce::MessageManager::callAsync ([safe, pct] { if (auto* p = safe.getComponent()) p->status.setText (p->status.getText().upToFirstOccurrenceOf (" [", false, false) + " [" + juce::String (pct) + "%]", juce::dontSendNotification); }); }

    static void setNote (juce::Component::SafePointer<RoliArchivePanel> safe, Item* item, const juce::String& s)
    { juce::MessageManager::callAsync ([safe, item, s] { if (safe.getComponent() != nullptr) item->note.setText (s, juce::dontSendNotification); }); }

    //==============================================================================
    juce::Label      title, info, status;
    juce::Viewport   listView;
    juce::Component   listBox;
    juce::TextButton archiveBtn, browserBtn, closeBtn;
    std::vector<std::unique_ptr<Item>> items;
    juce::String     token;
    bool             haveConnectCache = false;
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread      worker;
    std::atomic<bool> cancel { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoliArchivePanel)
};
