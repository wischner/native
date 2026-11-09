//
// Sends backend keyboard events to a real canvas, checking the public
// press/repeat/release/reset contract independently of editor commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <stdexcept>

#if defined(_WIN32)
#include "../lib/native/platforms/windows/globals.h"
#elif defined(__APPLE__)
#include "../lib/native/platforms/mac/globals.h"
#elif defined(MENU_TEST_X11)
#include "../lib/native/toolkits/x11/globals.h"
#include "../lib/native/toolkits/x11/scale.h"
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#elif defined(MENU_TEST_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#include <X11/keysym.h>
#elif defined(MENU_TEST_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#include <X11/keysym.h>
#elif defined(MENU_TEST_OPENLOOK)
#include "../lib/native/toolkits/openlook/globals.h"
#include <X11/keysym.h>
#include "../lib/native/toolkits/x_keyboard.h"
#elif defined(MENU_TEST_GEMIX)
#include "../lib/native/toolkits/gemix/globals.h"
#endif

// Round-trip classic-manager text as UTF-8, including non-Latin-1 punctuation.
void check_window_title(native::app_wnd &parent) {
#if defined(MENU_TEST_X11) || defined(MENU_TEST_MOTIF) || defined(MENU_TEST_WMAKER)
#if defined(MENU_TEST_X11)
    Widget shell = linux::x11::shell_bindings.handle_from_object(&parent);
    Widget client = linux::x11::wnd_bindings.handle_from_object(&parent);
    Dimension width = 1, height = 1;
    XtVaGetValues(client, XtNwidth, &width, XtNheight, &height, nullptr);
    Position top = 0;
    Dimension host_height = 1;
    XtVaGetValues(client, XtNy, &top, nullptr);
    XtVaGetValues(XtParent(client), XtNheight, &host_height, nullptr);
    if (int(top) + int(height) > int(host_height))
        throw std::runtime_error("Native menu clips the bottom of the drawing widget");
    const auto actual = linux::x11::size_to_logical(width, height);
    const auto reported = parent.get_dimensions();
    if (reported.w != actual.w || reported.h != actual.h)
        throw std::runtime_error("Initial client bounds include the native menu strip");
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
#if defined(MENU_TEST_GEMIX)
    if (native::app::get_unicode_text_input_supported())
        throw std::runtime_error("AES byte packets cannot advertise Unicode commits");
#else
    if (!native::app::get_unicode_text_input_supported())
        throw std::runtime_error("Unicode text backend lost its portable capability");
#endif
#if defined(_WIN32)
    native::canvas surface(10, 10, 100, 80);
    surface.set_parent(&parent).create();
    surface.show();
    surface.focus();
    const HWND handle = windows::wnd_bindings.handle_from_object(&surface);
    if (GetFocus() != handle) throw std::runtime_error("Canvas native focus missing");
    std::string typed;
    surface.on_text_input.connect([&](const std::string &text) {
        typed += text; return true;
    });
    surface.on_key.connect([](native::key_event event) {
        return event.key == native::key_code::a;
    });
    SendMessageW(handle, WM_KEYDOWN, 'A', 1 | (0x1eL << 16));
    SendMessageW(handle, WM_CHAR, 'a', 1 | (0x1eL << 16));
    SendMessageW(handle, WM_KEYUP, 'A', 1 | (0x1eL << 16) | (1LL << 31));
    if (!typed.empty()) throw std::runtime_error("Consumed Windows key produced text");
    SendMessageW(handle, WM_KEYDOWN, 'B', 1 | (0x30L << 16));
    SendMessageW(handle, WM_CHAR, 0x017e, 1 | (0x30L << 16));
    SendMessageW(handle, WM_CHAR, 0xd83d, 1 | (0x30L << 16));
    SendMessageW(handle, WM_CHAR, 0xde00, 1 | (0x30L << 16));
    if (typed != "\xc5\xbe\xf0\x9f\x98\x80")
        throw std::runtime_error("Windows canvas lost UTF-16 composed text");
    surface.destroy();
#elif defined(MENU_TEST_GEMIX)
    native::canvas writer(10, 10, 100, 80);
    writer.set_parent(&parent).create();
    writer.show();
    std::string written;
    writer.on_text_input.connect([&](const std::string &utf8) {
        written += utf8;
        return true;
    });
    writer.on_native_focus(true);
    if (!linux::gemix::handle_canvas_key(&parent, 0, 'a') ||
        !linux::gemix::handle_canvas_key(&parent, 3, 'A') || written != "aA")
        throw std::runtime_error("GEM canvas text: '" + written + "'");
    if (linux::gemix::handle_canvas_key(&parent, 4, 'a'))
        throw std::runtime_error("A Ctrl chord must not type");
    writer.destroy();
#elif defined(__APPLE__) || defined(MENU_TEST_X11) || \
    defined(MENU_TEST_MOTIF) || defined(MENU_TEST_WMAKER) || \
    defined(MENU_TEST_OPENLOOK)
    native::canvas surface(10, 10, 100, 80);
    surface.set_parent(&parent).create();
    surface.show();
    int presses = 0, releases = 0, repeats = 0, resets = 0;
    bool consume_keys = true;
    std::string typed;
    surface.on_text_input.connect([&](const std::string &utf8) {
        typed += utf8;
        return true;
    });
    surface.on_key.connect([&](native::key_event event) {
        if (!consume_keys)
            return false;
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
#elif defined(MENU_TEST_OPENLOOK)
    Display *display = linux::openlook::cached_display;
    const Window window = xv_get(
        linux::openlook::canvas_bindings.object_from_handle(&surface)
            ->paint_window,
        XV_XID);
#else
    auto *widget = linux::wmaker::wnd_bindings.handle_from_object(&surface);
    Display *display = linux::wmaker::display;
    Window window = WMWidgetXID(widget);
    WMSetFocusToWidget(widget);
#endif
    auto dispatch = [&](XEvent &event) {
#if defined(MENU_TEST_WMAKER)
        WMHandleEvent(&event);
#elif !defined(MENU_TEST_OPENLOOK)
        XtDispatchEvent(&event);
#endif
    };
#if defined(MENU_TEST_OPENLOOK)
    // XView drops synthetic keys, so they enter where its handler does.
    auto send_key = [&](bool down, unsigned keycode, unsigned state) {
        XEvent event{};
        event.xkey.type = down ? KeyPress : KeyRelease;
        event.xkey.display = display;
        event.xkey.window = window;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.keycode = keycode;
        event.xkey.state = state;
        event.xkey.same_screen = True;
        native::detail::dispatch_x_keyboard(surface, event);
    };
#else
    auto send_key = [&](bool down, unsigned keycode, unsigned state) {
        XEvent event{};
        event.xkey.type = down ? KeyPress : KeyRelease;
        event.xkey.display = display;
        event.xkey.window = window;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.keycode = keycode;
        event.xkey.state = state;
        event.xkey.same_screen = True;
        dispatch(event);
    };
#endif
    auto key = [&](bool down) {
        send_key(down, XKeysymToKeycode(display, XK_a), 0);
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
#if defined(MENU_TEST_OPENLOOK)
    (void)blur;
    (void)dispatch;
    consume_keys = false;
#else
    key(true);
    key(true);
    key(false);
    key(true);
    blur();
    key(false);
#endif
#if !defined(MENU_TEST_OPENLOOK)
    if (presses != 3 || repeats != 1 || releases != 1 || resets != 1)
        throw std::runtime_error("Canvas backend key counts: " +
            std::to_string(presses) + "/" + std::to_string(repeats) + "/" +
            std::to_string(releases) + "/" + std::to_string(resets));
#endif
    if (!typed.empty())
        throw std::runtime_error("A consumed key must not also type");
#if defined(__APPLE__)
    consume_keys = false;
    surface.focus();
    if ([[view window] firstResponder] != view)
        throw std::runtime_error("Canvas native focus missing");
    key(true);
    key(false);
    if (typed != "a") throw std::runtime_error("AppKit canvas text missing");
#else
    // Unconsumed presses compose to text: plain, shifted, and a character
    // outside Latin-1, which proves the path is UTF-8.
    consume_keys = false;
    surface.on_native_focus(true);
    const unsigned letter = XKeysymToKeycode(display, XK_a);
    send_key(true, letter, 0);
    send_key(false, letter, 0);
    send_key(true, letter, ShiftMask);
    send_key(false, letter, ShiftMask);
    if (typed != "aA")
        throw std::runtime_error("Canvas text: '" + typed + "'");
    int first = 0, last = 0, per_key = 0;
    XDisplayKeycodes(display, &first, &last);
    KeySym *map = XGetKeyboardMapping(display, last, 1, &per_key);
    const KeySym saved = map[0];
    XFree(map);
    KeySym zcaron = XK_zcaron;
    XChangeKeyboardMapping(display, last, 1, &zcaron, 1);
    XSync(display, False);
    XMappingEvent changed{};
    changed.type = MappingNotify;
    changed.display = display;
    changed.request = MappingKeyboard;
    changed.first_keycode = last;
    changed.count = 1;
    XRefreshKeyboardMapping(&changed);
    typed.clear();
    send_key(true, static_cast<unsigned>(last), 0);
    send_key(false, static_cast<unsigned>(last), 0);
    KeySym restore = saved;
    XChangeKeyboardMapping(display, last, 1, &restore, 1);
    XSync(display, False);
    if (typed != "\xc5\xbe")
        throw std::runtime_error("Canvas UTF-8 text: '" + typed + "'");
    send_key(true, letter, ControlMask);
    send_key(false, letter, ControlMask);
    if (typed != "\xc5\xbe")
        throw std::runtime_error("A Ctrl chord must not type");
#endif
    surface.destroy();
#else
    (void)parent;
#endif
}
