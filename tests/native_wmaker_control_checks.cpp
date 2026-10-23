//
// Exercises initial WINGs editor focus, actual X cursor pixels, native
// settings list sections/icons and symmetric search/navigation
// geometry.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/wmaker/globals.h"
#include "../lib/native/combo_picker.h"
#include "../lib/native/post_backend.h"
#include <WINGs/WINGsP.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/extensions/Xfixes.h>
#include <dlfcn.h>
#include <chrono>
#include <thread>
#include <vector>
#include <stdexcept>
#include <cstdlib>

namespace
{
    namespace wm = linux::wmaker;
    using namespace native;
    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    void pump() {
        for (int i = 0; i < 25; ++i) {
            XSync(wm::display, False);
            while (XPending(wm::display)) {
                XEvent event{};
                XNextEvent(wm::display, &event);
                WMHandleEvent(&event);
            }
            wm::dispatch_deferred();
            detail::drain_posted_work();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    // Compare rendered glyph pixels, not just their drawing
    // coordinates.
    void expect_matching_time_arrows(combo_box &field) {
        auto *state = wm::combo_box_bindings.object_from_handle(&field);
        expect(state && state->arrow_overlay,
               "time picker has its joined arrow overlay");
        const int width = WMWidgetWidth(state->arrow);
        const int height = WMWidgetHeight(state->arrow);
        auto *pixels = XGetImage(wm::display,
                                 WMWidgetXID(state->arrow),
                                 0,
                                 0,
                                 width,
                                 height,
                                 AllPlanes,
                                 ZPixmap);
        expect(pixels, "read the actual time arrow rendering");
        const auto *screen = reinterpret_cast<W_Screen *>(wm::screen);
        const auto ink = WMColorPixel(screen->black);
        bool matching = true;
        unsigned ink_pixels = 0;
        for (int y = 2; y < height / 2 - 2; ++y) {
            for (int x = 2; x < width - 2; ++x) {
                const bool up = XGetPixel(pixels, x, y) == ink;
                const bool down =
                    XGetPixel(pixels, x, height - 1 - y) == ink;
                matching = matching && up == down;
                ink_pixels += up;
            }
        }
        XDestroyImage(pixels);
        expect(ink_pixels > 0 && matching,
               "time arrows have equal visible size and mirrored "
               "placement at even and odd control heights");
    }
    void attach(wnd &child, wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
    void key(KeySym symbol) {
        static auto *library = dlopen("libXtst.so.6", RTLD_NOW);
        using proc = int (*)(Display *, unsigned, Bool, unsigned long);
        auto send =
            reinterpret_cast<proc>(dlsym(library, "XTestFakeKeyEvent"));
        expect(send, "XTest provides native keyboard input");
        const auto code = XKeysymToKeycode(wm::display, symbol);
        send(wm::display, code, True, 0);
        send(wm::display, code, False, 0);
        pump();
    }
    WMWidget *find(W_View *view, W_Class type) {
        if (view->self && WMWidgetClass(view->self) == type)
            return view->self;
        for (auto *child = view->childrenList; child;
             child = child->nextSister)
            if (auto *widget = find(child, type))
                return widget;
        return nullptr;
    }
    std::vector<unsigned long> cursor_pixels() {
        static auto *library = dlopen("libXfixes.so.3", RTLD_NOW);
        using proc = XFixesCursorImage *(*)(Display *);
        auto capture = reinterpret_cast<proc>(
            dlsym(library, "XFixesGetCursorImage"));
        expect(capture, "XFixes provides actual cursor pixels");
        auto *image = capture(wm::display);
        expect(image, "read current cursor");
        std::vector<unsigned long> pixels(
            image->pixels,
            image->pixels + image->width * image->height);
        XFree(image);
        return pixels;
    }
    void snapshot(wnd &owner) {
        const char *path =
            std::getenv("NATIVE_WMAKER_CONTROL_SNAPSHOT");
        if (!path)
            return;
        auto target = wm::drawable(&owner);
        XWindowAttributes bounds{};
        XGetWindowAttributes(wm::display, target, &bounds);
        int x = 0, y = 0;
        Window child = None;
        const auto root = DefaultRootWindow(wm::display);
        XTranslateCoordinates(
            wm::display, target, root, 0, 0, &x, &y, &child);
        auto *pixels = XGetImage(wm::display,
                                 root,
                                 x,
                                 y,
                                 bounds.width,
                                 bounds.height,
                                 AllPlanes,
                                 ZPixmap);
        expect(pixels, "read Window Maker control screenshot");
        img image(bounds.width, bounds.height);
        for (int row = 0; row < bounds.height; ++row)
            for (int column = 0; column < bounds.width; ++column) {
                const auto pixel = XGetPixel(pixels, column, row);
                image.pixels()[row * bounds.width + column] =
                    rgba(pixel >> 16, pixel >> 8, pixel, 255);
            }
        XDestroyImage(pixels);
        image.save(path);
    }
} // namespace

// Check controls created before their owning window first maps.
void check_wmaker_controls(native::app_wnd &root) {
    using namespace native;
    modeless_wnd window(
        root, "Window Maker follow-up", 30, 30, 680, 500);
    settings_sidebar sidebar(10, 10, 340, 470);
    panel general, dates_page;
    auto icon = std::make_shared<img>(16, 16);
    icon->get_gpx().clear(rgba(210, 20, 30, 255));
    sidebar.set_sidebar_width(190).set_search_visible(true);
    sidebar << settings_page(1, "General", general, icon, "Settings")
            << settings_page(2, "Dates", dates_page, {}, "Controls");
    code_edit code("abc", 370, 10, 290, 180);
    text_edit single(
        "Single", text_edit_mode::single_line, 370, 220, 230, 24);
    text_edit multi(
        "Multiline", text_edit_mode::multi_line, 370, 260, 230, 80);
    combo_box combo(
        {"One", "Two"}, combo_box_style::editable, 370, 365, 230, 24);
    date_picker dates(date{2026, 12, 24}, rect(370, 410, 290, 24));
    time_picker times(time_of_day{9, 30, 0}, rect(370, 455, 290, 24));
    window.on_wnd_create.connect([&] {
        for (wnd *child : std::vector<wnd *>{&sidebar,
                                             &code,
                                             &single,
                                             &multi,
                                             &combo,
                                             &dates,
                                             &times})
            attach(*child, window);
        return true;
    });
    window.create();
    window.show();
    pump();
    const auto target = wm::drawable(&window);
    XWindowChanges stacking{};
    stacking.stack_mode = Above;
    XReconfigureWMWindow(wm::display,
                         target,
                         DefaultScreen(wm::display),
                         CWStackMode,
                         &stacking);
    Window frame = target;
    for (;;) {
        Window desktop = None, parent = None, *children = nullptr;
        unsigned count = 0;
        XQueryTree(
            wm::display, frame, &desktop, &parent, &children, &count);
        if (children)
            XFree(children);
        if (!parent || parent == desktop)
            break;
        frame = parent;
    }
    XRaiseWindow(wm::display, frame);
    XSetInputFocus(wm::display, target, RevertToParent, CurrentTime);
    pump();
    expect(
        code.get_focused(),
        "initial native keyboard focus also paints the editor caret");
    key(XK_z);
    expect(code.get_text().find('z') != std::string::npos,
           "code editor receives keyboard input on its first opening "
           "without completion");
    auto *host =
        WMWidgetView(wm::wnd_bindings.handle_from_object(&sidebar));
    auto *list = static_cast<WMList *>(find(host, WC_List));
    auto *search = static_cast<WMTextField *>(find(host, WC_TextField));
    expect(list && search,
           "settings uses native WINGs list and search widgets");
    expect(WMGetListNumberOfRows(list) == 4 &&
               WMGetListItem(list, 0)->disabled &&
               WMGetListItem(list, 2)->disabled &&
               !WMGetListItem(list, 1)->isBranch,
           "settings sections are fixed inactive rows without collapse "
           "affordances");
    const auto nav = WMGetViewPosition(WMWidgetView(list));
    const auto nav_size = WMGetViewSize(WMWidgetView(list));
    const auto field = WMGetViewPosition(WMWidgetView(search));
    const auto field_size = WMGetViewSize(WMWidgetView(search));
    expect(nav.x == 6 && field.y == 6 &&
               nav.y - field.y - int(field_size.height) == 6 &&
               sidebar.get_dimensions().h - nav.y -
                       int(nav_size.height) ==
                   6,
           "native settings search gap equals all six-pixel outer "
           "margins");
    auto *pixels = XGetImage(wm::display,
                             WMWidgetXID(list),
                             0,
                             0,
                             nav_size.width,
                             WMGetListItemHeight(list) * 2,
                             AllPlanes,
                             ZPixmap);
    int black = 0, red = 0;
    for (int y = 3; y < WMGetListItemHeight(list) * 2; ++y)
        for (int x = 3; x < 100; ++x) {
            const auto pixel = XGetPixel(pixels, x, y);
            black += pixel == 0;
            red += (pixel & 0xffffff) == 0xd2141e;
        }
    XDestroyImage(pixels);
    expect(
        black > 4 && red > 20,
        "native settings rows show dark headings and optional icons");
    // Compare the server's current cursor with an explicit X I-beam.
    XWarpPointer(wm::display, None, target, 0, 0, 0, 0, 650, 490);
    pump();
    const auto beam = XCreateFontCursor(wm::display, XC_xterm);
    XDefineCursor(wm::display, target, beam);
    XSync(wm::display, False);
    const auto expected_cursor = cursor_pixels();
    XDefineCursor(
        wm::display,
        target,
        WMWidgetView(wm::wnd_bindings.handle_from_object(&window))
            ->cursor);
    XFreeCursor(wm::display, beam);
    snapshot(window);
    for (auto *edit : {&single, &multi}) {
        XWarpPointer(
            wm::display, None, wm::drawable(edit), 0, 0, 0, 0, 70, 10);
        pump();
        expect(cursor_pixels() == expected_cursor,
               "native edit controls display the actual I-beam cursor");
    }
    auto *combo_state =
        wm::combo_box_bindings.object_from_handle(&combo);
    XWarpPointer(wm::display,
                 None,
                 WMWidgetXID(combo_state->field),
                 0,
                 0,
                 0,
                 0,
                 70,
                 10);
    pump();
    expect(cursor_pixels() == expected_cursor,
           "editable combo text displays an I-beam");
    auto &date_field = detail::picker_field(dates);
    auto &time_field = detail::picker_field(times);
    const auto short_width = time_field.get_dimensions().w;
    expect(short_width < 140,
           "time field fits its text and native arrow column");
    times.set_seconds_visible(true);
    expect(
        time_field.get_dimensions().w > short_width &&
            time_field.get_dimensions().w < 160,
        "seconds expand the compact native time field only as needed");
    for (const auto height : {24, 25, 26, 27, 30, 31}) {
        times.set_dimensions(size(290, height));
        pump();
        expect_matching_time_arrows(time_field);
    }
    times.set_dimensions(size(290, 24));
    pump();
    expect(date_field.get_dimensions().w < 180,
           "date field fits its text instead of filling an oversized "
           "allocation");
    auto *actions =
        detail::peer_state<detail::combo_picker_actions>(date_field);
    actions->open();
    pump();
    auto *calendar = detail::picker_calendar(dates);
    expect(calendar && calendar->get_created() &&
               calendar->get_dimensions().w >=
                   date_field.get_dimensions().w,
           "calendar is at least as wide as its visible compact date "
           "field");
    auto *cache = wm::graphics_bindings.object_from_handle(calendar);
    auto *edge = XGetImage(
        wm::display, cache->backbuffer, 0, 0, 4, 4, AllPlanes, ZPixmap);
    expect(XGetPixel(edge, 0, 0) != XGetPixel(edge, 3, 3),
           "calendar paints a visible enclosing border");
    XDestroyImage(edge);
    calendar->destroy();
    pump();
    snapshot(window);
    window.destroy();
    pump();
}
