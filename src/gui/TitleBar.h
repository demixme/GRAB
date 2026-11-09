// Native title bar theming: the caption reads as part of the window (Fred's design) - the window's
// background colour, light title text, no caption icon. Windows: DWM caption colours + no icon.
// macOS: transparent titlebar over the window colour (standard windows carry no title icon there).
// Linux: the window manager draws the decorations, so this is a no-op.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace grab
{
    // Call once the window is visible (it needs the native peer); safe to call again.
    void themeNativeTitleBar (juce::ComponentPeer* peer, juce::Colour background, juce::Colour text);
}
