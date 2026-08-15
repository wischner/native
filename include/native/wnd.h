//
// Declares windows, application windows, and button controls.
// Common state lives in these types while backends implement lifecycle
// and native resource operations in their platform source directories.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "events.h"
#include "geometry.h"
#include "menu.h"
#include "signal.h"

namespace native
{
    class gpx;
    class layout_manager;

    // Represents a cross-platform native window or child control.
    class wnd
    {
    public:
        // Construct a window from scalar position and size values.
        wnd(
            coord x = 100,
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
        //      std::invalid_argument for a hierarchy cycle, an uncreated
        //      parent of a created window, or detaching a created control.
        //
        wnd &set_parent(wnd *parent);

        // Determine whether the backend resource currently exists.
        bool get_created() const;

        // Mark the complete client area for repainting.
        virtual wnd &invalidate() const;

        // Mark a client-area rectangle for repainting.
        virtual wnd &invalidate(const rect &invalid) const;

        // Accept a move notification from the native toolkit.
        void on_native_move(const point &position);

        // Accept destruction initiated by the native toolkit.
        void on_native_destroy();

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

    private:
        // Apply cached geometry to a created backend resource.
        void apply_position();
        void apply_dimensions();
        void apply_bounds();

        // Apply the cached parent to a created backend resource.
        void apply_parent();
    };

    // Represents the process's top-level application window.
    class app_wnd : public wnd
    {
    public:
        // Construct an application window from scalar bounds.
        app_wnd(
            std::string title,
            coord x = 100,
            coord y = 100,
            dim width = 640,
            dim height = 480);

        // Construct an application window from position and size.
        app_wnd(
            const std::string &title,
            const point &position,
            const size &dimensions);

        // Construct an application window from complete bounds.
        app_wnd(const std::string &title, const rect &bounds);

        // Destroy the application-window interface.
        ~app_wnd() override;

        // Return the cached window title.
        const std::string &get_title() const;

        // Change the window title and update a created native window.
        app_wnd &set_title(const std::string &title);

        // Create the backend application window.
        void create() const override;

        // Destroy the backend application window.
        void destroy() const override;

        // Show the backend application window.
        void show() const override;

        main_menu menu;

        // Emits the command ID selected from the attached menu.
        signal<int> on_menu;

    private:
        std::string _title;

        // Apply the cached title to a created backend window.
        void apply_title();
    };

    // Represents a clickable native or emulated push button.
    class button : public wnd
    {
    public:
        // Construct a button from label and scalar bounds.
        button(
            std::string text,
            coord x = 0,
            coord y = 0,
            dim width = 96,
            dim height = 28);

        // Construct a button from label, position, and size.
        button(
            const std::string &text,
            const point &position,
            const size &dimensions);

        // Construct a button from a label and complete bounds.
        button(const std::string &text, const rect &bounds);

        // Destroy the button interface.
        ~button() override;

        // Return the cached button label.
        const std::string &get_text() const;

        // Change the label and update a created backend button.
        button &set_text(const std::string &text);

        // Create the backend button resource.
        void create() const override;

        // Destroy the backend button resource.
        void destroy() const override;

        // Show the backend button resource.
        void show() const override;

        // Emits when the user activates the button.
        signal<> on_click;

    private:
        std::string _text;

        // Apply the cached label to a created backend button.
        void apply_text();
    };
}
