//
// Implements the SDL2 shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"
#include "../../mouse_state.h"

namespace linux::sdl2
{
    namespace
    {
        SDL_Cursor *arrow_cursor = nullptr;
        SDL_Cursor *ibeam_cursor = nullptr;
        SDL_Cursor *crosshair_cursor = nullptr;
        SDL_Cursor *horizontal_resize_cursor = nullptr;
        SDL_Cursor *vertical_resize_cursor = nullptr;
        SDL_Cursor *northwest_southeast_resize_cursor = nullptr;
        SDL_Cursor *northeast_southwest_resize_cursor = nullptr;

        SDL_Cursor *system_cursor(native::mouse_cursor cursor) {
            SDL_Cursor **cached = &arrow_cursor;
            SDL_SystemCursor shape = SDL_SYSTEM_CURSOR_ARROW;
            if (cursor == native::mouse_cursor::ibeam) {
                cached = &ibeam_cursor;
                shape = SDL_SYSTEM_CURSOR_IBEAM;
            } else if (cursor == native::mouse_cursor::crosshair) {
                cached = &crosshair_cursor;
                shape = SDL_SYSTEM_CURSOR_CROSSHAIR;
            } else if (cursor ==
                       native::mouse_cursor::resize_horizontal) {
                cached = &horizontal_resize_cursor;
                shape = SDL_SYSTEM_CURSOR_SIZEWE;
            } else if (cursor ==
                       native::mouse_cursor::resize_vertical) {
                cached = &vertical_resize_cursor;
                shape = SDL_SYSTEM_CURSOR_SIZENS;
            } else if (cursor == native::mouse_cursor::
                                     resize_northwest_southeast) {
                cached = &northwest_southeast_resize_cursor;
                shape = SDL_SYSTEM_CURSOR_SIZENWSE;
            } else if (cursor == native::mouse_cursor::
                                     resize_northeast_southwest) {
                cached = &northeast_southwest_resize_cursor;
                shape = SDL_SYSTEM_CURSOR_SIZENESW;
            }
            if (!*cached)
                *cached = SDL_CreateSystemCursor(shape);
            return *cached;
        }
    } // namespace

    int depth_of(const native::wnd &control) {
        int depth = 0;
        for (native::wnd *parent = control.get_parent(); parent;
             parent = parent->get_parent())
            ++depth;
        return depth;
    }

    int content_origin_y(native::wnd *window) {
        auto *application_window =
            dynamic_cast<native::app_wnd *>(window);
        return application_window && application_window->menu.id()
                   ? menu_bar_height
                   : 0;
    }

    void update_mouse_cursor(native::wnd *window,
                             native::point position) {
        if (!window)
            return;

        bool menu_open = false;
        if (auto *application = dynamic_cast<native::app_wnd *>(window))
            if (auto *menu = menu_bindings.object_from_handle(application->menu.id()))
                menu_open = menu->open_idx >= 0;
        const auto weak = native::detail::mouse_access::lifetime(*window);
        if (menu_open) native::detail::pointer_hover(nullptr, position);
        else native::detail::pointer_position(*window, position);
        const auto lifetime = weak.lock();
        if (!lifetime || !lifetime->alive) return;
        native::wnd *target = native::detail::deepest_at(*window, position);
        // A control that owns chrome answers per position, so the
        // root point becomes local before the shape is resolved.
        native::point local = position;
        if (target != window) {
            const native::point origin =
                native::detail::origin_in_root(*target);
            local = native::point(
                static_cast<native::coord>(position.x - origin.x),
                static_cast<native::coord>(position.y - origin.y));
        }
        auto shape = native::detail::pointer_cursor(*target, local);
        if (position.y < 0 || menu_open || !window->get_input_enabled())
            shape = native::mouse_cursor::arrow;
        if (native::detail::pointer_relative(*window)) return;
        SDL_ShowCursor(shape == native::mouse_cursor::hidden
            ? SDL_DISABLE : SDL_ENABLE);
        if (shape == native::mouse_cursor::hidden) return;
        SDL_Cursor *cursor = system_cursor(shape);
        if (cursor)
            SDL_SetCursor(cursor);
    }

    void shutdown_mouse_cursors() {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetCursor(SDL_GetDefaultCursor());
        if (arrow_cursor)
            SDL_FreeCursor(arrow_cursor);
        if (ibeam_cursor)
            SDL_FreeCursor(ibeam_cursor);
        if (crosshair_cursor)
            SDL_FreeCursor(crosshair_cursor);
        if (horizontal_resize_cursor)
            SDL_FreeCursor(horizontal_resize_cursor);
        if (vertical_resize_cursor)
            SDL_FreeCursor(vertical_resize_cursor);
        if (northwest_southeast_resize_cursor)
            SDL_FreeCursor(northwest_southeast_resize_cursor);
        if (northeast_southwest_resize_cursor)
            SDL_FreeCursor(northeast_southwest_resize_cursor);
        arrow_cursor = nullptr;
        ibeam_cursor = nullptr;
        crosshair_cursor = nullptr;
        horizontal_resize_cursor = nullptr;
        vertical_resize_cursor = nullptr;
        northwest_southeast_resize_cursor = nullptr;
        northeast_southwest_resize_cursor = nullptr;
    }

    void restore_window_focus(native::app_wnd *window) {
        SDL_CaptureMouse(SDL_FALSE);
        if (!window || !window->get_created())
            return;

        SDL_Window *native_window =
            wnd_bindings.handle_from_object(window);
        if (!native_window)
            return;

        SDL_RaiseWindow(native_window);
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetWindowInputFocus(native_window);
#endif
        window->invalidate();
    }

    SDL_Window *main_window = nullptr;
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    native::bindings<uint32_t, sdl2_menu *> menu_bindings;
    std::vector<native::check *> checks;
    std::vector<native::radio *> radios;
    std::vector<native::list *> lists;
    std::vector<native::combo_box *> combo_boxes;
    std::vector<native::button *> buttons;
    std::vector<native::text_edit *> text_edits;
    std::vector<native::accordion *> accordions;
    std::vector<native::tab_view *> tab_views;
    std::vector<native::split_view *> split_views;
    std::vector<native::icon_view *> icon_views;
    std::vector<native::tree_view *> tree_views;
    std::vector<native::table_view *> table_views;
    std::vector<native::code_edit *> code_edits;
    std::vector<native::panel *> panels;
    std::vector<native::canvas *> canvases;
    std::vector<native::app_wnd *> windows;
#ifdef HAVE_SDL2_TTF
    native::bindings<uint32_t, sdl2_font *> font_bindings;
#endif
} // namespace linux::sdl2
