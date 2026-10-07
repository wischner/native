//
// Declares the shared pointer router and private backend acquisition hooks.
// Native handles never cross this interface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/mouse.h>
#include <native/wnd.h>
#include "input_state.h"

namespace native::detail
{
    struct mouse_state
    {
        bool inside = false;
        bool baseline = false;
        bool cancel_pending = false;
        bool active = true;
        bool suspended = false;
        point position;
        mouse_buttons buttons = mouse_buttons::none;
    };
    struct mouse_capture_state
    {
        wnd *owner = nullptr;
        std::weak_ptr<wnd_lifetime> lifetime;
        mouse_capture_options options;
        bool active = false;
    };
    // Access generation tokens and children without a new registry.
    class mouse_access
    {
    public:
        static std::weak_ptr<wnd_lifetime> lifetime(wnd &owner);
        static void refresh(wnd &owner);
        static void cancel_tree(wnd &root, mouse_cancel_reason reason,
                                bool notify = true);
    };
    // Resolve actual hover independently of captured event delivery.
    mouse_cursor pointer_cursor(wnd &owner, point local);
    void pointer_hover(wnd *target, point local);
    void pointer_leave(wnd &owner, point local);
    // Resolve one root client point, excluding chrome and child occlusion.
    void pointer_position(wnd &root, point position);
    // Cancel hover/capture on takeover; destruction does not notify.
    void pointer_cancel(wnd &owner, mouse_cancel_reason reason,
                        bool notify = true);
    // Return a live captured descendant of this root, or null.
    wnd *pointer_capture(wnd &root);
    // Report the capture mode without exposing lease storage.
    bool pointer_relative(wnd &root);
    // Clear before calling OS release to tolerate reentrant capture loss.
    void release_pointer(const std::shared_ptr<mouse_capture_state> &state);
    // Backend capabilities and atomic acquisition; failure leaves no state.
    mouse_capabilities backend_mouse_capabilities();
    bool backend_capture_mouse(wnd &owner, mouse_capture_options options,
                               std::string &error);
    void backend_release_mouse(wnd &owner, mouse_capture_mode mode);
    // Refresh native cursor resolution after normal release/geometry changes.
    void backend_refresh_mouse(wnd &owner);
}
