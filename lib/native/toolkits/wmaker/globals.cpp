//
// Defines process-wide WINGs state and private backend lookup helpers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "globals.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

#include <native/app.h>
#include <native/file_dialog.h>
#include <native/modal_wnd.h>
#include <native/screen.h>

#include <WINGs/WINGsP.h>

namespace linux::wmaker
{
    namespace
    {
        std::vector<std::function<void()>> deferred_callbacks;
    }

    bool initialized = false;
    bool exit_requested = false;
    Display *display = nullptr;
    WMScreen *screen = nullptr;
    WMColor *list_selection_background = nullptr;
    WMColor *list_selection_text = nullptr;

    native::bindings<WMWidget *, native::wnd *> wnd_bindings;
    native::bindings<native::app_wnd *, window_state *>
        window_bindings;
    native::bindings<native::wnd *, window_graphics *>
        graphics_bindings;
    native::bindings<std::uint32_t, native_font *> font_bindings;
    native::bindings<std::uint32_t, native_menu *> menu_bindings;
    native::bindings<native::text_edit *, native_text_edit *>
        text_edit_bindings;
    native::bindings<native::combo_box *, native_combo_box *>
        combo_box_bindings;
    native::bindings<native::accordion *, native_collection *>
        accordion_bindings;
    native::bindings<native::tab_view *, native_tab_view *>
        tab_view_bindings;
    native::bindings<native::split_view *, native_split_view *>
        split_view_bindings;
    native::bindings<native::icon_view *, native_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, native_collection *>
        tree_view_bindings;
    native::bindings<native::table_view *, native_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, native_collection *>
        code_edit_bindings;

    void initialize() {
        if (initialized)
            return;

        int argc = native::app::argc;
        char **argv = native::app::argv;
        WMInitializeApplication("Native", &argc, argv);

        if (!display)
            display = XOpenDisplay(nullptr);
        if (!display) {
            WMReleaseApplication();
            throw std::runtime_error(
                "Window Maker/WINGs: unable to open the X display.");
        }

        screen = WMCreateScreen(display, DefaultScreen(display));
        if (!screen) {
            XCloseDisplay(display);
            display = nullptr;
            WMReleaseApplication();
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a screen.");
        }
        // Match the desktop's inactive-title/window gray. WINGs otherwise
        // uses a historical slightly purple gray that visibly disagrees
        // with the Window Maker session's native applications.
        auto *private_screen = reinterpret_cast<W_Screen *>(screen);
        WMColor *panel_gray = WMCreateRGBColor(
            screen, 0xaaaa, 0xaaaa, 0xaaaa, False);
        if (panel_gray) {
            WMReleaseColor(private_screen->gray);
            private_screen->gray = panel_gray;
        }
        // WINGs paints selected WMList rows white by default. Keep the
        // native list widget and its input/scroller behavior, but expose the
        // same selection colors used by the Window Maker collection and
        // table adapters to its supported user-draw callback.
        list_selection_background = WMCreateRGBColor(
            screen, 0x5555, 0x5555, 0x5555, False);
        list_selection_text = WMCreateRGBColor(
            screen, 0xd7d7, 0xd7d7, 0xd7d7, False);
        initialized = true;
    }

    WMWidget *parent_widget(native::wnd *control) {
        native::wnd *parent = control ? control->get_parent() : nullptr;
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *state = tab_view_bindings.object_from_handle(tabs);
            if (state) {
                for (std::size_t index = 0;
                     index < tabs->get_item_count() &&
                     index < state->pages.size(); ++index) {
                    if (&tabs->get_item(index).get_content() == control)
                        return state->pages[index];
                }
            }
        }
        if (auto *split = dynamic_cast<native::split_view *>(parent)) {
            auto *state = split_view_bindings.object_from_handle(split);
            if (state) {
                if (&split->get_first() == control)
                    return state->first;
                if (&split->get_second() == control)
                    return state->second;
            }
        }
        WMWidget *widget = parent
                               ? wnd_bindings.handle_from_object(parent)
                               : nullptr;
        if (!parent || !parent->get_created() || !widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: control requires a created "
                "parent.");
        }
        return widget;
    }

    window_state *state(native::app_wnd *window) {
        return window_bindings.object_from_handle(window);
    }

    Window drawable(native::wnd *window) {
        WMWidget *widget = wnd_bindings.handle_from_object(window);
        return widget ? WMWidgetXID(widget) : None;
    }

    native::point control_position(const native::wnd *control) {
        native::point result = control->get_position();
        auto *top = dynamic_cast<native::app_wnd *>(
            control->get_parent());
        window_state *owner_state = top ? state(top) : nullptr;
        if (owner_state)
            result.y += owner_state->menu_height;
        return result;
    }

    native::point constrain_position(
        const native::point &preferred,
        const native::size &dimensions) {
        if (!screen)
            return preferred;
        constexpr int title_height = 24;
        native::screen *target = nullptr;
        for (int index = 0; index < native::screen::count(); ++index) {
            native::screen *candidate = native::screen::at(index);
            if (candidate &&
                candidate->work_area().contains(preferred)) {
                target = candidate;
                break;
            }
        }
        if (!target)
            target = native::screen::primary();
        const native::rect area = target
                                      ? target->work_area()
                                      : native::rect(
                                            0,
                                            0,
                                            WMScreenWidth(screen),
                                            WMScreenHeight(screen));
        const int minimum_x = area.x1();
        const int minimum_y = area.y1();
        const int maximum_x = dimensions.w <= area.d.w
                                  ? area.x2() - dimensions.w
                                  : minimum_x;
        const int maximum_y = std::max(
            minimum_y, area.y2() - title_height);
        native::point result(
            static_cast<native::coord>(std::clamp<int>(
                preferred.x, minimum_x, maximum_x)),
            static_cast<native::coord>(std::clamp<int>(
                preferred.y, minimum_y, maximum_y)));
        return result;
    }

    bool permit_input(native::wnd *window) {
        if (!window)
            return false;
        if (window->get_input_enabled())
            return true;

        native::wnd *root = window;
        while (root->get_parent())
            root = root->get_parent();
        auto *branch = dynamic_cast<native::app_wnd *>(root);
        native::modal_wnd *modal = nullptr;
        while (branch && !modal) {
            modal = branch->get_active_modal();
            branch = branch->get_owner();
        }
        window_state *modal_state = modal ? state(modal) : nullptr;
        if (modal_state && modal_state->window) {
            WMRaiseWidget(modal_state->window);
            const Window target =
                WMWidgetXID(modal_state->window);
            if (target != None) {
                XSetInputFocus(display,
                               target,
                               RevertToParent,
                               CurrentTime);
            }
            XFlush(display);
        }
        return false;
    }

    void schedule_repaint(native::app_wnd *window,
                          const native::rect &area) {
        Window target = drawable(window);
        if (!display || target == None || area.d.w <= 0 ||
            area.d.h <= 0) {
            return;
        }
        window_state *window_state = state(window);
        const int offset = window_state
                               ? window_state->menu_height
                               : 0;
        XClearArea(display,
                   target,
                   area.p.x,
                   area.p.y + offset,
                   area.d.w,
                   area.d.h,
                   True);
        XFlush(display);
    }

    void defer(std::function<void()> callback) {
        if (callback)
            deferred_callbacks.push_back(std::move(callback));
    }

    void dispatch_deferred() {
        while (!deferred_callbacks.empty()) {
            std::vector<std::function<void()>> callbacks;
            callbacks.swap(deferred_callbacks);
            for (auto &callback : callbacks)
                callback();
        }
    }
} // namespace linux::wmaker
