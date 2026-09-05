//
// Implements the GEMix application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>
#include <native/app_wnd.h>

#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(this);
        if (handle <= 0)
            throw std::runtime_error(
                "GEMix: Missing window binding for app_wnd.");

        wind_set_str(handle, WF_NAME, _title.c_str());
    }

    void app_wnd::create_native() {
        validate_owner_created();
        if (!linux::gemix::ensure_runtime())
            throw std::runtime_error(
                "GEMix: failed to initialize AES/VDI runtime.");

        rect desktop = linux::gemix::desktop_rect();
        const WORD features =
            get_modal() ? 0
                        : NAME | CLOSER | FULLER | MOVER | SIZER;
        WORD handle =
            wind_create(features,
                        desktop.p.x,
                        desktop.p.y,
                        desktop.d.w,
                        desktop.d.h);
        if (handle <= 0)
            throw std::runtime_error("GEMix: failed to create window.");

        linux::gemix::wnd_bindings.register_pair(
            handle, this);
        linux::gemix::windows.push_back(
            this);
        linux::gemix::window_states.register_pair(
            this, new linux::gemix::gem_window);
        wind_set_str(handle, WF_NAME, _title.c_str());

        this->menu.attach(
            *this);
    }

    void app_wnd::show_native() {
        if (!_created)
            throw std::runtime_error(
                "GEMix: Cannot show window before it is created.");

        WORD handle = linux::gemix::wnd_bindings.handle_from_object(
            this);
        if (handle <= 0)
            throw std::runtime_error(
                "GEMix: Missing window binding for app_wnd.");

        // Public bounds are the client area, so grow the requested
        // size by whatever decorations this window carries before
        // asking AES to open it.
        const size outer =
            linux::gemix::outer_size_for(handle, _bounds.d);
        wind_open(handle, _bounds.p.x, _bounds.p.y, outer.w, outer.h);

        // AES may grant a different rectangle than the one asked for.
        // Report it the same way a user-driven resize is reported, so
        // the cache and any installed layout both follow the geometry
        // the window really has.
        auto *self = this;
        self->on_native_move(linux::gemix::outer_rect(handle).p);
        self->on_native_resize(linux::gemix::work_rect(handle).d);
        linux::gemix::active_window = self;
        if (get_modal())
            wind_set(handle, WF_TOP, 0, 0, 0, 0);
        invalidate();
    }

    void app_wnd::destroy_native() {
        if (!_created)
            return;

        app_wnd *self = this;
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        const rect exposed = handle > 0
            ? linux::gemix::outer_rect(handle) : rect();
        // Keep removal and repainting our exposed windows in one AES update.
        // Nested painting must not publish the intermediate desktop fill.
        struct update_scope {
            update_scope() { wind_update(BEG_UPDATE); }
            ~update_scope() { wind_update(END_UPDATE); }
        } update;
        menu.detach();
        auto *state = linux::gemix::window_states.object_from_handle(this);
        linux::gemix::window_states.unregister_by_handle(this);
        delete state;
        linux::gemix::runtime.pressed_button = nullptr;
        if (handle > 0) {
            wind_close(handle);
            wind_delete(handle);
            linux::gemix::wnd_bindings.unregister_by_handle(handle);
        }
        linux::gemix::windows.erase(
            std::remove(linux::gemix::windows.begin(),
                        linux::gemix::windows.end(),
                        self),
            linux::gemix::windows.end());

        if (linux::gemix::active_window == self) {
            linux::gemix::active_window = owner
                ? owner
                : (linux::gemix::windows.empty()
                       ? nullptr
                       : linux::gemix::windows.back());
        }

        if (get_modal() && owner) {
            app_wnd *focus = owner->get_input_enabled()
                                 ? owner
                                 : owner->get_active_modal();
            WORD focus_handle =
                focus ? linux::gemix::wnd_bindings
                            .handle_from_object(focus)
                      : 0;
            if (focus_handle > 0)
                wind_set(focus_handle, WF_TOP, 0, 0, 0, 0);
        }
        if (app::main_wnd() == this)
            linux::gemix::runtime.shutdown_requested = true;
        else {
            for (auto *remaining : linux::gemix::windows) {
                const WORD remaining_handle = linux::gemix::wnd_bindings
                    .handle_from_object(remaining);
                const rect work = linux::gemix::work_rect(remaining_handle);
                rect dirty = exposed.intersect(work);
                if (!dirty.w() || !dirty.h()) continue;
                dirty.p.x -= work.p.x;
                dirty.p.y -= work.p.y;
                linux::gemix::request_repaint(remaining, &dirty);
            }
            linux::gemix::flush_repaints();
        }
    }
} // namespace native
