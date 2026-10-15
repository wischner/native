//
// Reproduces WINGs/Xft font-pattern retention without linking Native.
// A released font and application still leave SDK-owned pattern allocations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <WINGs/WINGs.h>
#include <X11/Xlib.h>
int main(int argc, char **argv) {
    WMInitializeApplication("AllocationControl", &argc, argv);
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return 1;
    WMScreen *screen = WMCreateScreen(display, DefaultScreen(display));
    if (!screen)
        return 1;
    WMFont *font = WMCreateFont(screen, "monospace:pixelsize=12");
    if (font)
        WMReleaseFont(font);
    WMReleaseApplication();
    XCloseDisplay(display);
    return 0;
}
