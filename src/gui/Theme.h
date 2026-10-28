// Theme: GRAB's colour palette and a LookAndFeel that applies it app-wide.
//
// Colours follow ROLI Dashboard's language — a near-black navy canvas, cyan accents, and
// the five 5D-touch dimension colours already used by the curve panels (Strike blue,
// Glide green, Slide magenta, Press teal, Lift gold). This is a shared convention
// (colour-coding the five axes), not a copy of Dashboard's layout.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace grab::theme
{
    // Canvas and surfaces (official ROLI Dashboard values)
    inline const juce::Colour windowBg     { 0xff1a1c27 };  // app background
    inline const juce::Colour panelBg      { 0xff202034 };  // cards, combos, buttons
    inline const juce::Colour panelBgAlt   { 0xff15161f };  // insets, slider tracks (derived)
    inline const juce::Colour outline      { 0xff28283a };  // borders and 5D grid lines

    // Curve panels (card background; grid lines use the border colour)
    inline const juce::Colour curveBg   { 0xff202034 };
    inline const juce::Colour curveGrid { 0xff28283a };

    // Text
    inline const juce::Colour textPrimary   { 0xfff1f1f1 };
    inline const juce::Colour textSecondary { 0xff9aa6b6 };  // derived

    // Accent / highlight and section headers
    inline const juce::Colour accent  { 0xff00b0bb };
    inline const juce::Colour header  { 0xff00b0bb };

    // 5D-touch dimension colours (official; also used by CurveComponent)
    inline const juce::Colour strike { 0xff0be4f4 };
    inline const juce::Colour glide  { 0xff73fe91 };
    inline const juce::Colour slide  { 0xffff77fd };
    inline const juce::Colour press  { 0xff52d7ba };
    inline const juce::Colour lift   { 0xfffbc435 };
}

namespace grab
{

/** Decode a UTF-8 string literal into a juce::String. Needed because juce::String(const char*)
    decodes as ASCII/Latin-1 (each byte -> one codepoint), which mangles multi-byte UTF-8 (an
    em-dash E2 80 94 becomes three garbage chars). Use this for ANY non-ASCII UI text, with the
    bytes written as hex escapes to keep the source ASCII-clean, e.g. grab::u8 ("\xe2\x80\x94"). */
inline juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Dark ROLI-flavoured look, set as the default LookAndFeel for the whole app. */
class GrabLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    GrabLookAndFeel()  : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::getDarkColourScheme())
    {
        using namespace grab::theme;

        setColour (juce::ResizableWindow::backgroundColourId, windowBg);
        setColour (juce::DocumentWindow::textColourId,        textPrimary);

        setColour (juce::Slider::backgroundColourId,      panelBgAlt);
        setColour (juce::Slider::trackColourId,           accent);
        setColour (juce::Slider::thumbColourId,           accent);
        setColour (juce::Slider::textBoxTextColourId,     textPrimary);
        setColour (juce::Slider::textBoxBackgroundColourId, panelBgAlt);
        setColour (juce::Slider::textBoxOutlineColourId,  outline);

        setColour (juce::ComboBox::backgroundColourId, panelBg);
        setColour (juce::ComboBox::textColourId,       textPrimary);
        setColour (juce::ComboBox::outlineColourId,    outline);
        setColour (juce::ComboBox::arrowColourId,      accent);
        setColour (juce::ComboBox::buttonColourId,     panelBg);

        setColour (juce::TextButton::buttonColourId,   panelBg);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId,  textPrimary);
        setColour (juce::TextButton::textColourOnId,   windowBg);

        setColour (juce::ToggleButton::textColourId,         textPrimary);
        setColour (juce::ToggleButton::tickColourId,         accent);
        setColour (juce::ToggleButton::tickDisabledColourId, outline);

        setColour (juce::Label::textColourId, textPrimary);

        setColour (juce::PopupMenu::backgroundColourId,            panelBg);
        setColour (juce::PopupMenu::textColourId,                  textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId,       windowBg);

        // Alert / confirmation dialogs (e.g. factory reset) — match the app surface.
        setColour (juce::AlertWindow::backgroundColourId, panelBg);
        setColour (juce::AlertWindow::textColourId,       textPrimary);
        setColour (juce::AlertWindow::outlineColourId,    outline);

        setColour (juce::TextEditor::backgroundColourId, panelBgAlt);
        setColour (juce::TextEditor::textColourId,       textPrimary);
        setColour (juce::TextEditor::outlineColourId,    outline);
        setColour (juce::TextEditor::highlightColourId,  accent.withAlpha (0.30f));

        setColour (juce::ScrollBar::thumbColourId,        outline.brighter (0.25f));
        setColour (juce::CaretComponent::caretColourId,   accent);
    }

    // Buttons: dark panel by default, accent (cyan) highlight on hover/press and when toggled on,
    // so everything — including the factory-reset dialog — shares GRAB's accent language.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool isOver, bool isDown) override
    {
        using namespace grab::theme;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 5.0f;
        const bool on = b.getToggleState();

        juce::Colour fill = on   ? accent.withAlpha (0.90f)
                          : isDown ? accent.withAlpha (0.28f)
                          : isOver ? panelBg.brighter (0.18f)
                          :          panelBg;
        if (! b.isEnabled())
            fill = panelBg.withAlpha (0.45f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, corner);
        g.setColour ((on || isOver || isDown) && b.isEnabled() ? accent : outline);
        g.drawRoundedRectangle (r, corner, (isOver || isDown) ? 1.4f : 1.0f);
    }
};

} // namespace grab
