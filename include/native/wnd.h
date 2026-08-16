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
    class radio;

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

        // Return whether this window may currently receive user input.
        virtual bool get_input_enabled() const;

        // Mark the complete client area for repainting.
        virtual wnd &invalidate() const;

        // Mark a client-area rectangle for repainting.
        virtual wnd &invalidate(const rect &invalid) const;

        // Accept a move notification from the native toolkit.
        void on_native_move(const point &position);

        // Accept destruction initiated by the native toolkit.
        virtual void on_native_destroy();

        //
        // Accept a resize notification from the native toolkit.
        //
        // Notes:
        //      Cached bounds and layout are updated without sending a
        //      resize request back to the operating system.
        //
        void on_native_resize(const size &dimensions);

        // Make an already-created native resource visible.
        virtual void show() const = 0;

        // Create the backend resource for this object.
        virtual void create() const = 0;

        // Destroy the backend resource for this object.
        virtual void destroy() const = 0;

        // Install an owning layout manager for child controls.
        wnd &set_layout(std::unique_ptr<layout_manager> layout);

        // Return the installed layout manager, or null when absent.
        layout_manager *get_layout() const;

        // Return the lazily created graphics context for this window.
        gpx &get_gpx() const;

        // Lifecycle, geometry, paint, and mouse notifications.
        signal<> on_wnd_create;
        signal<point> on_wnd_move;
        signal<size> on_wnd_resize;
        signal<wnd_paint_event> on_wnd_paint;
        signal<point> on_mouse_move;
        signal<mouse_event> on_mouse_click;
        signal<mouse_wheel_event> on_mouse_wheel;

    protected:
        // True after a backend resource has been created.
        mutable bool _created;

        rect _bounds;
        std::unique_ptr<layout_manager> _layout;
        mutable gpx *_gpx = nullptr;
        wnd *_parent;

        // Non-owning children; callers retain child object ownership.
        std::vector<wnd *> _children;

        // Release child backend resources before destroying a parent.
        void destroy_children() const;

        // Apply cached geometry to a created backend resource.
        virtual void apply_position();
        virtual void apply_dimensions();
        virtual void apply_bounds();

        // Apply the cached parent to a created backend resource.
        virtual void apply_parent();

    private:
        // Radio controls inspect siblings to enforce exclusive
        // selection.
        friend class radio;

    };

} // namespace native
