// Native title bar theming for macOS: the titlebar is made transparent over the window's background
// colour, with the dark appearance so the title text and traffic lights read on it.

#include "TitleBar.h"
#import <Cocoa/Cocoa.h>

void grab::themeNativeTitleBar (juce::ComponentPeer* peer, juce::Colour background, juce::Colour text)
{
    juce::ignoreUnused (text);   // the dark appearance supplies the light title text

    if (peer == nullptr)
        return;

    auto* view = (NSView*) peer->getNativeHandle();
    NSWindow* window = [view window];
    if (window == nil)
        return;

    window.titlebarAppearsTransparent = YES;
    window.backgroundColor = [NSColor colorWithSRGBRed: background.getFloatRed()
                                                 green: background.getFloatGreen()
                                                  blue: background.getFloatBlue()
                                                 alpha: 1.0];
    window.appearance = [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua];
}
