//
// Declares the portable base window shared by top-level windows and
// controls. Backends implement native resource operations separately.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <vector>

#include "events.h"
#include "geometry.h"
#include "signal.h"

namespace native
{
    class gpx;
    class layout_manager;
    class non_client;
    class radio;
    class wnd;

    // Selects one of the portable system pointer shapes.
    enum class mouse_cursor
    {
        arrow,
        ibeam,
        crosshair,
        resize_horizontal,
        resize_vertical,
        resize_northwest_southeast,
        resize_northeast_southwest
    };

    namespace detail
    {
        class backend_wnd_peer;
        class wnd_peer_access;
        class wnd_peer;
        wnd *deepest_at(wnd &root, point position);
    }

    // Represents a cross-platform native window or child control.
    class wnd
    {
    public:
        // Construct a window from scalar position and size values.
        wnd(coord x = 100,
            coord y = 100,
            dim width = 640,
            dim height = 480);

        // Construct a window from a position and size.
        wnd(const point &position, const size &dimensions);

        // Construct a window from complete bounds.
        wnd(const rect &bounds);

        // Detach the window from its parent and release common state.
        virtual ~wnd();

        // Return the cached position in parent coordinates.
        point get_position() const;

        // Move the window and return it for call chaining.
        wnd &set_position(const point &position);

        // Return the cached client dimensions.
        size get_dimensions() const;

        // Resize the window and return it for call chaining.
        wnd &set_dimensions(const size &dimensions);

        // Return the cached position and dimensions.
        rect get_bounds() const;

        // Return the host-relative area available to laid-out children.
        // Visible non-client elements reserve space along its edges.
        rect get_client_bounds() const;

        // Move and resize the window in one operation.
        wnd &set_bounds(const rect &bounds);

        // Return the non-owning parent window, or null if unparented.
        wnd *get_parent() const;

        //
        // Assign a non-owning parent and update its child list.
        //
        // Throws:
        //      std::invalid_argument for a hierarchy cycle, an
        //      uncreated parent of a created window, or detaching a
        //      created control.
        //
        wnd &set_parent(wnd *parent);

        // Determine whether the backend resource currently exists.
        bool get_created() const;

        // Return whether show() has made the backend resource visible.
        bool get_visible() const;

        // Return the cached system pointer shape.
        mouse_cursor get_cursor() const;

        // Select a system pointer shape and return this window.
        wnd &set_cursor(mouse_cursor cursor);

        // Return whether this window may currently receive user input.
        virtual bool get_input_enabled() const;

        // Mark the complete client area for repainting.
        wnd &invalidate();

        // Mark a client-area rectangle for repainting.
        wnd &invalidate(const rect &invalid);

        // Accept creation completed by the native toolkit.
        virtual void on_native_create();

        // Accept a move notification from the native toolkit.
        virtual void on_native_move(const point &position);

        // Accept destruction initiated by the native toolkit.
        virtual void on_native_destroy();

        //
        // Accept a resize notification from the native toolkit.
        //
        // Notes:
        //      Cached bounds and layout are updated without sending a
        //      resize request back to the operating system.
        //
        //      A notification that repeats the cached dimensions is
        //      ignored. Backends report geometry through one event
        //      that also covers moves, so without this a window would
        //      relayout its children every time it was dragged.
        //
        virtual void on_native_resize(const size &dimensions);

        // Dispatch a backend paint notification to portable handlers.
        virtual void on_native_paint(wnd_paint_event event);

        // Dispatch a backend pointer-motion notification.
        virtual void on_native_mouse_move(const point &position);

        // Dispatch pointer motion with the matching screen position.
        // Backends should use this overload when the native event exposes
        // root/screen coordinates; the one-argument hook is still invoked.
        void on_native_mouse_move(const point &position,
                                  const point &screen_position);

        // Return the screen position from the latest pointer notification.
        point get_mouse_screen_position() const;

        // Dispatch a backend pointer-button notification.
        virtual void on_native_mouse_click(mouse_event event);

        // Dispatch a backend pointer-wheel notification.
        virtual void on_native_mouse_wheel(mouse_wheel_event event);

        // Dispatch a backend keyboard-focus transition.
        virtual void on_native_focus(bool focused);

        // Make an already-created native resource visible.
        void show();

        // Create the backend resource for this object.
        void create();

        // Destroy the backend resource for this object.
        void destroy();

        // Install an owning layout manager for child controls.
        wnd &set_layout(std::unique_ptr<layout_manager> layout);

        // Return the installed layout manager, or null when absent.
        layout_manager *get_layout() const;

        // Return the lazily created graphics context for this window.
        gpx &get_gpx();

        // Lifecycle, geometry, paint, and mouse notifications.
        signal<> on_wnd_create;
        signal<point> on_wnd_move;
        signal<size> on_wnd_resize;
        signal<wnd_paint_event> on_wnd_paint;
        signal<point> on_mouse_move;
        signal<mouse_event> on_mouse_click;
        signal<mouse_wheel_event> on_mouse_wheel;

    protected:
        // Create this window's backend-specific native resource.
        virtual void create_native() = 0;

        // Make this window's backend-specific resource visible.
        virtual void show_native() = 0;

        // Release this window's backend-specific native resource.
        virtual void destroy_native() = 0;

        // Schedule a complete backend repaint.
        virtual wnd &invalidate_native();

        // Schedule a backend repaint for one client rectangle.
        virtual wnd &invalidate_native(const rect &invalid);

        // True after a backend resource has been created.
        bool _created;
        bool _visible = false;
        bool _destroying = false;

        rect _bounds;
        std::unique_ptr<layout_manager> _layout;

        // True while a layout pass over this window's children runs,
        // or while cached geometry is being applied to the backend.
        bool _layout_suspended = false;
        gpx *_gpx = nullptr;
        std::unique_ptr<detail::wnd_peer> _peer;
        wnd *_parent;
        mouse_cursor _cursor = mouse_cursor::arrow;
        point _mouse_screen_position;
        bool _mouse_screen_position_exact = false;

        // Non-owning children; callers retain child object ownership.
        std::vector<wnd *> _children;

        // Release child backend resources before destroying a parent.
        void destroy_children();

        //
        // Run one layout pass over this window's children.
        //
        // Notes:
        //      Requests that arrive while a pass already runs are
        //      dropped. A backend may report a resize synchronously
        //      from inside the call that applied it, and geometry
        //      management in some toolkits resizes a parent when a
        //      child changes size. Either would otherwise re-enter
        //      this pass, at best repeating it and at worst not
        //      terminating.
        //
        void relayout_children();

        // React after cached client dimensions have changed.
        virtual void on_bounds_changed();

        // Apply cached geometry to a created backend resource.
        virtual void apply_position();
        virtual void apply_dimensions();
        virtual void apply_bounds();

        // Apply the cached parent to a created backend resource.
        virtual void apply_parent();

        // Apply the cached system pointer shape to the backend resource.
        virtual void apply_cursor();

        //
        // Return the host-relative area left for non-client elements
        // and the client after this window's own edge chrome.
        //
        // Notes:
        //      A plain window reserves nothing and returns its
        //      complete bounds. Controls which own permanent edge
        //      furniture, such as canvas scrollbars, subtract it
        //      here so both get_client_bounds() and non-client strip
        //      placement stay inside the remaining area.
        //
        virtual rect get_chrome_bounds() const;

        // Reduce a host-relative area by every visible non-client edge.
        rect reserve_non_client(const rect &available) const;

        // Paint visible non-client strips over their current bounds.
        void draw_non_client(gpx &graphics);

    private:
        friend class detail::backend_wnd_peer;
        friend class detail::wnd_peer_access;
        friend wnd *detail::deepest_at(wnd &root, point position);

        friend class non_client;

        // Radio controls inspect siblings to enforce exclusive
        // selection.
        friend class radio;

        // Non-owning edge elements; callers retain object ownership.
        std::vector<non_client *> _non_client;

        void attach_non_client(non_client *element);
        void detach_non_client(non_client *element);
        rect non_client_bounds(const non_client *element) const;

    };

} // namespace native
