// Theme: GRAB's colour palette, shape/spacing/type tokens, and a LookAndFeel that applies
// them app-wide.
//
// Source of truth: the "GRAB studio" token sheet from Fred's design track (PR #23,
// design/ui/prototype/tokens.json). The language is a near-black greyscale bento: surfaces
// are flat tone steps (no borders; sunken "wells" instead), text is a four-tier ink ladder,
// and the ONLY saturated colours are the five 5D-dimension hues (used for curves, chips, knob
// arcs and the dots that tie a setting to its dimension). There is deliberately no chromatic
// "accent" any more - the old cyan is gone; "active/selected" reads as bright ink or a raised
// grey. See docs/design/fabfilter-ui-redesign.md.
//
// Back-compat: the pre-redesign code referred to a handful of symbols (windowBg, panelBg,
// accent, strike...). They are kept below as aliases onto the new tokens so every call site
// keeps compiling while the palette shifts in one move. New code should prefer the named
// tokens (window, panel, ink, segmentOn, ...).

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "GRABFonts.h"   // embedded Red Hat Display weights (FontData namespace; see CMakeLists.txt)

namespace grab::theme
{
    // ---- Surfaces (tone ladder, darkest to lightest) --------------------------------------
    inline const juce::Colour chrome      { 0xff0e0e10 };  // top bar, status bar
    inline const juce::Colour window      { 0xff131315 };  // window background, gutters between tiles
    inline const juce::Colour display     { 0xff0c0c0e };  // the graph tile: recessed, darker than the rest
    inline const juce::Colour panel       { 0xff1b1b1e };  // every other tile, popup menus, dialogs
    inline const juce::Colour tileInset   { 0xff151517 };  // a sunken row inside a tile (fixed velocity)
    inline const juce::Colour well        { 0xff111113 };  // sunken controls: combo, stepper, segmented track, CC cards
    inline const juce::Colour wellHover   { 0xff18181b };  // sunken control, hover
    inline const juce::Colour raised      { 0xff1c1c1f };  // button fill, small pills
    inline const juce::Colour raisedHover { 0xff242428 };  // button / chip / menu-item hover
    inline const juce::Colour segmentOn   { 0xff2c2c31 };  // pressed button; selected segment, chip, menu item
    inline const juce::Colour knobFace    { 0xff232327 };  // knob disc
    inline const juce::Colour knobTrack   { 0xff2c2c31 };  // knob and slider track
    inline const juce::Colour switchOff   { 0xff3a3a3f };  // switch track, off
    inline const juce::Colour switchOffHover { 0xff46464c };  // switch track, off, hover
    inline const juce::Colour divider     { 0xff26262a };  // thin separators in bars
    inline const juce::Colour chromeLine  { 0xff222226 };  // line under the top bar / over the status bar
    inline const juce::Colour grid        { 0xff1e1e22 };  // graph grid, the graph tile's 1 px inner edge

    // ---- Ink (text) -----------------------------------------------------------------------
    inline const juce::Colour ink      { 0xfff2f2ef };  // primary text, values, focus ring, changed dot
    inline const juce::Colour ink2     { 0xffb8b8b4 };  // secondary: row names, idle buttons, help text
    inline const juce::Colour label    { 0xff8c8c88 };  // captions, axis labels, units
    inline const juce::Colour disabled { 0xff5e5e5a };  // disabled controls only, never readable text

    // ---- Primary action (Save-to-device: near-white fill, dark text) ----------------------
    inline const juce::Colour primaryFill    { 0xfff2f2ef };
    inline const juce::Colour primaryHover   { 0xffffffff };
    inline const juce::Colour primaryPressed { 0xffd6d6d2 };
    inline const juce::Colour primaryText    { 0xff0e0e10 };  // text on the primary button; switch thumb when on

    // ---- 5D-dimension hues (the only saturated colours; remapped in PR #23) ---------------
    //      was (pre-redesign): strike cyan, glide green, slide magenta, press teal, lift gold
    inline const juce::Colour strike { 0xfff0302f };  // red
    inline const juce::Colour glide  { 0xffff9a1f };  // orange
    inline const juce::Colour slide  { 0xffa8dc3c };  // lime
    inline const juce::Colour press  { 0xff27cbdd };  // cyan
    inline const juce::Colour lift   { 0xff7b6dff };  // violet

    // ---- Status ---------------------------------------------------------------------------
    inline const juce::Colour statusOk    { 0xffa8dc3c };
    inline const juce::Colour statusWarn  { 0xffff9a1f };
    inline const juce::Colour statusError { 0xfff0302f };

    // ---- Shape: corner radii (px) ---------------------------------------------------------
    namespace radius
    {
        inline constexpr float segmentInner = 7.0f;
        inline constexpr float control      = 10.0f;
        inline constexpr float card         = 10.0f;
        inline constexpr float inset        = 12.0f;
        inline constexpr float tile         = 16.0f;
        inline constexpr float pill         = 999.0f;  // fully rounded (switch)
    }

    // ---- Spacing (px) ---------------------------------------------------------------------
    namespace space
    {
        inline constexpr int unit          = 4;
        inline constexpr int gutter        = 12;   // between tiles
        inline constexpr int tilePadX      = 16;
        inline constexpr int tilePadY      = 12;
        inline constexpr int barInset      = 28;
        inline constexpr int rowHeight     = 32;
        inline constexpr int rowGap        = 8;
        inline constexpr int controlColumn = 200;
        inline constexpr int compactControl = 104;
    }

    // ---- Sizes (px) -----------------------------------------------------------------------
    namespace size
    {
        inline constexpr int topBar       = 52;
        inline constexpr int statusBar    = 28;
        inline constexpr int control      = 32;
        inline constexpr int controlLarge = 36;
        inline constexpr int iconButton   = 36;
        inline constexpr int knob         = 52;
        inline constexpr int knobCompact  = 48;
        inline constexpr int knobTrack    = 4;
        inline constexpr int switchTrackW = 34;
        inline constexpr int switchTrackH = 20;
        inline constexpr int switchThumb  = 14;
        inline constexpr int sliderTrack  = 2;
        inline constexpr int sliderThumb  = 12;
        inline constexpr int changedDot   = 6;
        inline constexpr int chip         = 30;
        inline constexpr int deviceUnit   = 32;  // device art: 3 units tall, 1 unit = 32 px
    }

    // ---- Borders (px) ---------------------------------------------------------------------
    namespace border
    {
        inline constexpr float chromeLine  = 1.0f;
        inline constexpr float graphEdge   = 1.0f;
        inline constexpr float focusRing   = 2.0f;
        inline constexpr float focusOffset = 2.0f;
    }

    // ---- Type -----------------------------------------------------------------------------
    //  One family, five roles. Red Hat Display (SIL OFL) is the target face; until its binary
    //  is embedded (next step: juce_add_binary_data + setDefaultSansSerifTypeface), these fall
    //  back to the platform sans at the right size/weight. Sizes are px heights.
    namespace type
    {
        enum class Role { readout, title, body, control, caption };

        // The three embedded Red Hat Display weights, loaded once each (lazy function-local
        // statics, so merely including this header pulls in no font data until a face is asked for).
        namespace detail
        {
            inline juce::Typeface::Ptr make (const void* data, int size)
            {
                return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            }
            inline juce::Typeface::Ptr regular()
            {
                static juce::Typeface::Ptr t = make (FontData::RedHatDisplayRegular_ttf,  FontData::RedHatDisplayRegular_ttfSize);
                return t;
            }
            inline juce::Typeface::Ptr medium()
            {
                static juce::Typeface::Ptr t = make (FontData::RedHatDisplayMedium_ttf,   FontData::RedHatDisplayMedium_ttfSize);
                return t;
            }
            inline juce::Typeface::Ptr semibold()
            {
                static juce::Typeface::Ptr t = make (FontData::RedHatDisplaySemiBold_ttf, FontData::RedHatDisplaySemiBold_ttfSize);
                return t;
            }
        }

        // Fred's design sizes are CSS px (20 readouts, 13 names, 12 values, 11 captions). JUCE's
        // withHeight on Windows renders those SMALLER than the browser does (height counts the em's
        // ascent+descent, so the visible glyphs end up below the CSS px), so we scale up to land on
        // the web-intended size. One knob to tune the whole type ramp if it reads large or small.
        inline constexpr float sizeScale = 1.18f;

        inline juce::Font font (Role r)
        {
            float h = 13.0f;
            juce::Typeface::Ptr tf = detail::regular();

            switch (r)
            {
                case Role::readout: h = 20.0f; tf = detail::medium();   break;  // knob readouts (weight 500)
                case Role::title:   h = 13.0f; tf = detail::semibold(); break;  // tile titles, device name (weight 600)
                case Role::body:    h = 13.0f; break;  // row names, buttons, menus, chips (weight 400)
                case Role::control: h = 12.0f; break;  // values inside controls
                case Role::caption: h = 11.0f; break;  // device facts, units, axis labels, status bar
            }

            return juce::Font { juce::FontOptions().withTypeface (tf).withHeight (h * sizeScale) };
        }
    }

    // ---- Back-compat aliases (pre-redesign symbols; repointed onto the tokens above) -------
    inline const juce::Colour windowBg      = window;
    inline const juce::Colour panelBg       = panel;
    inline const juce::Colour panelBgAlt    = well;     // insets / slider tracks
    inline const juce::Colour outline       = divider;  // control borders (now a quiet separator)
    inline const juce::Colour curveBg       = display;  // the graph tile
    inline const juce::Colour curveGrid     = grid;
    inline const juce::Colour textPrimary   = ink;
    inline const juce::Colour textSecondary = ink2;
    inline const juce::Colour accent        = ink;      // design is monochrome: "active" = bright ink
    inline const juce::Colour header        = ink;      // section titles: bold ink, not a colour
}

namespace grab
{

/** Decode a UTF-8 string literal into a juce::String. Needed because juce::String(const char*) -
    and String + const char*, String << const char* - decode as ASCII/Latin-1 (each byte -> one
    codepoint), which mangles multi-byte UTF-8 (an em-dash E2 80 94 shows as three garbage chars).

    CONVENTION: keep UI text ASCII. It's too easy to leak a raw byte - e.g. an em-dash embedded in
    a literal that's appended to a String with '+' renders as mojibake even if a neighbouring one is
    wrapped. So use plain ASCII ("-", "->", ">") for UI strings. This helper stays for the rare case
    where real UTF-8 is unavoidable (then wrap EVERY piece of the expression, not just one). */
inline juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Dark greyscale "GRAB studio" look, set as the default LookAndFeel for the whole app. */
class GrabLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    GrabLookAndFeel()  : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::getDarkColourScheme())
    {
        using namespace grab::theme;

        setColour (juce::ResizableWindow::backgroundColourId, window);
        setColour (juce::DocumentWindow::textColourId,        ink);

        setColour (juce::Slider::backgroundColourId,        well);
        setColour (juce::Slider::trackColourId,             knobTrack);
        setColour (juce::Slider::thumbColourId,             ink);
        setColour (juce::Slider::textBoxTextColourId,       ink);
        setColour (juce::Slider::textBoxBackgroundColourId, well);
        setColour (juce::Slider::textBoxOutlineColourId,    divider);

        setColour (juce::ComboBox::backgroundColourId, well);
        setColour (juce::ComboBox::textColourId,       ink);
        setColour (juce::ComboBox::outlineColourId,    divider);
        setColour (juce::ComboBox::arrowColourId,      ink2);
        setColour (juce::ComboBox::buttonColourId,     well);

        setColour (juce::TextButton::buttonColourId,   raised);
        setColour (juce::TextButton::buttonOnColourId, segmentOn);
        setColour (juce::TextButton::textColourOffId,  ink2);
        setColour (juce::TextButton::textColourOnId,   ink);

        setColour (juce::ToggleButton::textColourId,         ink);
        setColour (juce::ToggleButton::tickColourId,         ink);
        setColour (juce::ToggleButton::tickDisabledColourId, disabled);

        setColour (juce::Label::textColourId, ink);

        setColour (juce::PopupMenu::backgroundColourId,            panel);
        setColour (juce::PopupMenu::textColourId,                  ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, segmentOn);
        setColour (juce::PopupMenu::highlightedTextColourId,       ink);

        // Alert / confirmation dialogs (e.g. factory reset) - match the app surface.
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId,       ink);
        setColour (juce::AlertWindow::outlineColourId,    divider);

        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId,       ink);
        setColour (juce::TextEditor::outlineColourId,    divider);
        setColour (juce::TextEditor::highlightColourId,  ink.withAlpha (0.22f));

        setColour (juce::ScrollBar::thumbColourId,      divider.brighter (0.25f));
        setColour (juce::CaretComponent::caretColourId, ink);
    }

    // Every font without an explicit typeface resolves to Red Hat Display, weight chosen from
    // the font's style - so the whole app adopts the family, including call sites not yet moved
    // to theme::type::font(). Fonts built with an explicit typeface (theme::type::font) keep it.
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        const auto style = f.getTypefaceStyle();
        if (style.containsIgnoreCase ("semi") || style.containsIgnoreCase ("bold"))
            return grab::theme::type::detail::semibold();
        if (style.containsIgnoreCase ("medium"))
            return grab::theme::type::detail::medium();
        return grab::theme::type::detail::regular();
    }

    // Buttons: raised grey by default, a brighter grey on hover, and the selected (toggled-on)
    // state as the near-white segment fill - greyscale throughout; the five hues stay reserved
    // for the 5D dimensions. "On" buttons draw their text in the dark primaryText for contrast.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool isOver, bool isDown) override
    {
        using namespace grab::theme;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = radius::control;
        const bool on = b.getToggleState();

        juce::Colour fill = on     ? primaryFill
                          : isDown ? raisedHover
                          : isOver ? raisedHover
                          :          raised;
        if (! b.isEnabled())
            fill = raised.withAlpha (0.45f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, corner);

        // A hairline edge only when raised above the surface; selected (near-white) needs none.
        if (! on)
        {
            g.setColour (isOver || isDown ? divider.brighter (0.2f) : divider);
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    // Keep the toggled-on (near-white) button legible: dark text on the primary fill.
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool isOver, bool isDown) override
    {
        using namespace grab::theme;
        const bool on = b.getToggleState();
        auto col = ! b.isEnabled() ? disabled
                 : on              ? primaryText
                 : (isOver || isDown) ? ink
                 :                   ink2;
        g.setColour (col);
        g.setFont (type::font (type::Role::body));

        const int ydent = juce::jmin (4, b.getHeight() / 4);
        const int cnrSz = juce::jmin (b.getHeight(), b.getWidth()) / 2;
        const int left  = b.getWidth()  > cnrSz * 2 ? cnrSz : 0;
        g.drawFittedText (b.getButtonText(),
                          left, ydent, b.getWidth() - left * 2, b.getHeight() - ydent * 2,
                          juce::Justification::centred, 2);
    }
};

} // namespace grab
