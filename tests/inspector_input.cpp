//
// Supplies native hit-tested pointer and keyboard input to the inspector
// runtime fixture. Toolkit types remain private to test infrastructure.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "inspector_input.h"

#include <stdexcept>

#include "../lib/native/post_backend.h"

#if defined(_WIN32)
#include "../lib/native/platforms/windows/globals.h"
#elif defined(__HAIKU__)
#include "../lib/native/platforms/haiku/globals.h"
#include <Message.h>
#include <View.h>
#include <Window.h>
#include <Application.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Bitmap.h>
#include <Screen.h>
#elif defined(INSPECTOR_SDL2)
#include "../lib/native/toolkits/sdl2/globals.h"
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#if defined(INSPECTOR_X11)
#include "../lib/native/toolkits/x11/globals.h"
#elif defined(INSPECTOR_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#elif defined(INSPECTOR_OPENLOOK)
#include "../lib/native/toolkits/openlook/globals.h"
#elif defined(INSPECTOR_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#endif
#endif

namespace inspector_input
{
#if !defined(_WIN32) && !defined(__HAIKU__) && !defined(INSPECTOR_SDL2)
    std::pair<Display *, Window> drawable(native::app_wnd &owner);
#endif
    // Read the presented pixels, including native editor children.
    std::unique_ptr<native::img> capture(native::app_wnd &owner) {
        const auto dimensions = owner.get_dimensions();
        auto result = std::make_unique<native::img>(dimensions.w, dimensions.h);
#if defined(_WIN32)
        HWND window = windows::wnd_bindings.handle_from_object(&owner);
        POINT origin{};
        ClientToScreen(window, &origin);
        HDC screen = GetDC(nullptr), buffer = CreateCompatibleDC(screen);
        BITMAPINFO format{};
        format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        format.bmiHeader.biWidth = dimensions.w;
        format.bmiHeader.biHeight = -int(dimensions.h);
        format.bmiHeader.biPlanes = 1;
        format.bmiHeader.biBitCount = 32;
        void *bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(screen, &format, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(buffer, bitmap);
        BitBlt(buffer, 0, 0, dimensions.w, dimensions.h, screen, origin.x, origin.y, SRCCOPY);
        GdiFlush();
        const auto *bytes = static_cast<const unsigned char *>(bits);
        for (std::size_t i = 0; i < std::size_t(dimensions.w) * dimensions.h; ++i)
            result->pixels()[i] = native::rgba(bytes[i * 4 + 2], bytes[i * 4 + 1], bytes[i * 4], 255);
        SelectObject(buffer, old); DeleteObject(bitmap); DeleteDC(buffer); ReleaseDC(nullptr, screen);
#elif defined(__HAIKU__)
        auto *window = haiku::wnd_bindings.handle_from_object(&owner);
        if (!window->Lock()) throw std::runtime_error("lock inspector capture");
        BPoint origin;
        haiku::content_view(window)->ConvertToScreen(&origin);
        window->Sync();
        window->Unlock();
        BRect area(origin.x, origin.y, origin.x + dimensions.w - 1, origin.y + dimensions.h - 1);
        BBitmap bitmap(BRect(0, 0, dimensions.w - 1, dimensions.h - 1), B_RGB32);
        BScreen screen;
        if (screen.ReadBitmap(&bitmap, false, &area) != B_OK)
            throw std::runtime_error("read inspector screen");
        const auto *bytes = static_cast<const unsigned char *>(bitmap.Bits());
        for (int y = 0; y < dimensions.h; ++y)
            for (int x = 0; x < dimensions.w; ++x) {
                const auto *pixel = bytes + y * bitmap.BytesPerRow() + x * 4;
                result->pixels()[y * dimensions.w + x] = native::rgba(pixel[2], pixel[1], pixel[0], 255);
            }
#elif defined(INSPECTOR_SDL2)
        auto *state = linux::sdl2::wnd_gpx_bindings.object_from_handle(&owner);
        if (SDL_RenderReadPixels(state->renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                result->pixels(), dimensions.w * sizeof(native::rgba)) != 0)
            throw std::runtime_error(SDL_GetError());
#else
        const auto [display, window] = drawable(owner);
        XSync(display, False);
        int x = 0, y = 0;
        Window child = None;
        const auto root = DefaultRootWindow(display);
        XTranslateCoordinates(display, window, root, 0, 0, &x, &y, &child);
        XImage *image = XGetImage(display, root, x, y, dimensions.w, dimensions.h, AllPlanes, ZPixmap);
        if (!image) throw std::runtime_error("read inspector drawable");
        for (int y = 0; y < dimensions.h; ++y)
            for (int x = 0; x < dimensions.w; ++x) {
                const auto pixel = XGetPixel(image, x, y);
                result->pixels()[y * dimensions.w + x] = native::rgba(pixel >> 16, pixel >> 8, pixel, 255);
            }
        XDestroyImage(image);
#endif
        return result;
    }

    // Older loops drain app::post only at startup. Supply a test-only UI
    // heartbeat so worker assertions do not depend on that separate feature.
    void heartbeat() {
        native::detail::drain_posted_work();
        if (!native::app::main_wnd()->get_created()) {
#if defined(__HAIKU__)
            be_app->PostMessage(B_QUIT_REQUESTED);
#endif
            return;
        }
#if defined(INSPECTOR_MOTIF)
        auto context = linux::openmotif::app_instance;
        XtAppAddTimeOut(context, 10, [](XtPointer, XtIntervalId *) { heartbeat(); }, nullptr);
#elif defined(INSPECTOR_WMAKER)
        WMAddTimerHandler(10, [](void *) { heartbeat(); }, nullptr);
#endif
    }

    // Install the heartbeat on the GUI thread before worker checks begin.
    void start() {
#if !defined(_WIN32) && !defined(__HAIKU__) && !defined(INSPECTOR_X11)
        heartbeat();
#endif
    }

#if !defined(_WIN32) && !defined(__HAIKU__) && !defined(INSPECTOR_SDL2)
    // Resolve the actual client drawable, including each toolkit's menu inset.
    std::pair<Display *, Window> drawable(native::app_wnd &owner) {
#if defined(INSPECTOR_X11)
        auto widget = linux::x11::wnd_bindings.handle_from_object(&owner);
        return {XtDisplay(widget), XtWindow(widget)};
#elif defined(INSPECTOR_MOTIF)
        auto widget = linux::openmotif::wnd_bindings.handle_from_object(&owner);
        return {XtDisplay(widget), XtWindow(widget)};
#elif defined(INSPECTOR_OPENLOOK)
        return {linux::openlook::cached_display, linux::openlook::drawable(&owner)};
#else
        auto widget = linux::wmaker::wnd_bindings.handle_from_object(&owner);
        return {linux::wmaker::display, WMWidgetXID(widget)};
#endif
    }

    // Resolve XTest at runtime as in the existing Athena input regressions.
    void *xtest(const char *name) {
        static void *library = dlopen("libXtst.so.6", RTLD_NOW);
        void *function = library ? dlsym(library, name) : nullptr;
        if (!function) throw std::runtime_error("XTest is required for inspector input tests");
        return function;
    }
#endif

    // Hit-test through the native hierarchy before delivering the pointer.
    void move(native::app_wnd &owner, native::point point) {
#if !defined(_WIN32) && !defined(__HAIKU__) && !defined(INSPECTOR_SDL2)
        const auto [display, window] = drawable(owner);
        XWarpPointer(display, None, window, 0, 0, 0, 0, point.x, point.y);
        XFlush(display);
#else
        (void)owner; (void)point;
#endif
    }

    // Hit-test through the native hierarchy before delivering the pointer.
    void pointer(native::app_wnd &owner, native::point point, bool pressed) {
#if defined(_WIN32)
        HWND window = windows::wnd_bindings.handle_from_object(&owner);
        POINT screen{point.x, point.y};
        ClientToScreen(window, &screen);
        SetCursorPos(screen.x, screen.y);
        HWND target = GetCapture();
        if (!target) target = WindowFromPoint(screen);
        POINT local = screen;
        ScreenToClient(target, &local);
        SendMessageW(target, pressed ? WM_LBUTTONDOWN : WM_LBUTTONUP,
            pressed ? MK_LBUTTON : 0, MAKELPARAM(local.x, local.y));
#elif defined(__HAIKU__)
        BWindow *window = haiku::wnd_bindings.handle_from_object(&owner);
        if (!window->Lock()) throw std::runtime_error("lock inspector window");
        BView *content = haiku::content_view(window);
        BPoint where(point.x, point.y);
        content->ConvertToScreen(&where);
        window->ConvertFromScreen(&where);
        BView *target = window->FindView(where);
        window->ConvertToScreen(&where);
        target->ConvertFromScreen(&where);
        BMessage event(pressed ? B_MOUSE_DOWN : B_MOUSE_UP);
        event.AddPoint("where", where);
        event.AddPoint("be:view_where", where);
        event.AddInt32("buttons", pressed ? B_PRIMARY_MOUSE_BUTTON : 0);
        event.AddInt32("clicks", 1);
        window->PostMessage(&event, target);
        window->Unlock();
#elif defined(INSPECTOR_SDL2)
        SDL_Event event{};
        event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.windowID = SDL_GetWindowID(linux::sdl2::wnd_bindings.handle_from_object(&owner));
        event.button.button = SDL_BUTTON_LEFT;
        event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
        event.button.x = point.x; event.button.y = point.y;
        SDL_PushEvent(&event);
#else
        const auto [display, window] = drawable(owner);
        XWarpPointer(display, None, window, 0, 0, 0, 0, point.x, point.y);
        using function = int (*)(Display *, unsigned int, Bool, unsigned long);
        reinterpret_cast<function>(xtest("XTestFakeButtonEvent"))(display, Button1, pressed, 0);
        XFlush(display);
#endif
    }

    // Type through the focused native control, never through the property model.
    void type(native::app_wnd &owner, char character) {
#if defined(_WIN32)
        (void)owner;
        SendMessageW(GetFocus(), WM_CHAR, character, 1);
#elif defined(__HAIKU__)
        BWindow *window = haiku::wnd_bindings.handle_from_object(&owner);
        if (!window->Lock()) throw std::runtime_error("lock inspector focus");
        BMessage event(B_KEY_DOWN);
        const char bytes[] = {character, 0};
        event.AddString("bytes", bytes);
        event.AddInt32("modifiers", 0);
        // Let BWindow choose the focused view, as it does for server input.
        BMessenger(nullptr, window).SendMessage(&event);
        window->Unlock();
#elif defined(INSPECTOR_SDL2)
        SDL_Event event{};
        event.type = SDL_TEXTINPUT;
        event.text.windowID = SDL_GetWindowID(linux::sdl2::wnd_bindings.handle_from_object(&owner));
        event.text.text[0] = character;
        SDL_PushEvent(&event);
#else
        const auto [display, window] = drawable(owner);
        (void)window;
        const char text[] = {character, 0};
        const unsigned key = XKeysymToKeycode(display, XStringToKeysym(text));
        using function = int (*)(Display *, unsigned int, Bool, unsigned long);
        auto send = reinterpret_cast<function>(xtest("XTestFakeKeyEvent"));
        send(display, key, True, 0); send(display, key, False, 0);
        XFlush(display);
#endif
    }
}
