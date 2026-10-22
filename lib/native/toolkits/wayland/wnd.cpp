//
// Applies portable geometry, invalidation and cursors through GTK4.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../gpx_wnd.h"
#include "../../mouse_state.h"
namespace native
{
    void wnd::apply_position() {
        linux::wayland::geometry(*this);
    }
    void wnd::apply_dimensions() {
        linux::wayland::geometry(*this);
    }
    void wnd::apply_bounds() {
        linux::wayland::geometry(*this);
    }
    void wnd::apply_border_sides() {
        if (auto *peer = detail::peer_state<linux::wayland::widget_state>(*this);
            peer && peer->layout_children) peer->layout_children();
        invalidate();
    }
    void wnd::apply_parent() {
        linux::wayland::reparent(*this);
    }
    void wnd::apply_cursor() {
        static const char *names[] = {"default",
                                      "text",
                                      "crosshair",
                                      "ew-resize",
                                      "ns-resize",
                                      "nwse-resize",
                                      "nesw-resize",
                                      "none"};
        auto *peer =
            detail::peer_state<linux::wayland::widget_state>(*this);
        if (!peer || !peer->widget)
            return;
        gtk_widget_set_cursor_from_name(
            peer->widget, names[static_cast<int>(_cursor)]);
    }
    wnd &wnd::invalidate_native() {
        auto *peer =
            detail::peer_state<linux::wayland::widget_state>(*this);
        if (peer && peer->widget) {
            if (auto *editor = dynamic_cast<code_edit *>(this))
                linux::wayland::refresh_source(*editor);
        }
        if (peer && peer->widget)
            gtk_widget_queue_draw(
                peer->drawing && gtk_widget_get_visible(peer->drawing)
                    ? peer->drawing
                    : peer->widget);
        return *this;
    }
    wnd &wnd::invalidate_native(const rect &) {
        return invalidate_native();
    }
    gpx &wnd::get_gpx() {
        if (!_created)
            throw std::runtime_error(
                "Wayland graphics require a created window.");
        if (!_gpx)
            _gpx = new gpx_wnd(this);
        return *_gpx;
    }
} // namespace native
namespace native::detail
{
    mouse_capabilities backend_mouse_capabilities() {
        return {true, false, false, false};
    }
    bool backend_capture_mouse(wnd &,
                               mouse_capture_options,
                               std::string &error) {
        error = "Explicit pointer capture is unavailable in this GTK "
                "Wayland adapter.";
        return false;
    }
    void backend_release_mouse(wnd &, mouse_capture_mode) {}
    void backend_refresh_mouse(wnd &owner) {
        mouse_access::refresh(owner);
    }
} // namespace native::detail
