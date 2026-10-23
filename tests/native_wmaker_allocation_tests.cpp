//
// Reproduces WINGs/Xft font-pattern retention without linking Native.
// A released font and application still leave SDK-owned pattern
// allocations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <WINGs/WINGs.h>
#include <X11/Xlib.h>
#include <cstring>
int main(int argc, char **argv) {
    WMInitializeApplication("AllocationControl", &argc, argv);
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return 1;
    WMScreen *screen = WMCreateScreen(display, DefaultScreen(display));
    if (!screen)
        return 1;
    WMFont *fonts[] = {WMCreateFont(screen, "monospace:pixelsize=12"),
                       WMDefaultSystemFont(screen),
                       WMDefaultBoldSystemFont(screen),
                       WMSystemFontOfSize(screen, 10)};
    for (auto *font : fonts)
        if (font) {
            // Native measurement uses a separate Pango/Xft pattern
            // path.
            const char *samples[] = {"MW",
                                     "Settings",
                                     "\xe2\x96\xb8",
                                     "\xce\xbb",
                                     "\xe2\x86\x94"};
            const Pixmap buffer = XCreatePixmap(
                display,
                DefaultRootWindow(display),
                240,
                40,
                DefaultDepth(display, DefaultScreen(display)));
            for (const auto *sample : samples) {
                const int count = std::strlen(sample);
                WMWidthOfString(font, sample, count);
                WMDrawString(screen,
                             buffer,
                             WMBlackColor(screen),
                             font,
                             0,
                             0,
                             sample,
                             count);
            }
            XFreePixmap(display, buffer);
            WMReleaseFont(font);
        }
    WMReleaseApplication();
    XCloseDisplay(display);
    return 0;
}
