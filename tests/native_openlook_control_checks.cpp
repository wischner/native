//
// Checks server-delivered OPEN LOOK input, native picker composition,
// navigation metadata and background damage without clearing editors.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/openlook/globals.h"
#include "../lib/native/post_backend.h"
#include "../lib/native/code_render.h"
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <xview/svrimage.h>
#include <xview/font.h>
#include <xview/textsw.h>
#define delete xview_delete_handler
#include <xview_private/panel_impl.h>
#undef delete
#include <dlfcn.h>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <iostream>
#include <array>
#include <cstdlib>
extern "C" Notify_value xv_input_pending(Display *, int);

namespace
{
    using namespace native;
    namespace ol = linux::openlook;
    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    void pump() {
        XSync(ol::cached_display, False);
        for (int i = 0; i < 25; ++i) {
            xv_input_pending(ol::cached_display,
                             ConnectionNumber(ol::cached_display));
            notify_dispatch();
            detail::drain_posted_work();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    class input
    {
        void *library = dlopen("libXtst.so.6", RTLD_NOW);
        using button_proc = int (*)(Display *,
                                    unsigned,
                                    Bool,
                                    unsigned long);
        using key_proc = int (*)(Display *,
                                 unsigned,
                                 Bool,
                                 unsigned long);

    public:
        input() {
            expect(library,
                   "XTest is available for native Open Look input");
        }
        ~input() = default;
        void key(KeySym symbol, bool down) {
            auto send = reinterpret_cast<key_proc>(
                dlsym(library, "XTestFakeKeyEvent"));
            send(ol::cached_display,
                 XKeysymToKeycode(ol::cached_display, symbol),
                 down,
                 0);
        }
        void key(KeySym symbol) {
            key(symbol, true);
            key(symbol, false);
            pump();
        }
        void click(Window target, int x, int y) {
            XWarpPointer(
                ol::cached_display, None, target, 0, 0, 0, 0, x, y);
            auto send = reinterpret_cast<button_proc>(
                dlsym(library, "XTestFakeButtonEvent"));
            pump();
            send(ol::cached_display, Button1, True, 0);
            pump();
            send(ol::cached_display, Button1, False, 0);
            pump();
        }
        void click(Panel_item item) {
            const auto panel = xv_get(item, XV_OWNER);
            const auto *bounds =
                reinterpret_cast<Rect *>(xv_get(item, XV_RECT));
            click(xv_get(panel, XV_XID),
                  bounds->r_left + bounds->r_width / 2,
                  bounds->r_top + bounds->r_height / 2);
        }
    };
    void attach(wnd &child, wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
    Panel_item text(panel &owner) {
        auto native = ol::wnd_bindings.handle_from_object(&owner);
        return xv_get(native, PANEL_FIRST_ITEM);
    }
    combo_box *field(panel &owner) {
        return reinterpret_cast<combo_box *>(
            xv_get(text(owner), PANEL_CLIENT_DATA));
    }
    std::string value(Panel_item item) {
        const char *value =
            reinterpret_cast<const char *>(xv_get(item, PANEL_VALUE));
        return value ? value : "";
    }
    void edit(Panel_item item, const std::string &value) {
        xv_set(item, PANEL_VALUE, value.c_str(), nullptr);
        auto notify =
            reinterpret_cast<Panel_setting (*)(Panel_item, Event *)>(
                xv_get(item, PANEL_NOTIFY_PROC));
        Event event{};
        event_set_id(&event, '\r');
        notify(item, &event);
        pump();
    }
    unsigned long pixel(Window window, int x, int y) {
        auto *image = XGetImage(
            ol::cached_display, window, x, y, 1, 1, AllPlanes, ZPixmap);
        const auto value = XGetPixel(image, 0, 0);
        XDestroyImage(image);
        return value;
    }
    void snapshot(Window target, const char *path) {
        if (!path)
            return;
        XWindowAttributes bounds{};

        XGetWindowAttributes(ol::cached_display, target, &bounds);
        int root_x = 0, root_y = 0;
        Window child = None;
        const auto root = DefaultRootWindow(ol::cached_display);
        XTranslateCoordinates(ol::cached_display,
                              target,
                              root,
                              0,
                              0,
                              &root_x,
                              &root_y,
                              &child);
        auto *image = XGetImage(ol::cached_display,
                                root,
                                root_x,
                                root_y,
                                bounds.width,
                                bounds.height,
                                AllPlanes,
                                ZPixmap);
        img capture(bounds.width, bounds.height);
        for (int y = 0; y < bounds.height; ++y)
            for (int x = 0; x < bounds.width; ++x) {
                const auto color = XGetPixel(image, x, y);
                capture.pixels()[y * bounds.width + x] =
                    rgba((color >> 16) & 255,
                         (color >> 8) & 255,
                         color & 255,
                         255);
            }
        XDestroyImage(image);
        capture.save(path);
    }
    class model final : public table_model
    {
    public:
        std::size_t row_count() const override {
            return 10;
        }
        table_row_id row_id(std::size_t row) const override {
            return row + 1;
        }
        table_cell cell(std::size_t row,
                        table_column_id) const override {
            return {std::to_string(row + 1), nullptr};
        }
    };
} // namespace
void check_openlook_controls(native::app_wnd &owner) {
    using namespace native;
    namespace ol = linux::openlook;
    model data;
    modeless_wnd window(
        owner, "OPEN LOOK control input", 100, 100, 780, 640);
    table_view table(10, 10, 330, 160);
    code_edit code("abc", 10, 205, 330, 160);
    combo_box combo({"10 mm", "25 mm", "50 mm"},
                    combo_box_style::editable,
                    370,
                    10,
                    240,
                    24);
    date_picker dates(date{2026, 12, 24},
                      native::rect(370, 70, 240, 24));
    time_picker times(time_of_day{9, 30, 0},
                      native::rect(370, 120, 240, 24));
    panel picker_host(360, 60, 360, 120);
    text_edit single("Font sample 0123",
                     text_edit_mode::single_line,
                     10,
                     390,
                     330,
                     24);
    text_edit multi("Font sample 0123",
                    text_edit_mode::multi_line,
                    10,
                    430,
                    330,
                    70);
    dates.set_bounds(native::rect(10, 10, 240, 24));
    times.set_bounds(native::rect(10, 60, 240, 24));
    picker_host.on_wnd_create.connect([&] {
        attach(dates, picker_host);
        attach(times, picker_host);
        return true;
    });
    panel page;
    settings_sidebar sidebar(370, 200, 390, 390);
    sidebar.set_sidebar_width(200).set_search_visible(true);
    sidebar << settings_page(1, "General", page, nullptr, "Settings");
    table.set_model(&data)
        .set_columns({table_column{1, "Value", 260}})
        .set_header_visible(false)
        .set_row_height(24)
        .set_selection_mode(table_selection_mode::multiple);
    dates.set_disabled_dates({date{2026, 12, 25}});
    window.create();
    window.show();
    for (wnd *child : std::array<wnd *, 7>{&table,
                                           &code,
                                           &combo,
                                           &picker_host,
                                           &sidebar,
                                           &single,
                                           &multi})
        attach(*child, window);
    pump();
    const auto single_item =
        ol::text_edit_bindings.object_from_handle(&single)->item;
    const auto multi_item =
        ol::text_edit_bindings.object_from_handle(&multi)->item;
    const auto single_font = xv_get(single_item, PANEL_VALUE_FONT);
    const auto multi_view =
        xv_get(multi_item, PANEL_ITEM_NTH_WINDOW, 0);
    expect(multi_view, "native multiline editor has a text view");
    const auto multi_font =
        xv_get(xv_get(multi_view, WIN_PARENT), WIN_FONT);
    const auto *single_metrics =
        reinterpret_cast<XFontStruct *>(xv_get(single_font, FONT_INFO));
    const auto *multi_metrics =
        reinterpret_cast<XFontStruct *>(xv_get(multi_font, FONT_INFO));
    expect(
        single_metrics && multi_metrics &&
            single_metrics->fid == multi_metrics->fid,
        "single and multiline editors use the same actual native font");
    input server;
    auto *table_peer =
        ol::table_view_bindings.object_from_handle(&table);
    const auto table_window =
        Window(xv_get(table_peer->paint_window, XV_XID));
    server.click(table_window, 30, 12);
    expect(table.get_selected_rows().size() == 1,
           "OPEN LOOK physical table click selects a row");
    server.key(XK_Control_L, true);
    server.click(table_window, 30, 36);
    server.key(XK_Control_L, false);
    pump();
    expect(table.get_selected_rows().size() == 2,
           "OPEN LOOK physical Ctrl-click adds selection");
    server.key(XK_Shift_L, true);
    server.click(table_window, 30, 84);
    server.key(XK_Shift_L, false);
    pump();
    expect(table.get_selected_rows().size() >= 3,
           "OPEN LOOK physical Shift-click extends selection");
    auto *code_peer = ol::code_edit_bindings.object_from_handle(&code);
    server.click(xv_get(code_peer->paint_window, XV_XID), 100, 12);
    const auto before = code.get_caret_offset();
    server.key(XK_Left);
    expect(code.get_caret_offset() < before,
           "OPEN LOOK physical Left moves code caret");
    std::string accepted;
    code.on_complete.connect([&](completion_item item) {
        accepted = item.label;
        return true;
    });
    code.show_completion({{"One", "1", ""}, {"Two", "2", ""}});
    pump();
    server.key(XK_Down);
    server.key(XK_Tab);
    expect(accepted == "Two",
           "OPEN LOOK physical arrows and Tab accept completion");
    accepted.clear();
    code.show_completion({{"One", "1", ""}, {"Two", "2", ""}});
    pump();
    const auto *completion =
        detail::peer_state<detail::code_completion_geometry>(code);
    expect(completion,
           "OPEN LOOK completion records its painted hit geometry");
    server.click(xv_get(code_peer->paint_window, XV_XID),
                 completion->bounds.x1() + 10,
                 completion->bounds.y1() + completion->row_height + 4);
    expect(accepted == "Two",
           "OPEN LOOK mouse accepts the painted completion row");
    code.show_completion({{"One", "1", ""}});
    pump();
    server.key(XK_Escape);
    expect(!code.get_completion_visible(),
           "OPEN LOOK physical Escape dismisses completion");
    auto *combo_peer =
        ol::combo_box_bindings.object_from_handle(&combo);
    xv_set(combo_peer->text, PANEL_VALUE, "", nullptr);
    const auto combo_panel = xv_get(combo_peer->text, XV_OWNER);
    xv_set(combo_panel,
           PANEL_CARET_ITEM,
           combo_peer->text,
           WIN_SET_FOCUS,
           nullptr);
    pump();
    for (const auto key : {XK_5, XK_0, XK_space, XK_m, XK_m})
        server.key(key);
    expect(combo.get_text() == "50 mm",
           "OPEN LOOK native typing reaches the combo model after "
           "editing");
    expect(combo.get_selected_index() == 2 &&
               xv_get(xv_get(combo_peer->menu, MENU_NTH_ITEM, 3),
                      MENU_SELECTED) &&
               xv_get(combo_peer->menu, MENU_DEFAULT) == 3,
           "OPEN LOOK typed combo match synchronizes native popup "
           "selection");
    edit(text(dates), "25.12.2026");
    expect(dates.get_date() == date{2026, 12, 24} &&
               value(text(dates)) == "24.12.2026",
           "OPEN LOOK dot date rejects excluded holiday and restores "
           "editor");
    edit(text(dates), "26.12.2026");
    expect(dates.get_date() == date{2026, 12, 26},
           "OPEN LOOK dot date commits");
    edit(text(times), "12:45");
    expect(times.get_time() == time_of_day{12, 45, 0},
           "OPEN LOOK joined colon time commits");
    auto *time_field = field(times);
    const auto time_text = text(times);
    auto *time_native = TEXT_PRIVATE(time_text);
    auto time_panel = xv_get(time_text, XV_OWNER);
    Panel_item hour_up = XV_NULL, minute_down = XV_NULL;
    for (auto item = xv_get(time_panel, PANEL_FIRST_ITEM); item;
         item = xv_get(item, PANEL_NEXT_ITEM)) {
        if (!xv_get(item, XV_IS_SUBTYPE_OF, PANEL_BUTTON))
            continue;
        if (!hour_up)
            hour_up = item;
        else
            minute_down = item;
    }
    expect(hour_up && minute_down,
           "native time field has both step buttons");
    time_native->caret_position = 0;
    time_native->caret_offset = 0;
    time_native->delete_pending = False;
    server.click(hour_up);
    server.click(hour_up);
    expect(times.get_time() == time_of_day{14, 45, 0} &&
               time_native->caret_position == 0,
           "repeated hour_up clicks retain and increment the selected "
           "hour");
    time_native->caret_position = 3;
    server.click(minute_down);
    expect(times.get_time() == time_of_day{14, 44, 0} &&
               time_native->caret_position == 3,
           ("time minute stepping preserves its native caret: " +
            times.get_time().to_string(true) + " caret " +
            std::to_string(time_native->caret_position))
               .c_str());
    for (auto *picker : {field(dates), time_field}) {
        const auto *peer =
            ol::combo_box_bindings.object_from_handle(picker);
        const auto native_font = xv_get(peer->text, PANEL_VALUE_FONT);
        auto *metrics = reinterpret_cast<XFontStruct *>(
            xv_get(native_font, FONT_INFO));
        const int padding = 2 * XTextWidth(metrics, "0", 1);
        const auto *area =
            reinterpret_cast<Rect *>(xv_get(peer->text, XV_RECT));
        expect(
            area->r_width <= XTextWidth(metrics,
                                        picker->get_text().c_str(),
                                        picker->get_text().size()) +
                                 padding + 2,
            "native picker text area fits text and character margins");
        expect(area->r_left == padding / 2,
               "native picker layout reserves a left character margin");
        expect(picker->get_dimensions().w < 180,
               "Open Look pickers no longer fill the page allocation");
    }
    times.set_time({12, 45, 0});
    auto *date_field = field(dates);
    auto *date_peer =
        ol::combo_box_bindings.object_from_handle(date_field);
    server.click(date_peer->choice);
    pump();
    Frame popup = XV_NULL;
    for (int i = 0; i < 8 && !popup; ++i) {
        const auto candidate = xv_get(
            ol::window_state(&window)->frame, FRAME_NTH_SUBFRAME, i);
        if (candidate && xv_get(candidate, XV_SHOW))
            popup = candidate;
    }
    expect(popup && xv_get(popup, XV_SHOW),
           "OPEN LOOK date arrow opens a native popup frame");
    const auto calendar = xv_get(popup, FRAME_CMD_PANEL);
    bool excluded = false;
    for (auto item = xv_get(calendar, PANEL_FIRST_ITEM); item;
         item = xv_get(item, PANEL_NEXT_ITEM)) {
        const char *label = reinterpret_cast<const char *>(
            xv_get(item, PANEL_LABEL_STRING));
        if (label && std::string(label) == "25")
            excluded = xv_get(item, PANEL_INACTIVE);
    }
    expect(excluded,
           "OPEN LOOK calendar uses native inactive holiday buttons");
    snapshot(xv_get(popup, XV_XID),
             std::getenv("NATIVE_OPENLOOK_CALENDAR_SNAPSHOT"));
    snapshot(DefaultRootWindow(ol::cached_display),
             std::getenv("NATIVE_OPENLOOK_DESKTOP_SNAPSHOT"));
    const auto previous = xv_get(calendar, PANEL_FIRST_ITEM);
    server.click(previous);
    expect(xv_get(popup, XV_SHOW),
           "OPEN LOOK month navigation keeps the calendar open");
    server.click(date_peer->choice);
    pump();
    expect(!xv_get(popup, XV_SHOW),
           "OPEN LOOK second arrow click closes the native calendar");
    server.click(date_peer->choice);
    Panel_item day = XV_NULL;
    for (auto item = xv_get(calendar, PANEL_FIRST_ITEM); item;
         item = xv_get(item, PANEL_NEXT_ITEM)) {
        const char *label = reinterpret_cast<const char *>(
            xv_get(item, PANEL_LABEL_STRING));
        if (label && std::string(label) == "24")
            day = item;
    }
    expect(day, "OPEN LOOK calendar exposes the requested day");
    server.click(day);
    expect(dates.get_date() == date{2026, 12, 24} &&
               !xv_get(popup, XV_SHOW),
           "OPEN LOOK calendar day click commits and closes");
    times.set_minute_step(15);
    const auto time_choice = xv_get(text(times), PANEL_NEXT_ITEM);
    const auto up = xv_get(time_choice, PANEL_NEXT_ITEM);
    const auto arrow = xv_get(up, PANEL_LABEL_IMAGE);
    auto *arrow_pixels = XGetImage(ol::cached_display,
                                   xv_get(arrow, XV_XID),
                                   0,
                                   0,
                                   9,
                                   2,
                                   AllPlanes,
                                   ZPixmap);
    int ink = 0;
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 9; ++x)
            ink += XGetPixel(arrow_pixels, x, y) != 0;
    XDestroyImage(arrow_pixels);
    expect(ink == 8, "OPEN LOOK time button arrow bitmap is visible");
    const auto down = xv_get(up, PANEL_NEXT_ITEM);
    const auto *up_bounds =
        reinterpret_cast<Rect *>(xv_get(up, XV_RECT));
    const auto *down_bounds =
        reinterpret_cast<Rect *>(xv_get(down, XV_RECT));
    expect(up_bounds->r_top + up_bounds->r_height <=
                   down_bounds->r_top &&
               down_bounds->r_top + down_bounds->r_height <=
                   times.get_client_bounds().h(),
           "OPEN LOOK paired time buttons fit without overlap or "
           "clipping");
    server.click(up);
    expect(times.get_time() == time_of_day{13, 0, 0},
           "OPEN LOOK paired time arrow honors minute steps");
    // A background update must not erase a live, windowless editor.
    auto *root = ol::window_state(&window);
    const auto *bounds =
        reinterpret_cast<Rect *>(xv_get(combo_peer->text, XV_RECT));
    GC gc = XCreateGC(
        ol::cached_display, xv_get(root->content, XV_XID), 0, nullptr);
    XSetForeground(ol::cached_display, gc, 0xff0000);
    const int x = bounds->r_left + 8, y = bounds->r_top + 8;
    XFillRectangle(ol::cached_display,
                   xv_get(root->content, XV_XID),
                   gc,
                   x,
                   y,
                   2,
                   2);
    XSync(ol::cached_display, False);
    const auto sentinel = pixel(xv_get(root->content, XV_XID), x, y);
    window.invalidate();
    pump();
    expect(
        pixel(xv_get(root->content, XV_XID), x, y) == sentinel,
        "OPEN LOOK background redraw preserves native editor pixels");
    XFreeGC(ol::cached_display, gc);
    window.invalidate(native::rect(1, 1, 3, 3));
    window.invalidate(native::rect(8, 8, 3, 3));
    expect(root->pending_damage.x1() == 1 &&
               root->pending_damage.y1() == 1 &&
               root->pending_damage.x2() == 11,
           "OPEN LOOK invalidations retain and merge their damage "
           "rectangles");
    pump();
    auto navigation = static_cast<Panel>(
        ol::wnd_bindings.handle_from_object(&sidebar));
    const auto search = xv_get(navigation, PANEL_FIRST_ITEM);
    const char *label = reinterpret_cast<const char *>(
        xv_get(search, PANEL_LABEL_STRING));
    expect(label && std::string(label) == "Search:",
           "OPEN LOOK native search has a visible prompt");
    xv_set(
        navigation, PANEL_CARET_ITEM, search, WIN_SET_FOCUS, nullptr);
    pump();
    server.key(XK_z);
    expect(sidebar.get_search_text() == "z",
           "OPEN LOOK native search typing updates navigation");
    server.key(XK_BackSpace);
    expect(sidebar.get_search_text().empty(),
           "OPEN LOOK native search deletion restores navigation");
    Panel_item navigation_list = XV_NULL;
    for (auto item = xv_get(navigation, PANEL_FIRST_ITEM); item;
         item = xv_get(item, PANEL_NEXT_ITEM))
        if (xv_get(item, XV_IS_SUBTYPE_OF, PANEL_LIST)) {
            navigation_list = item;
            break;
        }
    expect(navigation_list,
           "settings contains a native list after search filtering");
    const auto *search_bounds =
        reinterpret_cast<Rect *>(xv_get(search, XV_RECT));
    const auto *list_bounds =
        reinterpret_cast<Rect *>(xv_get(navigation_list, XV_RECT));
    expect(search_bounds->r_top == 6 && list_bounds->r_left == 6 &&
               list_bounds->r_top - search_bounds->r_top -
                       search_bounds->r_height ==
                   6 &&
               sidebar.get_dimensions().h - list_bounds->r_top -
                       list_bounds->r_height ==
                   6,
           ("native search/list margins: search y/h " +
            std::to_string(search_bounds->r_top) + "/" +
            std::to_string(search_bounds->r_height) + " list x/y/h " +
            std::to_string(list_bounds->r_left) + "/" +
            std::to_string(list_bounds->r_top) + "/" +
            std::to_string(list_bounds->r_height))
               .c_str());
    const auto navigation_scrollbar =
        xv_get(navigation_list, PANEL_LIST_SCROLLBAR);
    const int list_width = xv_get(navigation_list, PANEL_LIST_WIDTH);
    const int scrollbar_width = xv_get(navigation_scrollbar, XV_WIDTH);
    expect(list_width + scrollbar_width <=
               sidebar.get_sidebar_width() - 12,
           "OPEN LOOK navigation reserves its native scrollbar within "
           "both margins");
    sidebar.set_dimensions({390, 427});
    pump();
    const auto *larger_list =
        reinterpret_cast<Rect *>(xv_get(navigation_list, XV_RECT));
    expect(427 - larger_list->r_top - larger_list->r_height == 6,
           "native list keeps its bottom margin after a non-row-sized "
           "resize");
    sidebar.set_dimensions({390, 390});
    pump();
    const auto page_host = ol::wnd_bindings.handle_from_object(&page);
    expect(int(xv_get(page_host, XV_X)) ==
                   sidebar.get_position().x +
                       sidebar.get_content_bounds().x1() &&
               int(xv_get(page_host, XV_Y)) ==
                   sidebar.get_position().y +
                       sidebar.get_content_bounds().y1(),
           "OPEN LOOK filtered pages retain accumulated frame "
           "coordinates");
    snapshot(ol::drawable(&window),
             std::getenv("NATIVE_OPENLOOK_CONTROL_SNAPSHOT"));
    bool closed_from_commit = false;
    times.on_change.connect([&](time_of_day value) {
        closed_from_commit = value == time_of_day{7, 15, 0};
        window.destroy();
        return true;
    });
    field(times)->set_text("07:15");
    server.click(up);
    expect(closed_from_commit && !window.get_created(),
           "a typed-time commit may destroy its window before stepping continues");
    window.destroy();
    pump();
}
