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

        // Fred's design sizes are CSS px (20 readouts, 13 names, 12 values, 11 captions), where the
        // px size IS the em square. JUCE's legacy withHeight() instead sizes ascent+descent, and with
        // Red Hat Display's tall metrics (1.32 em) that renders glyphs ~24% small - and historically
        // differed between Windows and macOS (CoreText), which is why a per-platform fudge was needed.
        // withPointHeight() renders ~1.33x large here (it sizes in typographic points, 1pt ~= 1.33px,
        // not CSS px), so 0.75 lands exactly on Fred's literal token px. But Fred authors on a Mac
        // (Retina + heavier smoothing makes those px read larger), so a small bump matches the design's
        // apparent size on Windows. This is the single knob for the whole type ramp.
        inline constexpr float uiScale = 0.90f;

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

            return juce::Font { juce::FontOptions().withTypeface (tf).withPointHeight (h * uiScale) };
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

        setColour (juce::PopupMenu::backgroundColourId,            panel);
        setColour (juce::PopupMenu::textColourId,                  ink2);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, raisedHover);
        setColour (juce::PopupMenu::highlightedTextColourId,       ink);
        setColour (juce::PopupMenu::headerTextColourId,            label);

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
        const bool on      = b.getToggleState();
        const bool ghost   = (bool) b.getProperties().getWithDefault ("ghost",   false);
        const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
        const bool lit     = on || (primary && b.isEnabled());

        if (ghost) return;   // a text-only link (Revert / In sync): no fill, no outline

        juce::Colour fill = lit            ? ((isOver || isDown) ? primaryHover : primaryFill)
                          : (isOver || isDown) ? raisedHover : raised;
        if (! b.isEnabled())
            fill = raised.withAlpha (0.45f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, corner);

        // A hairline edge only when raised above the surface; a lit (near-white) button needs none.
        if (! lit)
        {
            g.setColour (isOver || isDown ? divider.brighter (0.2f) : divider);
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    // Keep the toggled-on (near-white) button legible: dark text on the primary fill.
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool isOver, bool isDown) override
    {
        using namespace grab::theme;
        const bool on      = b.getToggleState();
        const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
        const bool lit     = on || (primary && b.isEnabled());
        auto col = ! b.isEnabled() ? disabled
                 : lit             ? primaryText
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

    // Fred's .sw: every toggle is a 34x20 pill switch (grey track off, near-white on, 14px thumb).
    // With no button text it's just the switch (boolean config rows); with text, the label sits to
    // its left (Autosave).
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool isOver, bool) override
    {
        using namespace grab::theme;
        const bool on = b.getToggleState();
        const bool en = b.isEnabled();
        auto area = b.getLocalBounds();
        const float tw = 34.0f, th = 20.0f;

        juce::Rectangle<float> sw;
        const auto text = b.getButtonText();
        if (text.isNotEmpty())
        {
            g.setColour (en ? ink2 : disabled);
            g.setFont (type::font (type::Role::body));
            const int twd = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text);
            g.drawText (text, area.getX(), area.getY(), twd + 2, area.getHeight(), juce::Justification::centredLeft);
            sw = { (float) area.getX() + (float) twd + 10.0f, (float) area.getCentreY() - th * 0.5f, tw, th };
        }
        else
        {
            sw = { (float) area.getX(), (float) area.getCentreY() - th * 0.5f, tw, th };
        }

        g.setColour (! en ? switchOff.withAlpha (0.5f) : on ? primaryFill : (isOver ? switchOffHover : switchOff));
        g.fillRoundedRectangle (sw, th * 0.5f);
        const float kr = th - 6.0f;
        const float kx = on ? sw.getRight() - kr - 3.0f : sw.getX() + 3.0f;
        g.setColour (! en ? ink.withAlpha (0.5f) : on ? primaryText : ink);
        g.fillEllipse (kx, sw.getY() + 3.0f, kr, kr);
    }

    // Fred's .sl: a 2px knob-track rail inset 6px, an ink fill up to a 12px ink thumb.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        using namespace grab::theme;
        const float cy    = (float) y + (float) height * 0.5f;
        const float left  = (float) x + 6.0f;
        const float right = (float) x + (float) width - 6.0f;
        const float tx    = juce::jlimit (left, right, sliderPos);

        g.setColour (knobTrack);
        g.fillRoundedRectangle (left, cy - 1.0f, right - left, 2.0f, 1.0f);
        g.setColour (ink);
        g.fillRoundedRectangle (left, cy - 1.0f, juce::jmax (0.0f, tx - left), 2.0f, 1.0f);
        g.fillEllipse (tx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    }

    // Fred's .combo: a flat sunken well (no outline), rounded, with a down chevron in the label
    // colour on the right. Hover lifts to wellHover.
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        using namespace grab::theme;
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (box.isMouseOver (true) ? wellHover : well);
        g.fillRoundedRectangle (r, (float) radius::control);

        const float cx = (float) width - 16.0f, cy = (float) height * 0.5f, w = 4.0f, h = 2.5f;
        juce::Path chev;
        chev.startNewSubPath (cx - w, cy - h);
        chev.lineTo          (cx,     cy + h);
        chev.lineTo          (cx + w, cy - h);
        g.setColour (box.isEnabled() ? label : disabled);
        g.strokePath (chev, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return grab::theme::type::font (grab::theme::type::Role::control);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 0, box.getWidth() - 12 - 26, box.getHeight());
        label.setFont (grab::theme::type::font (grab::theme::type::Role::control));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    // ---- Popup menus (Fred's .menu): a rounded panel, items with a 7px rounded hover/selected pill,
    //      a left checkmark on the ticked item, and caption-grey section headers. --------------------
    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        using namespace grab::theme;
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        // The menu window isn't transparent on Windows, so cover its (white) backdrop with the window
        // tone first; the rounded panel then reads with dark corners rather than white ones.
        g.fillAll (window);
        g.setColour (panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (divider);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    // Device menu callout: no arrow, a rounded panel like the popup menus (not JUCE's default box).
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        using namespace grab::theme;
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, {}).drawForPath (g, path);
        g.setColour (panel);
        g.fillPath (path);
        g.setColour (divider);
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 12; }

    juce::Font getPopupMenuFont() override { return grab::theme::type::font (grab::theme::type::Role::body); }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int,
                                    int& idealWidth, int& idealHeight) override
    {
        if (isSeparator) { idealWidth = 60; idealHeight = 11; return; }
        idealHeight = 32;
        idealWidth  = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 28 + 18;
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        using namespace grab::theme;
        g.setColour (label);
        g.setFont (type::font (type::Role::caption));
        g.drawText (sectionName, area.reduced (10, 0).withTrimmedBottom (3),
                    juce::Justification::bottomLeft, true);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable*, const juce::Colour*) override
    {
        using namespace grab::theme;
        if (isSeparator)
        {
            auto s = area.withSizeKeepingCentre (area.getWidth() - 10, 1);
            g.setColour (divider);
            g.fillRect (s);
            return;
        }

        auto r = area.reduced (4, 1);
        if (isHighlighted && isActive) { g.setColour (raisedHover); g.fillRoundedRectangle (r.toFloat(), 7.0f); }
        else if (isTicked)             { g.setColour (segmentOn);   g.fillRoundedRectangle (r.toFloat(), 7.0f); }

        if (isTicked)   // left checkmark
        {
            const float cx = (float) r.getX() + 16.0f, cy = (float) r.getCentreY();
            juce::Path check;
            check.startNewSubPath (cx - 4.0f, cy + 0.5f);
            check.lineTo          (cx - 1.0f, cy + 3.5f);
            check.lineTo          (cx + 4.5f, cy - 3.5f);
            g.setColour (ink);
            g.strokePath (check, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (! isActive ? disabled : (isHighlighted || isTicked) ? ink : ink2);
        g.setFont (getPopupMenuFont());
        auto textR = r.withTrimmedLeft (28).withTrimmedRight (hasSubMenu || shortcutKeyText.isNotEmpty() ? 22 : 10);
        g.drawText (text, textR, juce::Justification::centredLeft, true);

        if (hasSubMenu)   // right chevron
        {
            const float cx = (float) r.getRight() - 12.0f, cy = (float) r.getCentreY(), w = 2.5f, h = 3.5f;
            juce::Path chev;
            chev.startNewSubPath (cx - w, cy - h);
            chev.lineTo          (cx + w, cy);
            chev.lineTo          (cx - w, cy + h);
            g.setColour (label);
            g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
};

} // namespace grab
