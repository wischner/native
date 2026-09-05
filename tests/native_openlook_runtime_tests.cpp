//
// Checks XView choice state, native sizing, partial OLGX exposure,
// source-editor keyboard routing and dependent window teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "../lib/native/toolkits/openlook/globals.h"
#include "../lib/native/post_backend.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <xview/rectlist.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" void rl_initwithrect(Rect *, Rectlist *);
extern "C" void rl_free(Rectlist *);

namespace
{
    namespace ol = linux::openlook;
    int failures = 0;
    int paint_count = 0;

    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    Xv_opaque peer(native::wnd &owner) {
        return ol::wnd_bindings.handle_from_object(&owner);
    }

    void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    void await_viewable(Window window) {
        // Real window managers map frames asynchronously. XGetImage needs
        // a viewable descendant, not merely a created native resource.
        XWindowAttributes attributes{};
        for (int attempt = 0; attempt < 200; ++attempt) {
            XGetWindowAttributes(ol::cached_display, window, &attributes);
            if (attributes.map_state == IsViewable)
                return;
            notify_dispatch();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        expect(false, "native window is mapped before capturing pixels");
    }

    std::vector<unsigned long> pixels(native::wnd &owner) {
        auto *cache = ol::wnd_gpx_bindings.object_from_handle(&owner);
        expect(cache && cache->backbuffer, "backbuffer exists");
        XImage *image = XGetImage(ol::cached_display, cache->backbuffer,
            250, 200, 180, 100, AllPlanes, ZPixmap);
        expect(image != nullptr, "read theme pixels");
        std::vector<unsigned long> result;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 180; ++x)
                result.push_back(XGetPixel(image, x, y));
        XDestroyImage(image);
        return result;
    }

    void key(native::code_edit &editor, KeySym symbol,
             unsigned int modifiers = 0) {
        auto *state = ol::code_edit_bindings.object_from_handle(&editor);
        XEvent native_event{};
        native_event.xkey.type = KeyPress;
        native_event.xkey.display = ol::cached_display;
        native_event.xkey.window = ol::drawable(&editor);
        native_event.xkey.keycode = XKeysymToKeycode(ol::cached_display, symbol);
        native_event.xkey.state = modifiers;
        Event event{};
        event_set_id(&event, symbol < 128 ? symbol : 0);
        event_set_xevent(&event, &native_event);
        notify_post_event(state->paint_window,
            reinterpret_cast<Notify_event>(&event), NOTIFY_SAFE);
        notify_dispatch();
    }

    void check_tab_fitting(native::tab_view &tabs) {
        for (const auto placement : {native::tab_placement::top,
                 native::tab_placement::bottom, native::tab_placement::left,
                 native::tab_placement::right}) {
            tabs.set_tab_placement(placement);
            for (const int height : {120, 138, 149, 121}) {
                tabs.set_dimensions(native::size(300, height));
                for (const int selected : {0, 1}) {
                    tabs.on_native_selection(selected);
                    notify_dispatch();
                    native::detail::drain_posted_work();
                    auto &page = tabs.get_item(selected).get_content();
                    const int actual_height = xv_get(peer(page), XV_HEIGHT);
                    const int actual_y = xv_get(peer(page), XV_Y);
                    const int page_height = tabs.get_content_bounds().d.h;
                    expect(actual_height <= page_height,
                           "native list bottom border fits inside the tab page");
                    if (placement == native::tab_placement::bottom) {
                        expect(actual_y + actual_height == page_height,
                               "bottom tab joins the actual native list border");
                    } else {
                        expect(actual_y == 0,
                               "top and side pages retain their top alignment");
                    }
                    if (actual_y > 0) {
                        const auto *state = ol::tab_view_bindings
                            .object_from_handle(&tabs);
                        await_viewable(xv_get(state->content_panel, XV_XID));
                        XImage *image = XGetImage(ol::cached_display,
                            xv_get(state->content_panel, XV_XID),
                            0, 0, 100, actual_y, AllPlanes, ZPixmap);
                        expect(image != nullptr, "read spare page space");
                        const auto paper = XGetPixel(image, 50, actual_y - 1);
                        bool clean = true;
                        for (int y = 0; y < actual_y; ++y)
                            for (int x = 0; x < 100; ++x)
                                clean = clean && XGetPixel(image, x, y) == paper;
                        XDestroyImage(image);
                        if (!clean)
                            std::cerr << "tab height=" << height
                                << " selected=" << selected
                                << " list y=" << actual_y
                                << " native height=" << actual_height << '\n';
                        expect(clean, "page switching leaves no provisional border");
                    }
                }
            }
        }
    }

    void check_split_grip(native::split_view &split) {
        const auto *state = ol::split_view_bindings.object_from_handle(&split);
        const Window window = xv_get(state->host, XV_XID);
        await_viewable(window);
        for (const auto orientation : {native::split_orientation::horizontal,
                 native::split_orientation::vertical}) {
            split.set_orientation(orientation);
            const auto bar = split.get_splitter_bounds();
            XClearArea(ol::cached_display, window,
                bar.p.x, bar.p.y, bar.d.w, bar.d.h, False);
            auto repaint = reinterpret_cast<void (*)(Panel, Xv_Window, Rectlist *)>(
                xv_get(state->host, PANEL_REPAINT_PROC));
            repaint(state->host, state->host, nullptr);
            XImage *image = XGetImage(ol::cached_display, window,
                bar.p.x, bar.p.y, bar.d.w, bar.d.h, AllPlanes, ZPixmap);
            expect(image != nullptr, "read splitter grip pixels");
            const bool vertical = orientation == native::split_orientation::horizontal;
            const int start = ((vertical ? bar.d.h : bar.d.w) - 18) / 2;
            const auto sample = [&](int offset) {
                return XGetPixel(image,
                    vertical ? bar.d.w / 2 : start + offset,
                    vertical ? start + offset : bar.d.h / 2);
            };
            const bool ribbed = sample(1) != sample(2) &&
                sample(1) == sample(5) && sample(2) == sample(6);
            XDestroyImage(image);
            expect(ribbed, "splitter has separated grip ribs in both orientations");
        }
    }

    void checks(native::app_wnd &root) {
        const int before = paint_count;
        for (int index = 0; index < 20; ++index)
            root.invalidate();
        expect(paint_count == before && ol::window_state(&root)->repaint_pending,
               "repeated invalidation is deferred and coalesced");
        native::radio compact("Compact", 10, 10, 140, 24);
        native::radio detailed("Detailed", 10, 40, 140, 24);
        compact.set_selected(true);
        attach(compact, root);
        attach(detailed, root);
        expect(xv_get(peer(compact), PANEL_TOGGLE_VALUE, 0) &&
                   !xv_get(peer(detailed), PANEL_TOGGLE_VALUE, 0),
               "initial native radio selection matches portable state");
        for (int index = 0; index < 8; ++index) {
            auto &selected = index % 2 ? compact : detailed;
            auto &other = index % 2 ? detailed : compact;
            auto callback = reinterpret_cast<void (*)(Panel_item, int, Event *)>(
                xv_get(peer(selected), PANEL_NOTIFY_PROC));
            callback(peer(selected), 0, nullptr);
            expect(selected.get_selected() && !other.get_selected() &&
                       xv_get(peer(selected), PANEL_TOGGLE_VALUE, 0) &&
                       !xv_get(peer(other), PANEL_TOGGLE_VALUE, 0),
                   "native radio callbacks exclude the sibling");
        }

        native::button button("Activate", 10, 80, 120, 32);
        attach(button, root);
        const int height = xv_get(peer(button), XV_HEIGHT);
        button.set_dimensions(native::size(200, 90));
        expect(static_cast<int>(xv_get(peer(button), XV_WIDTH)) == 200 &&
                   static_cast<int>(xv_get(peer(button), XV_HEIGHT)) == height,
               "native button width changes, native height stays intrinsic");

        auto *window = ol::window_state(&root);
        auto repaint = reinterpret_cast<void (*)(Panel, Xv_Window, Rectlist *)>(
            xv_get(window->content, PANEL_REPAINT_PROC));
        repaint(window->content, window->paint_window, nullptr);
        const auto original = pixels(root);
        Rect damaged{300, 210, 45, 55};
        Rectlist areas{};
        rl_initwithrect(&damaged, &areas);
        repaint(window->content, window->paint_window, &areas);
        rl_free(&areas);
        expect(pixels(root) == original,
               "partial exposure retains identical native theme pixels");

        native::list general({"General content"}, 0, 0, 100, 100);
        native::list advanced({"Advanced content"}, 0, 0, 100, 100);
        native::tab_view tabs(250, 10, 300, 140);
        tabs.add_item("General", general);
        tabs.add_item("Advanced", advanced);
        attach(tabs, root);
        for (const int index : {1, 0, 1}) {
            tabs.on_native_selection(index);
            notify_dispatch();
            native::detail::drain_posted_work();
            const auto *tab = ol::tab_view_bindings.object_from_handle(&tabs);
            await_viewable(xv_get(tab->content_panel, XV_XID));
            XImage *image = XGetImage(ol::cached_display,
                xv_get(tab->content_panel, XV_XID),
                5, 5, 120, 30, AllPlanes, ZPixmap);
            expect(image != nullptr, "read native tab page pixels");
            const auto background = XGetPixel(image, 0, 0);
            bool text_visible = false;
            for (int y = 0; y < 30; ++y)
                for (int x = 0; x < 120; ++x)
                    text_visible = text_visible ||
                        XGetPixel(image, x, y) != background;
            XDestroyImage(image);
            expect(text_visible, "new native tab page survives old page cleanup");
        }
        check_tab_fitting(tabs);

        native::code_edit editor("", 10, 320, 400, 100);
        attach(editor, root);
        auto *code = ol::code_edit_bindings.object_from_handle(&editor);
        expect(xv_get(code->panel, PANEL_ACCEPT_KEYSTROKE),
               "source editor panel accepts keyboard focus");
        expect(xv_set(code->paint_window, WIN_SET_FOCUS, nullptr) == XV_OK,
               "source editor can acquire native keyboard focus");
        key(editor, XK_a);
        expect(editor.get_text() == "a", "XView key event inserts text");
        key(editor, XK_BackSpace);
        expect(editor.get_text().empty(), "XView Backspace edits text");
        editor.on_complete.connect([&editor](native::completion_item item) {
            editor.on_native_text_input(item.insert);
            return true;
        });
        editor.show_completion({{"return", "return", "keyword"}});
        key(editor, XK_Return);
        expect(editor.get_text() == "return" && !editor.get_completion_visible(),
               "XView Return accepts completion");

        for (int cycle = 0; cycle < 4; ++cycle) {
            native::modeless_wnd chrome(root, "Chrome lifecycle", 80, 80, 480, 360);
            chrome.create();
            auto appearance = native::theme::create(chrome.get_gpx());
            expect(appearance->get_status_bar_height() > 0,
                   "pre-show graphics support chrome metrics");
            appearance.reset();
            native::ruler ruler(chrome, native::window_edge::top, 24);
            native::status_bar status(chrome);
            native::combo_box choice({"One", "Two"},
                native::combo_box_style::drop_down_list, 10, 30, 180, 24);
            native::combo_box editable({"25 mm", "50 mm"},
                native::combo_box_style::editable, 10, 60, 180, 24);
            attach(choice, chrome);
            attach(editable, chrome);
            choice.set_selected_index(1);
            editable.set_selected_index(1);
            expect(editable.get_text() == "50 mm", "combo selection updates text");
            native::list first({"One", "Two"}, 0, 0, 100, 100);
            native::list second({"Three", "Four"}, 0, 0, 100, 100);
            native::split_view split(first, second,
                native::split_orientation::horizontal, 10, 100, 440, 180);
            attach(split, chrome);
            chrome.show();
            for (const float ratio : {0.2f, 0.8f, 0.5f}) {
                split.set_ratio(ratio);
                expect(static_cast<int>(xv_get(peer(first), XV_WIDTH)) ==
                           first.get_dimensions().w &&
                           static_cast<int>(xv_get(peer(second), XV_WIDTH)) ==
                           second.get_dimensions().w,
                       "both native lists and scrollbars fit their resized panes");
            }
            if (cycle == 0)
                check_split_grip(split);
            chrome.destroy();
            expect(!split.get_created() && !first.get_created() &&
                       !second.get_created() && !editable.get_created(),
                   "closing chrome releases child peers before parent panels");
            native::modal_wnd modal(root, "Modal lifecycle", 80, 80, 220, 120);
            modal.create();
            modal.show();
            modal.destroy();
        }
    }
}

int program(int, char **) {
    native::app_wnd root("OPEN LOOK peer regressions", 20, 20, 660, 480);
    root.on_wnd_paint.connect([](native::wnd_paint_event event) {
        ++paint_count;
        auto appearance = native::theme::create(event.g);
        appearance->draw_list(native::rect(250, 200, 180, 100),
            {"Themed list", "Native look", "Portable API"}, 1, {});
        return true;
    });
    root.on_wnd_create.connect([&root] {
        native::app::post([&root] {
            try {
                checks(root);
                std::cout << "OPEN LOOK peer checks passed\n";
            } catch (const std::exception &error) {
                ++failures;
                std::cerr << error.what() << '\n';
            }
            // Let queued item/panel destruction run before closing its root.
            native::app::post([&root] { root.destroy(); });
        });
        return true;
    });
    return native::app::run(root) || failures ? 1 : 0;
}
