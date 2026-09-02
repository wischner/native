//
// Declares common state and lifecycle behavior for native file chooser
// panels without exposing operating-system or toolkit handle types.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include "modal_wnd.h"

namespace native
{
    // Describes one named group of filename patterns in a chooser.
    struct file_filter
    {
        // Text shown for this group, such as "Images".
        std::string name;

        // Wildcard patterns such as "*.png" and "*.jpg".
        std::vector<std::string> patterns;
    };

    // Stores the portable state shared by open and save file dialogs.
    class file_dialog : public modal_wnd
    {
    public:
        // Destroy an active native panel and release its modal session.
        ~file_dialog() override;

        // Return the folder or path initially presented to the user.
        const std::string &get_initial_path() const;

        // Set the folder or path initially presented to the user.
        file_dialog &set_initial_path(const std::string &path);

        // Return the ordered filename filter groups.
        const std::vector<file_filter> &get_filters() const;

        // Replace all filename filter groups.
        file_dialog &set_filters(
            const std::vector<file_filter> &filters);

        // Append one filename filter group.
        file_dialog &add_filter(const file_filter &filter);

        // Remove every filename filter group.
        file_dialog &clear_filters();

        // Return the first selected path, or an empty string.
        const std::string &get_path() const;

        // Return all selected paths in chooser order.
        const std::vector<std::string> &get_paths() const;

        // Accept paths reported by a native panel callback.
        virtual void on_native_accept(
            const std::vector<std::string> &paths);

        // Cancel the active dialog from a native panel callback.
        virtual void on_native_cancel();

        // Prepare this logical panel after its owner has been created.
        void create() const override;

        // Close the native panel and release the logical resource.
        void destroy() const override;

    protected:
        // Construct common file-dialog state for an owner and title.
        file_dialog(app_wnd &owner, std::string title);

        // Start an owner-modal session; false means already active.
        bool begin_dialog() const;

    private:
        std::string _initial_path;
        std::vector<file_filter> _filters;
        std::vector<std::string> _paths;
        std::string _empty_path;

        // Close and release a panel supplied by the selected backend.
        void cancel_native_dialog() const;

        // System panels do not use ordinary window update hooks.
        void apply_position() override;
        void apply_dimensions() override;
        void apply_bounds() override;
        void apply_parent() override;
        void apply_title() override;
    };
} // namespace native
