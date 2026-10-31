// AppPreview.cpp - the littlefoot VM behind LittleFootApp (see AppPreview.h).
//
// IMPORTANT: `using namespace juce` must be active BEFORE the roli_blocks_basics include, because the
// vendored littlefoot Runner calls some juce functions (e.g. readLittleEndianBitsInBuffer) unqualified.
// The GRAB target sets DONT_SET_USING_JUCE_NAMESPACE=1, so we restore it here, in this TU only.

#include <juce_gui_extra/juce_gui_extra.h>   // declares the juce namespace first
using namespace juce;                         // required by the vendored Runner (unqualified juce calls)

#include <roli_blocks_basics/roli_blocks_basics.h>   // roli::littlefoot::Compiler / Runner / NativeFunction

#include "AppPreview.h"

#include <array>
#include <cstring>
#include <vector>

namespace lf = roli::littlefoot;

//==============================================================================
// Metadata -> Littlefoot code generation.
//
// A BLOCKS app's .littlefoot source references variables and accessor functions that are never
// declared in the file - e.g. Fader Block uses `midiChannel`, `getCc(i)`, `getSlider(i)`,
// `setSlider(i,v)`. ROLI Dashboard SYNTHESISES those from the app's <metadata> block at build time
// (each <variable> -> a global; each <vector wantsGetFunctions/wantsSetFunctions> -> a backing array
// plus get<Name>/set<Name>), seeds them to their value= defaults, and prepends the result before
// compiling. We replicate that here so metadata-driven apps compile and run in the preview.
//
// The metadata lives inside the file's leading /* ... */ comment, so the compiler ignores the raw XML.
namespace metagen
{
    static juce::String extractBlock (const juce::String& src)
    {
        const auto a = src.indexOf ("<metadata");
        if (a < 0) return {};
        const auto b = src.indexOf (a, "</metadata>");
        if (b < 0) return {};
        return src.substring (a, b + (int) juce::String ("</metadata>").length());
    }

    static juce::String lfType (const juce::String& metaType)
    {
        if (metaType == "float") return "float";
        if (metaType == "bool")  return "bool";
        return "int";                       // int, colour, option all map to int
    }

    static juce::String ucfirst (const juce::String& s)
    { return s.isEmpty() ? s : s.substring (0, 1).toUpperCase() + s.substring (1); }

    // A Littlefoot literal for a variable's default value= attribute, given its type and (for
    // "random" colours) its index within a vector, so each element gets a distinct, stable colour.
    static juce::String literalFor (const juce::String& type, const juce::String& value, int idx, int count)
    {
        if (type == "float")
        {
            if (value.isEmpty()) return "0.0";
            auto s = value;
            if (! s.containsChar ('.')) s << ".0";
            return s;
        }
        if (type == "bool")
            return (value == "true" || value == "1") ? "true" : "false";

        if (type == "colour")
        {
            if (value.equalsIgnoreCase ("random"))
            {
                const float hue = count > 0 ? (float) idx / (float) count : 0.0f;
                return "0x" + juce::String::toHexString ((int) juce::Colour::fromHSV (hue, 0.85f, 1.0f, 1.0f).getARGB());
            }
            if (value.startsWithIgnoreCase ("0x")) return value;
            if (value.isNotEmpty() && value.containsOnly ("0123456789"))
                return "0x" + juce::String::toHexString ((int) (juce::uint32) value.getLargeIntValue());
            return "0xffffffff";             // sane visible default for an unspecified colour
        }

        // int / option
        return value.containsOnly ("-0123456789") && value.isNotEmpty() ? value : "0";
    }

    // Build the preamble; sets hasSeed if a __grabSeed() was emitted (host calls it before initialise()).
    static juce::String buildPreamble (const juce::String& src, bool& hasSeed)
    {
        hasSeed = false;
        const auto block = extractBlock (src);
        if (block.isEmpty()) return {};

        auto root = juce::XmlDocument::parse (block);
        auto* vars = root != nullptr ? root->getChildByName ("variables") : nullptr;
        if (vars == nullptr) return {};

        juce::StringArray decls, funcs, seeds;

        for (auto* child : vars->getChildIterator())
        {
            if (child->hasTagName ("variable"))
            {
                const auto name = child->getStringAttribute ("name");
                if (name.isEmpty()) continue;
                const auto type = child->getStringAttribute ("type", "int");
                decls.add (lfType (type) + " " + name + ";");
                seeds.add ("    " + name + " = " + literalFor (type, child->getStringAttribute ("value"), 0, 1) + ";");
            }
            else if (child->hasTagName ("vector"))
            {
                const int count = juce::jlimit (1, 256, child->getStringAttribute ("count", "1").getIntValue());
                const bool wantGet = child->getStringAttribute ("wantsGetFunctions") == "true";
                const bool wantSet = child->getStringAttribute ("wantsSetFunctions") == "true";

                for (auto* v : child->getChildIterator())
                {
                    if (! v->hasTagName ("variable")) continue;
                    const auto name = v->getStringAttribute ("name");
                    if (name.isEmpty()) continue;
                    const auto type  = v->getStringAttribute ("type", "int");
                    const auto lft   = lfType (type);

                    decls.add (lft + " " + name + "[" + juce::String (count) + "];");
                    if (wantGet) funcs.add (lft + " get" + ucfirst (name) + " (int index) { return " + name + "[index]; }");
                    if (wantSet) funcs.add ("void set" + ucfirst (name) + " (int index, " + lft + " value) { " + name + "[index] = value; }");

                    const auto value = v->getStringAttribute ("value");
                    for (int i = 0; i < count; ++i)
                        seeds.add ("    " + name + "[" + juce::String (i) + "] = " + literalFor (type, value, i, count) + ";");
                }
            }
        }

        if (decls.isEmpty()) return {};

        juce::String out;
        out << "// ---- generated from <metadata> (GRAB preview; mirrors ROLI Dashboard's build step) ----\n";
        out << decls.joinIntoString ("\n") << "\n\n";
        if (! funcs.isEmpty()) out << funcs.joinIntoString ("\n") << "\n\n";
        out << "void __grabSeed()\n{\n" << seeds.joinIntoString ("\n") << "\n}\n";
        out << "// ---- end generated ----\n\n";
        hasSeed = true;
        return out;
    }
}

struct LittleFootApp::Impl
{
    static constexpr int W = LittleFootApp::W, H = LittleFootApp::H;

    using RunnerT = lf::Runner<7200, 4096>;      // program+heap, stack+globals (Lightpad sizes)
    RunnerT runner;
    std::array<juce::uint32, (size_t) (W * H)> grid {};
    std::array<juce::int32, 256> config {};
    juce::Random rng;
    juce::uint32 startMs = 0;
    bool loaded = false;
    std::vector<lf::NativeFunction> natives;

    Impl()
    {
        buildNatives();
        runner.setNativeFunctions (natives.data(), (int) natives.size(), this);
        grid.fill (0xff000000);
    }

    juce::Result load (const juce::String& source, const juce::Array<juce::File>& searchPaths)
    {
        bool hasSeed = false;
        const auto preamble = metagen::buildPreamble (source, hasSeed);

        lf::Compiler compiler;
        compiler.addNativeFunctions (runner);
        auto res = compiler.compile (preamble + source, 2048, searchPaths);
        if (res.failed())
            return res;

        const auto& code = compiler.compiledObjectCode;
        if ((int) code.size() > (int) runner.getMaximumProgramSize())
            return juce::Result::fail ("Program too large for the preview VM");

        runner.reset();
        std::memcpy (runner.getProgramAndDataStart(), code.begin(), (size_t) code.size());
        loaded = true;
        startMs = juce::Time::getMillisecondCounter();

        seedDefaultConfig();                        // so config-driven apps (e.g. Note Grid) aren't all-black
        if (hasSeed) callFn ("__grabSeed/v");       // seed the generated metadata globals before initialise()
        callFn ("initialise/v");
        return juce::Result::ok();
    }

    // Standard-config defaults an attached device would already hold. Without these a config-driven app
    // reads 0 for everything - e.g. Note Grid's gridSize (ConfigId_mode) is 0, so it draws no pads at all.
    void seedDefaultConfig()
    {
        config.fill (0);
        config[4]  = 0;      // ConfigId_octave
        config[5]  = 0;      // ConfigId_transpose
        config[20] = 5;      // ConfigId_mode        -> Note Grid gridSize = 5x5
        config[22] = 0;      // ConfigId_scale       -> Major
        config[27] = 0;      // ConfigId_rootKey     -> C
        config[33] = 1;      // ConfigId_gammaCorrection
        config[34] = (i32) 0xff2b6fff;   // ConfigId_globalKeyColour (blue; used by some apps)
        config[35] = (i32) 0xffffffff;   // ConfigId_rootKeyColour   (white)
        config[36] = 100;    // ConfigId_brightness
    }

    void repaint() { if (loaded) callFn ("repaint/v"); }

    juce::uint32 pixel (int x, int y) const
    { return (x >= 0 && x < W && y >= 0 && y < H) ? grid[(size_t) (y * W + x)] : 0xff000000; }

private:
    using i32 = juce::int32;
    static float asF (i32 v) { float f; std::memcpy (&f, &v, 4); return f; }
    static i32   toI (float f) { i32 v; std::memcpy (&v, &f, 4); return v; }

    // The vendored callFunction() overloads can't instantiate (they call run() with no timeout), so run
    // via FunctionExecutionContext with a wall-clock budget - also guards the UI against a runaway app.
    void callFn (const char* sig)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + 50;
        RunnerT::FunctionExecutionContext (runner, sig).run ([deadline] { return juce::Time::getMillisecondCounter() > deadline; });
    }

    static juce::uint32 blend (juce::uint32 base, juce::uint32 over)
    {
        const juce::uint32 a = (over >> 24) & 0xff;
        if (a == 0)   return base;
        if (a == 255) return 0xff000000 | (over & 0xffffff);
        auto mix = [a] (juce::uint32 b, juce::uint32 o) { return (((b * (255 - a) + o * a) / 255) & 0xff); };
        return 0xff000000
             | (mix ((base >> 16) & 0xff, (over >> 16) & 0xff) << 16)
             | (mix ((base >> 8)  & 0xff, (over >> 8)  & 0xff) << 8)
             |  mix ( base        & 0xff,  over        & 0xff);
    }

    void setPix   (int x, int y, juce::uint32 c) { if (x >= 0 && x < W && y >= 0 && y < H) grid[(size_t) (y * W + x)] = 0xff000000 | (c & 0xffffff); }
    void blendPix (int x, int y, juce::uint32 c) { if (x >= 0 && x < W && y >= 0 && y < H) { auto& g = grid[(size_t) (y * W + x)]; g = blend (g, c); } }
    void fillRect (juce::uint32 c, int x, int y, int w, int h, bool b)
    { for (int j = y; j < y + h; ++j) for (int k = x; k < x + w; ++k) b ? blendPix (k, j, c) : setPix (k, j, c); }
    void clearAll (juce::uint32 c) { for (auto& g : grid) g = 0xff000000 | (c & 0xffffff); }

    static Impl* self (void* c) { return static_cast<Impl*> (c); }

    void buildNatives()
    {
        auto add = [this] (const char* sig, lf::NativeFunction::ImplementationFunction fn)
        { natives.push_back (lf::NativeFunction (sig, fn)); };

        add ("min/iii",   +[] (void*, const i32* a) -> i32 { return juce::jmin (a[0], a[1]); });
        add ("min/fff",   +[] (void*, const i32* a) -> i32 { return toI (juce::jmin (asF (a[0]), asF (a[1]))); });
        add ("max/iii",   +[] (void*, const i32* a) -> i32 { return juce::jmax (a[0], a[1]); });
        add ("max/fff",   +[] (void*, const i32* a) -> i32 { return toI (juce::jmax (asF (a[0]), asF (a[1]))); });
        add ("clamp/iiii",+[] (void*, const i32* a) -> i32 { return juce::jlimit (a[0], a[1], a[2]); });
        add ("clamp/ffff",+[] (void*, const i32* a) -> i32 { return toI (juce::jlimit (asF (a[0]), asF (a[1]), asF (a[2]))); });
        add ("abs/ii",    +[] (void*, const i32* a) -> i32 { return std::abs (a[0]); });
        add ("abs/ff",    +[] (void*, const i32* a) -> i32 { return toI (std::abs (asF (a[0]))); });
        add ("mod/iii",   +[] (void*, const i32* a) -> i32 { return a[1] != 0 ? a[0] % a[1] : 0; });
        add ("map/ffffff",+[] (void*, const i32* a) -> i32 { const float v=asF(a[0]),lo=asF(a[1]),hi=asF(a[2]),ol=asF(a[3]),oh=asF(a[4]); return toI (hi==lo?ol:ol+(v-lo)*(oh-ol)/(hi-lo)); });
        add ("map/ffff",  +[] (void*, const i32* a) -> i32 { const float v=asF(a[0]),hi=asF(a[1]),oh=asF(a[2]); return toI (hi==0?0:v*oh/hi); });
        add ("getRandomFloat/f", +[] (void* c, const i32*) -> i32 { return toI ((float) self(c)->rng.nextDouble()); });
        add ("getRandomInt/ii",  +[] (void* c, const i32* a) -> i32 { return a[0] > 0 ? self(c)->rng.nextInt (a[0]) : 0; });

        add ("getMillisecondCounter/i",       +[] (void* c, const i32*) -> i32 { return (i32) (juce::Time::getMillisecondCounter() - self(c)->startMs); });
        add ("getTimeInCurrentFunctionCall/i",+[] (void*, const i32*) -> i32 { return 0; });
        add ("getFirmwareVersion/i",          +[] (void*, const i32*) -> i32 { return 0; });

        add ("makeARGB/iiiii", +[] (void*, const i32* a) -> i32
        { return (i32) (((juce::uint32) (a[0]&0xff)<<24)|((juce::uint32)(a[1]&0xff)<<16)|((juce::uint32)(a[2]&0xff)<<8)|(juce::uint32)(a[3]&0xff)); });
        add ("blendARGB/iii", +[] (void*, const i32* a) -> i32 { return (i32) blend ((juce::uint32) a[0], (juce::uint32) a[1]); });
        add ("fillPixel/viii",  +[] (void* c, const i32* a) -> i32 { self(c)->setPix   (a[1], a[2], (juce::uint32) a[0]); return 0; });
        add ("blendPixel/viii", +[] (void* c, const i32* a) -> i32 { self(c)->blendPix (a[1], a[2], (juce::uint32) a[0]); return 0; });
        add ("fillRect/viiiii", +[] (void* c, const i32* a) -> i32 { self(c)->fillRect ((juce::uint32) a[0], a[1], a[2], a[3], a[4], false); return 0; });
        add ("blendRect/viiiii",+[] (void* c, const i32* a) -> i32 { self(c)->fillRect ((juce::uint32) a[0], a[1], a[2], a[3], a[4], true);  return 0; });
        // blendGradientRect (c0,c1,c2,c3, x, y, w, h): the last two args are width/height, not x2/y2.
        // We approximate the 4-corner gradient with a flat fill of the first colour - fine for preview.
        add ("blendGradientRect/viiiiiiii", +[] (void* c, const i32* a) -> i32 { self(c)->fillRect ((juce::uint32) a[0], a[4], a[5], a[6], a[7], true); return 0; });
        add ("blendCircle/vifffb", +[] (void* c, const i32* a) -> i32
        { auto* s=self(c); const juce::uint32 col=(juce::uint32)a[0]; const float cx=asF(a[1]),cy=asF(a[2]),r=asF(a[3]);
          for (int y=0;y<H;++y) for (int x=0;x<W;++x) { const float dx=x-cx, dy=y-cy; if (dx*dx+dy*dy<=r*r) s->blendPix(x,y,col); } return 0; });
        add ("clearDisplay/v",  +[] (void* c, const i32*) -> i32 { self(c)->clearAll (0); return 0; });
        add ("clearDisplay/vi", +[] (void* c, const i32* a) -> i32 { self(c)->clearAll ((juce::uint32) a[0]); return 0; });
        add ("setLED/viii",   +[] (void* c, const i32* a) -> i32 { self(c)->setPix   (a[0], a[1], (juce::uint32) a[2]); return 0; });
        add ("blendLED/viii", +[] (void* c, const i32* a) -> i32 { self(c)->blendPix (a[0], a[1], (juce::uint32) a[2]); return 0; });

        add ("getLocalConfig/ii",  +[] (void* c, const i32* a) -> i32 { const int i=a[0]; return (i>=0 && i<256) ? self(c)->config[(size_t)i] : 0; });
        add ("setLocalConfig/vii", +[] (void* c, const i32* a) -> i32 { const int i=a[0]; if (i>=0 && i<256) self(c)->config[(size_t)i]=a[1]; return 0; });

        auto zero = +[] (void*, const i32*) -> i32 { return 0; };
        auto one  = +[] (void*, const i32*) -> i32 { return 1; };
        for (const char* sig : { "log/vi","logHex/vi","getBatteryLevel/f","isBatteryCharging/b",
             "setStatusOverlayActive/vb","getNumBlocksInTopology/i","getBlockIDForIndex/ii",
             "getBlockIDOnPort/ii","getPortToMaster/i","getBlockTypeForID/ii","sendMessageToBlock/viiii",
             "sendMessageToHost/viii","getHorizontalDistFromMaster/i","getVerticalDistFromMaster/i",
             "getAngleFromMaster/i","setAutoRotate/vb","getClusterIndex/i","getClusterXpos/i",
             "getClusterYpos/i","getBlockIdForBlockInCluster/ii","setClusteringActive/vb",
             "addPressurePoint/vifff","drawPressureMap/v","fadePressureMap/v","drawNumber/viiii",
             "displayBatteryLevel/v","sendMIDI/vi","sendMIDI/vii","sendMIDI/viii","sendNoteOn/viii",
             "sendNoteOff/viii","sendAftertouch/viii","sendCC/viii","sendPitchBend/vii","sendPitchBend/viii",
             "sendChannelPressure/vii","addPitchCorrectionPad/viiffff","setPitchCorrectionEnabled/vb",
             "getPitchCorrectionPitchBend/iii","setChannelRange/vbii","assignChannel/ii","deassignChannel/vii",
             "getControlChannel/i","useMPEDuplicateFilter/vb","getSensorValue/iii","isKeyPressed/bi",
             "handleTouchAsSeaboard/vi","setPowerSavingEnabled/vb","requestRemoteConfig/vii",
             "setRemoteConfig/viii","setLocalConfigItemRange/viii","setLocalConfigActiveState/vibb",
             "linkBlockIDtoController/vi","repaintControl/v","onControlPress/vi","onControlRelease/vi",
             "initControl/viiiiiiiii","setButtonMode/vii","setButtonType/viii","setButtonMinMaxDefault/viiii",
             "setButtonColours/viii","setButtonTriState/vii","padControllerInitDefault/vb","padControllerReset/v",
             "padControllerRegenDefault/v","padControllerRepaint/v","padControllerDrawPad/vi",
             "setUseDefaultKeyHandler/vb","setUseDefaultKeyHandler/vbb" })
            add (sig, zero);

        for (const char* sig : { "isMasterBlock/b","isConnectedToHost/b","isMasterInCurrentCluster/b",
             "getClusterWidth/i","getClusterHeight/i","getNumBlocksInCurrentCluster/i","localHasMIDIOutPort/b" })
            add (sig, one);
    }
};

juce::String grab::generateMetadataPreamble (const juce::String& source)
{
    bool hasSeed = false;
    return metagen::buildPreamble (source, hasSeed);
}

LittleFootApp::LittleFootApp() : impl (std::make_unique<Impl>()) {}
LittleFootApp::~LittleFootApp() = default;

juce::Result LittleFootApp::load (const juce::String& source, const juce::Array<juce::File>& searchPaths)
{ return impl->load (source, searchPaths); }

void         LittleFootApp::repaint()                 { impl->repaint(); }
bool         LittleFootApp::isLoaded() const          { return impl->loaded; }
juce::uint32 LittleFootApp::pixel (int x, int y) const { return impl->pixel (x, y); }
