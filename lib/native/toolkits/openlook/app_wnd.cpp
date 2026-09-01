//
// Implements XView application, modeless, and modal windows around a
// native Frame and Panel drawing/control surface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native.h>
#include <native/app.h>
#include <native/app_wnd.h>
#include <native/file_dialog.h>
#include <native/modal_wnd.h>

#include <X11/Xlib.h>
#include <xview/canvas.h>
#include <xview/notify.h>
#include <xview/rectlist.h>
#include <xview/win_input.h>
#include <xview/window.h>
#include <xview/xview.h>

#include "globals.h"
#include "window_position.h"
#include "xview_init.h"

namespace
{
    native::mouse_button decode_button(int action) {
        switch (action) {
        case ACTION_SELECT:
            return native::mouse_button::left;
        case ACTION_ADJUST:
            return native::mouse_button::middle;
        case ACTION_MENU:
            return native::mouse_button::right;
        default:
            return native::mouse_button::none;
        }
    }

    void ensure_backbuffer(native::app_wnd *owner,
                           int width,
                           int height) {
        if (!owner || !linux::openlook::cached_display ||
            width <= 0 || height <= 0)
            return;
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(owner);
        if (!cache ||
            (cache->backbuffer &&
             cache->buffer_width == width &&
             cache->buffer_height == height)) {
            return;
        }

        if (cache->backbuffer) {
            XFreePixmap(
                linux::openlook::cached_display,
                cache->backbuffer);
        }
        const Window drawable = linux::openlook::drawable(owner);
        if (drawable == None)
            return;
        cache->backbuffer = XCreatePixmap(
            linux::openlook::cached_display,
            drawable,
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            DefaultDepth(
                linux::openlook::cached_display,
                DefaultScreen(linux::openlook::cached_display)));
        cache->buffer_width = width;
        cache->buffer_height = height;
    }

    // True while the panel repaint procedure runs, so restoring the
    // items below cannot re-enter it.
    bool repainting_panel = false;

    void repaint_panel(Panel panel,
                       Xv_Window paint_window,
                       Rectlist *areas) {
        auto *owner = dynamic_cast<native::app_wnd *>(
            linux::openlook::wnd_bindings.object_from_handle(panel));
        if (!owner || !owner->get_created())
            return;

        const Window window = paint_window
                                  ? static_cast<Window>(xv_get(
                                        paint_window, XV_XID))
                                  : None;
        if (window == None)
            return;

        const int width = static_cast<int>(xv_get(panel, XV_WIDTH));
        const int height = static_cast<int>(xv_get(panel, XV_HEIGHT));
        native::rect invalid(0, 0, width, height);
        if (areas && !rl_empty(areas)) {
            invalid = native::rect(
                areas->rl_bound.r_left + areas->rl_x,
                areas->rl_bound.r_top + areas->rl_y,
                areas->rl_bound.r_width,
                areas->rl_bound.r_height);
        }

        native::gpx &graphics = owner->get_gpx();
        ensure_backbuffer(owner, width, height);
        graphics.set_clip(invalid);
        graphics.clear(graphics.get_paper());
        native::wnd_paint_event event(invalid, graphics);
        owner->on_wnd_paint.emit(event);

        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(owner);
        if (!cache || !cache->gc || !cache->backbuffer)
            return;
        XSetClipMask(
            linux::openlook::cached_display, cache->gc, None);
        XCopyArea(linux::openlook::cached_display,
                  cache->backbuffer,
                  window,
                  cache->gc,
                  invalid.p.x,
                  invalid.p.y,
                  invalid.d.w,
                  invalid.d.h,
                  invalid.p.x,
                  invalid.p.y);

        // XView calls this procedure to restore the background under
        // an item it is clearing, which is what it does to every item
        // a layout moves or resizes. The background just painted
        // covers whatever else stood in that rectangle, so any other
        // item overlapping it is now erased and nothing would draw it
        // again. Put the items back on top.
        //
        // PANEL_NO_CLEAR paints without clearing, so this cannot
        // come back round through panel_default_clear_item(); the
        // guard covers the case where a paint handler itself
        // triggers another repaint.
        if (!repainting_panel) {
            repainting_panel = true;
            panel_paint(panel, PANEL_NO_CLEAR);
            repainting_panel = false;
        }
    }

    void resize_panel(Canvas canvas, int width, int height) {
        auto *owner = dynamic_cast<native::app_wnd *>(
            linux::openlook::wnd_bindings.object_from_handle(canvas));
        if (!owner || width <= 0 || height <= 0)
            return;
        ensure_backbuffer(owner, width, height);
        const native::size dimensions(
            static_cast<native::dim>(width),
            static_cast<native::dim>(height));
        owner->on_native_resize(dimensions);
        owner->on_wnd_resize.emit(dimensions);
    }

    // Bring the portable size and the content panel back in line
    // with the frame.
    //
    // CANVAS_RESIZE_PROC is a Canvas attribute and the content is a
    // Panel, so the resize proc set on it never runs and a resized
    // window would otherwise never reach the portable layer: an
    // installed layout would arrange its children once and never
    // again. ConfigureNotify on the frame is the notification that
    // does arrive, so the size is taken from the frame itself.
    void sync_frame_size(native::app_wnd *owner) {
        auto *state = linux::openlook::window_state(owner);
        if (!state || !state->frame || !state->content)
            return;

        const int frame_width =
            static_cast<int>(xv_get(state->frame, XV_WIDTH));
        const int frame_height =
            static_cast<int>(xv_get(state->frame, XV_HEIGHT));
        const int width = frame_width;
        const int height = frame_height - state->menu_height;
        if (width <= 0 || height <= 0)
            return;

        const native::size dimensions(
            static_cast<native::dim>(width),
            static_cast<native::dim>(height));
        if (dimensions.w == owner->get_dimensions().w &&
            dimensions.h == owner->get_dimensions().h)
            return;

        xv_set(state->content,
               XV_WIDTH,
               width,
               XV_HEIGHT,
               height,
               nullptr);

        // Resizing the panel gives it a fresh paint window. Holding
        // the old one would leave every later repaint drawing into a
        // window that is no longer on screen.
        if (Xv_Window paint = static_cast<Xv_Window>(
                xv_get(state->content, CANVAS_NTH_PAINT_WINDOW, 0)))
            state->paint_window = paint;

        ensure_backbuffer(owner, width, height);
        owner->on_native_resize(dimensions);
        owner->on_wnd_resize.emit(dimensions);

        // The server clears the window when it changes size and the
        // panel does not redraw its items by itself, so without this
        // controls vanish until something else asks for a repaint.
        // Done after the layout has run, so items are drawn where the
        // new size puts them.
        owner->invalidate();
    }

    Notify_value handle_window_event(
        Xv_Window window,
        Event *event,
        Notify_arg argument,
        Notify_event_type type) {
        auto *owner = reinterpret_cast<native::app_wnd *>(
            xv_get(window, WIN_CLIENT_DATA));
        if (!owner || !event) {
            return notify_next_event_func(
                window,
                reinterpret_cast<Notify_event>(event),
                argument,
                type);
        }

        linux::openlook::permit_input(owner);

        const int action = event_action(event);

        // The window manager's Quit sends WM_DELETE_WINDOW, which
        // XView turns into ACTION_DISMISS for an owned frame (a
        // root-owned one it destroys itself). ACTION_DISMISS is what
        // frame_event_proc() would turn into a FRAME_DONE_PROC call,
        // but installing an event procedure on the frame displaces
        // that procedure, so the done proc never runs: the frame is
        // dismissed while the window still believes it exists, and
        // every later show() re-shows a frame that is gone. Close it
        // here instead. destroy() is idempotent, so a backend path
        // that does reach frame_done() still costs nothing.
        if (action == ACTION_DISMISS) {
            if (!linux::openlook::permit_input(owner))
                return NOTIFY_DONE;

            owner->destroy();

            // The frame and its panel items are gone. Passing the
            // event on would hand the next procedure in the chain a
            // window that no longer exists, which XView reports as an
            // invalid object, so the dispatch stops here.
            return NOTIFY_DONE;
        }
        if (owner->get_input_enabled()) {
            if (action == LOC_MOVE || action == LOC_DRAG) {
                owner->on_mouse_move.emit(native::point(
                    event_x(event), event_y(event)));
            } else if (action == ACTION_SCROLL_UP ||
                       action == ACTION_SCROLL_DOWN) {
                owner->on_mouse_wheel.emit(
                    native::mouse_wheel_event(
                        native::point(
                            event_x(event), event_y(event)),
                        action == ACTION_SCROLL_UP ? 1 : -1,
                        native::wheel_direction::vertical));
            } else {
                const native::mouse_button button =
                    decode_button(action);
                if (button != native::mouse_button::none) {
                    owner->on_mouse_click.emit(native::mouse_event(
                        button,
                        event_is_down(event)
                            ? native::mouse_action::press
                            : native::mouse_action::release,
                        native::point(
                            event_x(event), event_y(event))));
                }
            }
        }

        XEvent *native_event = event_xevent(event);
        if (native_event && native_event->type == ConfigureNotify) {
            auto *state = linux::openlook::window_state(owner);
            const native::point position =
                linux::openlook::frame_position(
                    state ? state->frame : XV_NULL);
            owner->on_native_move(position);
            owner->on_wnd_move.emit(position);
            sync_frame_size(owner);
        }
        return notify_next_event_func(
            window,
            reinterpret_cast<Notify_event>(event),
            argument,
            type);
    }

    void frame_done(Frame frame) {
        native::app_wnd *owner =
            linux::openlook::frame_bindings.object_from_handle(frame);
        if (linux::openlook::permit_input(owner))
            owner->destroy();
    }

    int frame_height(const native::app_wnd *window,
                     const linux::openlook::openlook_window *state) {
        return static_cast<int>(window->get_dimensions().h) +
               (state ? state->menu_height : 0);
    }
} // namespace

namespace linux::openlook
{
    void repaint_window(native::app_wnd *window,
                        const native::rect &area) {
        openlook_window *state = window_state(window);
        if (!state || !state->content || !state->paint_window ||
            area.d.w <= 0 || area.d.h <= 0) {
            return;
        }

        repaint_panel(state->content,
                      state->paint_window,
                      nullptr);
        panel_paint(state->content, PANEL_NO_CLEAR);
        XFlush(cached_display);
    }
} // namespace linux::openlook

namespace native
{
    void app_wnd::apply_title() {
        auto *state = linux::openlook::window_state(this);
        if (!state || !state->frame) {
            throw std::runtime_error(
                "OpenLook/XView: missing frame binding.");
        }
        xv_set(state->frame,
               FRAME_LABEL,
               _title.c_str(),
               nullptr);
    }

    void app_wnd::create() const {
        if (_created)
            return;
        validate_owner_created();
        linux::openlook::initialize_xview();

        auto *self = const_cast<app_wnd *>(this);
        Frame native_owner = XV_NULL;
        if (app_wnd *owner = get_owner()) {
            auto *owner_state = linux::openlook::window_state(owner);
            native_owner = owner_state ? owner_state->frame : XV_NULL;
        }
        Frame frame = static_cast<Frame>(xv_create(
            XV_NULL,
            FRAME,
            FRAME_LABEL,
            _title.c_str(),
            FRAME_SHOW_LABEL,
            TRUE,
            XV_X,
            _bounds.p.x,
            XV_Y,
            _bounds.p.y,
            XV_WIDTH,
            _bounds.d.w,
            XV_HEIGHT,
            _bounds.d.h,
            FRAME_DONE_PROC,
            frame_done,
            nullptr));
        if (!frame) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create a frame.");
        }
        if (get_modal() && native_owner) {
            const Window dialog = static_cast<Window>(xv_get(
                frame, XV_XID));
            const Window parent = static_cast<Window>(xv_get(
                native_owner, XV_XID));
            if (dialog != None && parent != None) {
                XSetTransientForHint(
                    linux::openlook::cached_display,
                    dialog,
                    parent);
            }
        }

        auto *state = new linux::openlook::openlook_window;
        state->frame = frame;
        try {
            linux::openlook::window_bindings.register_pair(
                self, state);
            linux::openlook::frame_bindings.register_pair(frame, self);
            self->menu.attach(*self);

            Panel menu_bar = XV_NULL;
            if (self->menu.id()) {
                auto *menu_state = linux::openlook::menu_bindings
                                       .object_from_handle(
                                           self->menu.id());
                menu_bar = menu_state ? menu_state->bar : XV_NULL;
                state->menu_height = menu_bar
                                         ? static_cast<int>(xv_get(
                                               menu_bar, XV_HEIGHT))
                                         : 0;
            }

            state->content = static_cast<Panel>(xv_create(
                frame,
                PANEL,
                PANEL_LAYOUT,
                PANEL_HORIZONTAL,
                PANEL_BORDER,
                FALSE,
                PANEL_REPAINT_PROC,
                repaint_panel,
                CANVAS_RESIZE_PROC,
                resize_panel,
                XV_Y,
                state->menu_height,
                XV_WIDTH,
                _bounds.d.w,
                XV_HEIGHT,
                _bounds.d.h,
                nullptr));
            if (!state->content) {
                throw std::runtime_error(
                    "OpenLook/XView: failed to create content panel.");
            }
            state->paint_window = static_cast<Xv_Window>(xv_get(
                state->content, CANVAS_NTH_PAINT_WINDOW, 0));
            if (!state->paint_window) {
                throw std::runtime_error(
                    "OpenLook/XView: panel has no paint window.");
            }
            xv_set(state->paint_window,
                   WIN_CLIENT_DATA,
                   self,
                   WIN_NOTIFY_SAFE_EVENT_PROC,
                   handle_window_event,
                   WIN_CONSUME_EVENTS,
                   LOC_MOVE,
                   LOC_DRAG,
                   ACTION_SELECT,
                   ACTION_ADJUST,
                   ACTION_MENU,
                   ACTION_SCROLL_UP,
                   ACTION_SCROLL_DOWN,
                   nullptr,
                   nullptr);
            xv_set(frame,
                   WIN_CLIENT_DATA,
                   self,
                   WIN_NOTIFY_SAFE_EVENT_PROC,
                   handle_window_event,
                   WIN_CONSUME_EVENTS,
                   KBD_USE,
                   ACTION_SELECT,
                   ACTION_ADJUST,
                   ACTION_MENU,
                   nullptr,
                   XV_HEIGHT,
                   frame_height(this, state),
                   nullptr);
            linux::openlook::wnd_bindings.register_pair(
                state->content, self);
        } catch (...) {
            linux::openlook::wnd_bindings.unregister_by_object(self);
            linux::openlook::frame_bindings.unregister_by_object(self);
            linux::openlook::window_bindings.unregister_by_handle(self);
            xv_destroy_safe(frame);
            delete state;
            throw;
        }

        _created = true;
        if (self == app::main_wnd())
            linux::openlook::main_frame = frame;
        self->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created) {
            throw std::runtime_error(
                "OpenLook/XView: window is not created.");
        }
        auto *self = const_cast<app_wnd *>(this);
        auto *state = linux::openlook::window_state(self);
        if (!state || !state->frame) {
            throw std::runtime_error(
                "OpenLook/XView: missing frame binding.");
        }

        const point position =
            linux::openlook::constrain_frame_position(
                state->frame, _bounds.p, _bounds.d);
        xv_set(state->frame,
               XV_X,
               position.x,
               XV_Y,
               position.y,
               XV_SHOW,
               TRUE,
               WIN_FRONT,
               nullptr);
        self->on_native_move(position);
        linux::openlook::request_frame_extents(state->frame);

        if (get_modal())
            xv_set(state->frame, WIN_SET_FOCUS, nullptr);
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<app_wnd *>(this);
        auto *state = linux::openlook::window_state(self);

        // Drop the content panel's binding before anything is torn
        // down. Destroying a child item makes XView clear it, which
        // calls the repaint procedure, which would otherwise repaint
        // a panel whose items are half destroyed. Without the
        // binding the procedure has no owner to find and returns.
        linux::openlook::wnd_bindings.unregister_by_object(self);

        self->on_native_destroy();
        linux::openlook::frame_bindings.unregister_by_object(self);
        linux::openlook::window_bindings.unregister_by_handle(self);

        if (state) {
            Frame frame = state->frame;
            state->frame = XV_NULL;
            state->content = XV_NULL;
            state->paint_window = XV_NULL;
            delete state;
            if (frame)
                xv_destroy_safe(frame);
        }
        if (self == app::main_wnd()) {
            linux::openlook::exit_requested = true;
            linux::openlook::main_frame = XV_NULL;
        }
    }
} // namespace native
