// AirwavePanel - an EXPERIMENTAL / DOCUMENTATION space, not a device GRAB configures.
//
// ROLI Airwave has no on-device MIDI settings (it's a host-side gesture controller), so it's out
// of scope for GRAB's read/write config work. But it's an Ultraleap IR-camera rig whose software
// is ROLI-server-locked, so this panel documents what it is, what to archive before ROLI's servers
// go, and the open ecosystem that can drive the same camera without ROLI. It talks to no hardware.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Theme.h"

class AirwavePanel  : public juce::Component
{
public:
    AirwavePanel()
    {
        addAndMakeVisible (title);
        title.setText ("Airwave & gesture control " + grab::u8 ("-") + " experimental",
                       juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
        title.setColour (juce::Label::textColourId, grab::theme::accent);

        addAndMakeVisible (body);
        body.setMultiLine (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setColour (juce::TextEditor::backgroundColourId, grab::theme::panelBgAlt);
        body.setColour (juce::TextEditor::outlineColourId,    grab::theme::outline);
        populate();

        auto linkBtn = [this] (juce::TextButton& b, juce::String label, const char* url)
        {
            addAndMakeVisible (b);
            b.setButtonText (label);
            b.onClick = [url] { juce::URL (url).launchInDefaultBrowser(); };
        };
        linkBtn (ultraleapBtn, "Ultraleap tracking", "https://www.ultraleap.com/");
        linkBtn (midipawBtn,   "MidiPaw",            "https://midipaw.com/");
        linkBtn (bridgesBtn,   grab::u8 ("Leap->MIDI (GitHub)"), "https://github.com/topics/leapmotion");

        addAndMakeVisible (closeBtn);
        closeBtn.setButtonText ("Close");
        closeBtn.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                juce::MessageManager::callAsync ([dw] { delete dw; });
        };

        setSize (700, 650);
    }

    void paint (juce::Graphics& g) override { g.fillAll (grab::theme::windowBg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (32);
        closeBtn.setBounds   (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        bridgesBtn.setBounds (bottom.removeFromRight (150));
        bottom.removeFromRight (6);
        midipawBtn.setBounds (bottom.removeFromRight (110));
        bottom.removeFromRight (6);
        ultraleapBtn.setBounds (bottom.removeFromRight (140));
        r.removeFromBottom (8);
        body.setBounds (r);
    }

private:
    enum Style { Body, H1, H2, Click, Note };

    void add (Style s, const char* utf8)
    {
        juce::Font   f (juce::FontOptions (15.0f));
        juce::Colour col = grab::theme::textPrimary;
        switch (s)
        {
            case H1:   f = juce::Font (juce::FontOptions (17.0f)).boldened(); col = grab::theme::accent; break;
            case H2:   f = juce::Font (juce::FontOptions (15.0f)).boldened();                            break;
            case Click:col = grab::theme::accent;                                                        break;
            case Note: f = juce::Font (juce::FontOptions (13.5f)); col = grab::theme::textPrimary.withAlpha (0.72f); break;
            case Body: default: break;
        }
        body.setFont (f);
        body.setColour (juce::TextEditor::textColourId, col);
        body.insertTextAtCaret (juce::String::fromUTF8 (utf8));
    }

    void populate()
    {
        add (Note,  "Experimental / documentation. GRAB does not drive an Airwave: its hand expression is produced in "
                    "host software, not on the device. This page is research - what Airwave is, its exact MIDI, what "
                    "to archive before ROLI's servers go, and how the same camera could run without ROLI. "
                    "(Details below are from ROLI's own Airwave manual.)\n\n");

        add (H1,    "What Airwave actually is\n");
        add (Body,  "An overhead tower holding two ");
        add (H2,    "Ultraleap IR 170");
        add (Body,  " infrared cameras plus ROLI's \"ROLI Vision\" software - 27 joints per hand at 90 fps, tracking "
                    "range ~100-1000 mm. The cameras are standard Ultraleap devices (the macOS driver is literally "
                    "UltraleapUVCDriver), not bespoke ROLI hardware. Over an internal USB2 hub it presents three "
                    "class-compliant devices:\n");
        add (Body,  "  -  ");
        add (H2,    "ROLI Airwave Expression");
        add (Body,  " - the hand-gesture MIDI (the CCs below)\n");
        add (Body,  "  -  ");
        add (H2,    "ROLI Airwave Pedal");
        add (Body,  " - pedal MIDI, plus Airwave's own config + firmware via ROLI Dashboard\n");
        add (Body,  "  -  ");
        add (H2,    "Airwave Audio");
        add (Body,  " - a class-compliant audio interface (headphone out)\n");
        add (Note,  "  The IR-camera-to-MIDI translation runs in host software (ROLI Airwave Control), not on the device.\n\n");

        add (H1,    "The six Dimensions of Air (how it really works)\n");
        add (Body,  "Raise, Tilt, Glide, Slide, Flex, Grasp - all computed from ");
        add (H2,    "wrist position and the plane of your metacarpals");
        add (Body,  ". It explicitly ignores individual fingers, and only tracks hands above the keyboard. Each hand is "
                    "computed separately and most dimensions use the LARGER of the two hands (Grasp is the exception: it "
                    "sums both - grasp both hands to reach 100%). So it is per-HAND, not per-note / per-finger like a "
                    "Seaboard. Values reset to 0 when tracking is lost and no notes are held.\n\n");

        add (H1,    "The exact MIDI (ROLI Airwave Expression)\n");
        add (Body,  "Each hand on its own CC, 0-127 (unsigned, scaled); notes come separately from your keyboard:\n");
        add (Click, "    CC21  Grasp (both hands summed)\n");
        add (Click, "    CC22 / CC23   Slide  Left / Right\n");
        add (Click, "    CC24 / CC25   Glide  Left / Right\n");
        add (Click, "    CC26 / CC27   Raise  Left / Right\n");
        add (Click, "    CC28 / CC29   Tilt   Left / Right\n");
        add (Click, "    CC30 / CC31   Flex   Left / Right\n");
        add (Note,  "  CC28/29 are undefined in the MIDI spec, so some plug-ins hijack them for preset switching.\n\n");

        add (H1,    "What this means for a ROLI-free rebuild\n");
        add (Body,  "The hard part is already open. The cameras are commodity Ultraleap - its SDK hands you wrist "
                    "position, hand orientation and grab strength directly, so you never touch the raw IR feed. The "
                    "mapping is simple geometry, and the output is a documented, class-compliant CC set (CC21-31). An "
                    "Ultraleap-SDK app can emit those exact CCs with no ROLI software; \"ROLI Airwave Control\" is "
                    "essentially that mapper, and it's replaceable. Ready-made stand-ins and parts:\n");
        add (Click, "    MidiPaw  (midipaw.com)");
        add (Body,  " - free, Windows, Ultraleap -> MIDI (not open source, closest turnkey).\n");
        add (Body,  "    Open-source: ");
        add (Click, "Emchei/leapmotion-midi");
        add (Body,  ", ");
        add (Click, "ndiesslin/LeapMidiBridge");
        add (Body,  ", ");
        add (Click, "geekychris/leap_motion_harp");
        add (Body,  ". The Max/MSP \"Ultraleap\" package supports the IR 170 directly.\n\n");

        add (H1,    "The GRAB angle\n");
        add (Body,  "  -  GRAB doesn't drive Airwave (its expression is host-side), but ");
        add (H2,    "Capture MIDI");
        add (Body,  " can log the Expression CCs directly - it's class-compliant USB MIDI - handy for study.\n");
        add (Body,  "  -  The ROLI Airwave Pedal device is configured over ROLI's own MIDI protocol via Dashboard "
                    "(pedal type/model, pitch transpose) - a small surface GRAB could in principle read/write like other "
                    "ROLI gear. (GRAB never touches firmware.)\n");
        add (Body,  "  -  ");
        add (H2,    "Airwave CC tester");
        add (Body,  " (Advanced > Airwave > CC tester) emits the exact CC21-31 to a MIDI output, so you can map "
                    "and test Airwave gestures in a DAW without the hardware.\n\n");

        add (H1,    "Archive now, and the licence clock\n");
        add (Body,  "To your own disk, while ROLI's servers are up: ROLI Airwave Control (carries UltraleapUVCDriver + "
                    "ROLI Vision), Airwave Player + its factory-library package, the manuals, and - the durable path "
                    "- ");
        add (Click, "Ultraleap's SDK");
        add (Body,  " from ultraleap.com, which drives the camera with no ROLI software at all. Airwave Player runs "
                    "about four weeks offline before it must phone home to ROLI again - the same lease as Equator2.\n");
        add (Note,  "  Keep copies for yourself; don't publicly re-host ROLI's installers, firmware or manuals.\n");

        body.setReadOnly (true);
        body.setCaretPosition (0);
    }

    juce::Label      title;
    juce::TextEditor body;
    juce::TextButton ultraleapBtn, midipawBtn, bridgesBtn, closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirwavePanel)
};
