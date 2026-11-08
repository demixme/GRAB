// Native title bar theming for Windows (and the Linux no-op). macOS lives in TitleBar.mm.

#include "TitleBar.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX                 // keep windows.h's min/max macros from poisoning JUCE headers
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <dwmapi.h>
 #include <uxtheme.h>
 #pragma comment (lib, "dwmapi.lib")
 #pragma comment (lib, "uxtheme.lib")
#endif

void grab::themeNativeTitleBar (juce::ComponentPeer* peer, juce::Colour background, juce::Colour text)
{
   #if JUCE_WINDOWS
    if (peer == nullptr)
        return;

    auto hwnd = (HWND) peer->getNativeHandle();
    auto toRef = [] (juce::Colour c) -> COLORREF { return RGB (c.getRed(), c.getGreen(), c.getBlue()); };

    // Dark caption buttons (light min/max/close glyphs) whatever the system theme.
    BOOL dark = TRUE;
    DwmSetWindowAttribute (hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof (dark));

    COLORREF caption = toRef (background), title = toRef (text);
    DwmSetWindowAttribute (hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof (caption));
    DwmSetWindowAttribute (hwnd, 36 /*DWMWA_TEXT_COLOR*/,    &title,   sizeof (title));

    // Title text only: drop the caption icon. The system menu (Alt+Space) and the taskbar icon stay.
    WTA_OPTIONS opts { WTNCA_NODRAWICON, WTNCA_NODRAWICON };
    SetWindowThemeAttribute (hwnd, WTA_NONCLIENT, &opts, sizeof (opts));
   #else
    juce::ignoreUnused (peer, background, text);
   #endif
}
