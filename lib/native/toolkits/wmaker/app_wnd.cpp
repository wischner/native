//
// Implements portable top-level windows with WINGs windows and panels.
// Painting is buffered below the native menu strip.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/Xlib.h>
#include <WINGs/WINGs.h>

#include <native/app.h>
#include <native/app_wnd.h>

#include "globals.h"

namespace
{
    native::mouse_button decode_button(unsigned int button) {
        switch (button) {
        case Button1:
            return native::mouse_button::left;
        case Button2:
            return native::mouse_button::middle;
        case Button3:
            return native::mouse_button::right;
        default:
            return native::mouse_button::none;
        }
    }

    void resize_backbuffer(native::app_wnd *owner,
                           int width,
                           int height) {
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(owner);
        if (!cache || width <= 0 || height <= 0 ||
            (cache->backbuffer != None && cache->width == width &&
             cache->height == height)) {
            return;
        }
        if (cache->backbuffer != None) {
            XFreePixmap(linux::wmaker::display, cache->backbuffer);
        }
        const Window target = linux::wmaker::drawable(owner);
        if (target == None)
            return;
        cache->backbuffer = XCreatePixmap(
            linux::wmaker::display,
            target,
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            static_cast<unsigned int>(DefaultDepth(
                linux::wmaker::display,
                DefaultScreen(linux::wmaker::display))));
        cache->width = width;
        cache->height = height;
    }

    void paint_window(native::app_wnd *owner) {
        if (!owner || !owner->get_created())
            return;
        auto *state = linux::wmaker::state(owner);
        const Window target = linux::wmaker::drawable(owner);
        if (!state || target == None)
            return;

        native::gpx &graphics = owner->get_gpx();
        auto *cache = linux::wmaker::graphics_bindings
                          .object_from_handle(owner);
        const native::size dimensions = owner->get_dimensions();
        resize_backbuffer(owner, dimensions.w, dimensions.h);
        cache = linux::wmaker::graphics_bindings
                    .object_from_handle(owner);
        if (!cache || cache->backbuffer == None || !cache->gc)
            return;

        const native::rect invalid(0, 0,
                                   dimensions.w,
                                   dimensions.h);
        graphics.set_clip(invalid);
        graphics.clear(graphics.get_paper());
        native::wnd_paint_event event(invalid, graphics);
        owner->on_wnd_paint.emit(event);

        XSetClipMask(linux::wmaker::display, cache->gc, None);
        XCopyArea(linux::wmaker::display,
                  cache->backbuffer,
                  target,
                  cache->gc,
                  0,
                  0,
                  static_cast<unsigned int>(cache->width),
                  static_cast<unsigned int>(cache->height),
                  0,
                  state->menu_height);
        XFlush(linux::wmaker::display);
    }

    void close_window(WMWidget *, void *client_data) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (owner && linux::wmaker::permit_input(owner)) {
            linux::wmaker::defer([owner]() {
                if (owner->get_created())
                    owner->destroy();
            });
        }
    }

    void dispatch_window_event(native::app_wnd *owner,
                               const XEvent &native_event) {
        if (!owner || !owner->get_created())
            return;
        const XEvent *event = &native_event;
        auto *state = linux::wmaker::state(owner);
        const int menu_height = state ? state->menu_height : 0;

        switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0 &&
                event->xexpose.y + event->xexpose.height >
                    menu_height) {
                paint_window(owner);
            }
            break;
        case ConfigureNotify: {
            const int content_height = std::max(
                1, event->xconfigure.height - menu_height);
            const native::size dimensions(
                static_cast<native::dim>(event->xconfigure.width),
                static_cast<native::dim>(content_height));
            if (dimensions.w != owner->get_dimensions().w ||
                dimensions.h != owner->get_dimensions().h) {
                resize_backbuffer(owner,
                                  dimensions.w,
                                  dimensions.h);

                // The menu strip spans the window, so it follows the
                // new width before anything repaints against it.
                linux::wmaker::resize_menu_bar(
                    owner, event->xconfigure.width);
                owner->on_native_resize(dimensions);
                owner->on_wnd_resize.emit(dimensions);
                linux::wmaker::schedule_repaint(
                    owner,
                    native::rect({0, 0}, dimensions));
            }
            const WMPoint position = WMGetViewScreenPosition(
                WMWidgetView(state->window));
            const native::point native_position(
                static_cast<native::coord>(position.x),
                static_cast<native::coord>(position.y));
            if (native_position.x != owner->get_position().x ||
                native_position.y != owner->get_position().y) {
                owner->on_native_move(native_position);
                owner->on_wnd_move.emit(native_position);
            }
            break;
        }
        case FocusIn:
            linux::wmaker::permit_input(owner);
            break;
        case ButtonPress:
        case ButtonRelease: {
            if (!linux::wmaker::permit_input(owner)) {
                return;
            }
            if (event->xbutton.y < menu_height)
                return;
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_mouse_wheel.emit(
                    native::mouse_wheel_event(
                        native::point(
                            event->xbutton.x,
                            event->xbutton.y - menu_height),
                        event->xbutton.button == Button4 ? 1 : -1,
                        native::wheel_direction::vertical));
                return;
            }
            const native::mouse_button button =
                decode_button(event->xbutton.button);
            if (button != native::mouse_button::none) {
                owner->on_mouse_click.emit(native::mouse_event(
                    button,
                    event->type == ButtonPress
                        ? native::mouse_action::press
                        : native::mouse_action::release,
                    native::point(
                        event->xbutton.x,
                        event->xbutton.y - menu_height)));
            }
            break;
        }
        case MotionNotify:
            if (linux::wmaker::permit_input(owner) &&
                event->xmotion.y >= menu_height) {
                owner->on_mouse_move.emit(native::point(
                    event->xmotion.x,
                    event->xmotion.y - menu_height));
            }
            break;
        default:
            break;
        }
    }

    void handle_window_event(XEvent *event, void *client_data) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;
        const XEvent copy = *event;
        linux::wmaker::defer([owner, copy]() {
            dispatch_window_event(owner, copy);
        });
    }
} // namespace

namespace native
{
    void app_wnd::apply_title() {
        auto *window_state = linux::wmaker::state(this);
        if (!window_state || !window_state->window) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing window binding.");
        }
        WMSetWindowTitle(window_state->window, _title.c_str());
    }

    void app_wnd::create() const {
        if (_created)
            return;
        validate_owner_created();
        linux::wmaker::initialize();

        auto *self = const_cast<app_wnd *>(this);
        WMWindow *window = nullptr;
        if (app_wnd *owner = get_owner(); owner && get_modal()) {
            auto *owner_state = linux::wmaker::state(owner);
            if (!owner_state || !owner_state->window) {
                throw std::runtime_error(
                    "Window Maker/WINGs: modal owner is unavailable.");
            }
            window = WMCreatePanelForWindow(
                owner_state->window, "native_dialog");
        } else {
            window = WMCreateWindow(
                linux::wmaker::screen, "native_window");
        }
        if (!window) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a window.");
        }

        auto *window_state = new linux::wmaker::window_state;
        window_state->window = window;
        try {
            linux::wmaker::wnd_bindings.register_pair(window, self);
            linux::wmaker::window_bindings.register_pair(
                self, window_state);
        } catch (...) {
            linux::wmaker::wnd_bindings.unregister_by_handle(window);
            WMDestroyWidget(window);
            delete window_state;
            throw;
        }

        self->menu.attach(*self);
        const native::point position =
            linux::wmaker::constrain_position(_bounds.p, _bounds.d);
        WMSetWindowInitialPosition(window, position.x, position.y);
        WMResizeWidget(
            window,
            static_cast<unsigned int>(_bounds.d.w),
            static_cast<unsigned int>(
                _bounds.d.h + window_state->menu_height));
        WMSetWindowTitle(window, _title.c_str());

        // WINGs publishes the size it realized a window at and
        // nothing wider, so Window Maker offers no resize handles and
        // an installed layout is arranged exactly once. Publish a
        // usable range instead, the way the Win32, Cocoa, and SDL2
        // shells are resizable.
        WMSetWindowMinSize(window, 64, 48);
        WMSetWindowMaxSize(window, 32767, 32767);

        WMSetWindowCloseAction(window, close_window, self);
        WMCreateEventHandler(
            WMWidgetView(window),
            ExposureMask | StructureNotifyMask | FocusChangeMask |
                PointerMotionMask | ButtonPressMask |
                ButtonReleaseMask,
            handle_window_event,
            self);

        _created = true;
        self->on_native_move(position);
        self->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated window.");
        }
        auto *self = const_cast<app_wnd *>(this);
        auto *window_state = linux::wmaker::state(self);
        if (!window_state || !window_state->window) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing window binding.");
        }

        WMRealizeWidget(window_state->window);
        const Window target = WMWidgetXID(window_state->window);
        XSetWindowBackgroundPixmap(linux::wmaker::display,
                                   target,
                                   None);
        WMMapWidget(window_state->window);
        if (get_modal())
            WMRaiseWidget(window_state->window);
        linux::wmaker::schedule_repaint(
            self, native::rect({0, 0}, get_dimensions()));
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<app_wnd *>(this);
        auto *window_state = linux::wmaker::state(self);
        app_wnd *owner = get_owner();
        self->on_native_destroy();

        linux::wmaker::wnd_bindings.unregister_by_object(self);
        linux::wmaker::window_bindings.unregister_by_handle(self);
        if (window_state && window_state->window)
            WMDestroyWidget(window_state->window);
        delete window_state;

        if (get_modal() && owner && owner->get_created()) {
            app_wnd *focus = owner->get_input_enabled()
                                 ? owner
                                 : owner->get_active_modal();
            auto *focus_state = focus
                                    ? linux::wmaker::state(focus)
                                    : nullptr;
            if (focus_state && focus_state->window) {
                WMRaiseWidget(focus_state->window);
                const Window target =
                    WMWidgetXID(focus_state->window);
                if (target != None) {
                    XSetInputFocus(linux::wmaker::display,
                                   target,
                                   RevertToParent,
                                   CurrentTime);
                }
            }
        }
        if (self == app::main_wnd())
            linux::wmaker::exit_requested = true;
    }
} // namespace native
