//
// Sends backend keyboard events to a real canvas, checking the public
// press/repeat/release/reset contract independently of editor commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <stdexcept>

#if defined(__APPLE__)
#include "../lib/native/platforms/mac/globals.h"
#elif defined(MENU_TEST_X11)
#include "../lib/native/toolkits/x11/globals.h"
#include <X11/keysym.h>
#elif defined(MENU_TEST_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#include <X11/keysym.h>
#elif defined(MENU_TEST_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#include <X11/keysym.h>
#endif

// Round-trip classic-manager text as UTF-8, including non-Latin-1 punctuation.
void check_window_title(native::app_wnd &parent) {
#if defined(MENU_TEST_X11) || defined(MENU_TEST_MOTIF) || defined(MENU_TEST_WMAKER)
#if defined(MENU_TEST_X11)
    Widget shell = linux::x11::shell_bindings.handle_from_object(&parent);
    Display *display = XtDisplay(shell);
    Window window = XtWindow(shell);
#elif defined(MENU_TEST_MOTIF)
    Widget shell = linux::openmotif::shell_bindings.handle_from_object(&parent);
    Display *display = XtDisplay(shell);
    Window window = XtWindow(shell);
#else
    Display *display = linux::wmaker::display;
    Window window = WMWidgetXID(linux::wmaker::state(&parent)->window);
#endif
    const auto original = parent.get_title();
    const std::string title = "Native — Café";
    parent.set_title(title);
    XTextProperty property{};
    if (!XGetWMName(display, window, &property))
        throw std::runtime_error("Missing ICCCM window title");
    char **strings = nullptr;
    int count = 0;
    const int status = Xutf8TextPropertyToTextList(display, &property, &strings, &count);
    const bool matches = status == Success && count == 1 && strings && title == strings[0];
    if (strings) XFreeStringList(strings);
    if (property.value) XFree(property.value);
    parent.set_title(original);
    if (!matches) throw std::runtime_error("ICCCM title lost UTF-8 characters");
#else
    (void)parent;
#endif
}

// Exercise the actual view/widget callback rather than on_native_key itself.
void check_canvas_keyboard(native::app_wnd &parent) {
#if defined(__APPLE__) || defined(MENU_TEST_X11) || \
    defined(MENU_TEST_MOTIF) || defined(MENU_TEST_WMAKER)
    native::canvas surface(10, 10, 100, 80);
    surface.set_parent(&parent).create();
    surface.show();
    int presses = 0, releases = 0, repeats = 0, resets = 0;
    surface.on_key.connect([&](native::key_event event) {
        if (event.key != native::key_code::a)
            throw std::runtime_error("Canvas lost physical key identity");
        if (event.action == native::key_action::press) {
            ++presses;
            if (event.repeat) ++repeats;
        } else ++releases;
        return true;
    });
    surface.on_key_reset.connect([&] { ++resets; return false; });
#if defined(__APPLE__)
    NSView *view = mac::canvas_bindings.object_from_handle(&surface)->view;
    [[view window] makeFirstResponder:view];
    auto key = [&](bool down) {
        NSEvent *event = [NSEvent keyEventWithType:down ? NSEventTypeKeyDown : NSEventTypeKeyUp
            location:NSZeroPoint modifierFlags:0 timestamp:0
            windowNumber:[[view window] windowNumber] context:nil
            characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
        if (down) [view keyDown:event]; else [view keyUp:event];
    };
    auto blur = [&] { [[view window] makeFirstResponder:nil]; };
#else
#if defined(MENU_TEST_X11)
    Widget widget = linux::x11::wnd_bindings.handle_from_object(&surface);
    Display *display = XtDisplay(widget);
    Window window = XtWindow(widget);
#elif defined(MENU_TEST_MOTIF)
    Widget widget = linux::openmotif::wnd_bindings.handle_from_object(&surface);
    Display *display = XtDisplay(widget);
    Window window = XtWindow(widget);
#else
    auto *widget = linux::wmaker::wnd_bindings.handle_from_object(&surface);
    Display *display = linux::wmaker::display;
    Window window = WMWidgetXID(widget);
    WMSetFocusToWidget(widget);
#endif
    auto dispatch = [&](XEvent &event) {
#if defined(MENU_TEST_WMAKER)
        WMHandleEvent(&event);
#else
        XtDispatchEvent(&event);
#endif
    };
    auto key = [&](bool down) {
        XEvent event{};
        event.xkey.type = down ? KeyPress : KeyRelease;
        event.xkey.display = display;
        event.xkey.window = window;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.keycode = XKeysymToKeycode(display, XK_a);
        event.xkey.same_screen = True;
        dispatch(event);
    };
    auto blur = [&] {
        XEvent event{};
        event.xfocus.type = FocusOut;
        event.xfocus.display = display;
        event.xfocus.window = window;
        event.xfocus.mode = NotifyNormal;
        event.xfocus.detail = NotifyNonlinear;
        dispatch(event);
    };
#endif
    key(true);
    key(true);
    key(false);
    key(true);
    blur();
    key(false);
    if (presses != 3 || repeats != 1 || releases != 1 || resets != 1)
        throw std::runtime_error("Canvas backend key counts: " +
            std::to_string(presses) + "/" + std::to_string(repeats) + "/" +
            std::to_string(releases) + "/" + std::to_string(resets));
    surface.destroy();
#else
    (void)parent;
#endif
}
