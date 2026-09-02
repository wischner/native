//
// Declares a portable top-level application window, including its
// cached title, menu event surface, and independent owner graph.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include "menu.h"
#include "wnd.h"

namespace native
{
    class modal_wnd;
    class owned_wnd;

    // Represents a portable top-level application window.
    class app_wnd : public wnd
    {
    public:
        // Construct an application window from scalar bounds.
        app_wnd(std::string title,
                coord x = 100,
                coord y = 100,
                dim width = 640,
                dim height = 480);

        // Construct an application window from position and size.
        app_wnd(const std::string &title,
                const point &position,
                const size &dimensions);

        // Construct an application window from complete bounds.
        app_wnd(const std::string &title, const rect &bounds);

        // Destroy the application window and its native resource.
        ~app_wnd() override;

        // Return the cached window title.
        const std::string &get_title() const;

        // Change the title and update a created native window.
        app_wnd &set_title(const std::string &title);

        // Return the owning top-level window, or null for the main one.
        virtual app_wnd *get_owner() const;

        // Center an owned window over its owner; main windows are unchanged.
        app_wnd &center_to_parent();

        // Return whether this top-level window has modal semantics.
        virtual bool get_modal() const;

        // Return whether the native frame should provide its own title bar.
        // Compact tool windows can return false and paint one client caption.
        virtual bool get_native_title_visible() const;

        // Return whether ownership and modality permit user input.
        bool get_input_enabled() const override;

        // Return the currently active direct modal child, if any.
        modal_wnd *get_active_modal() const;

        // Reconcile destruction initiated by the native toolkit.
        void on_native_destroy() override;

        // Dispatch a command selected from the native menu.
        virtual void on_native_menu(int command);

        // Create the backend application window.
        void create() const override;

        // Destroy the backend application window.
        void destroy() const override;

        // Show the backend application window.
        void show() const override;

        // Menu model attached when the application window is created.
        main_menu menu;

        // Emits the command ID selected from the attached menu.
        signal<int> on_menu;

    protected:
        // Apply the cached title to a created backend window.
        virtual void apply_title();

    private:
        friend class modal_wnd;
        friend class owned_wnd;

        std::string _title;

        // Independent owned windows never participate in child layout.
        std::vector<owned_wnd *> _owned_windows;

        // The final entry is the direct modal window receiving input.
        std::vector<modal_wnd *> _modal_windows;

        // Register or unregister a borrowed independently owned window.
        void attach_owned_window(owned_wnd *window);
        void detach_owned_window(owned_wnd *window);

        // Update the modal stack for an owned dialog session.
        void begin_modal(modal_wnd *window);
        void end_modal(modal_wnd *window);

        // Destroy native resources of independent owned windows first.
        void destroy_owned_windows() const;

        // Reject native creation before an assigned owner exists.
        void validate_owner_created() const;

    };
} // namespace native
