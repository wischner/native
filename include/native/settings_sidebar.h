//
// Declares settings-page navigation with stable IDs, optional images,
// stream-style construction, and borrowed page-window ownership.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "graphics.h"
#include "panel.h"

namespace native
{
    class tree_view;
    class list;
    class text_edit;

    struct settings_page
    {
        // Describe a positive stable ID and borrow an uncreated page window
        // that outlives this sidebar, or is explicitly removed before deletion.
        settings_page(int id, std::string title, wnd &content,
                      std::shared_ptr<const img> image = {},
                      std::string section = {}, bool enabled = true);
        int id;
        std::string title;
        wnd *content;
        std::shared_ptr<const img> image;
        std::string section;
        bool enabled;
    };

    class settings_sidebar : public panel
    {
    public:
        // Construct a settings navigator and page host from scalar bounds.
        settings_sidebar(coord x = 0, coord y = 0, dim width = 640, dim height = 400);
        // Construct from a position and dimensions.
        settings_sidebar(const point &position, const size &dimensions);
        // Construct from complete bounds.
        explicit settings_sidebar(const rect &bounds);
        // Destroy resources and detach borrowed pages without deleting them.
        ~settings_sidebar() override;
        // Append a page with a unique non-zero ID and unique content window.
        settings_sidebar &add_item(settings_page page);
        // Append a page using menu-style construction.
        settings_sidebar &operator<<(settings_page page);
        // Return the ordered page descriptors.
        const std::vector<settings_page> &get_items() const;
        // Find a descriptor by stable ID or throw out_of_range.
        const settings_page &get_item(int id) const;
        // Remove one page, destroy its native resource and detach its window.
        settings_sidebar &remove_item(int id);
        // Remove and detach every page without deleting C++ windows.
        settings_sidebar &clear_items();
        // Enable or disable a page; choose the first enabled page if needed.
        settings_sidebar &set_item_enabled(int id, bool enabled);
        // Change a page title silently.
        settings_sidebar &set_item_text(int id, std::string title);
        // Replace a page's optional image silently.
        settings_sidebar &set_item_image(int id, std::shared_ptr<const img> image);
        // Return the selected stable ID, or zero for no enabled page.
        int get_selected_item() const;
        // Select an enabled ID, or zero to clear, without emitting a signal.
        settings_sidebar &set_selected_item(int id);
        // Set a positive navigation width in logical pixels.
        settings_sidebar &set_sidebar_width(int width);
        // Return the requested navigation width.
        int get_sidebar_width() const;
        // Show an optional search field; hiding it suspends filtering.
        settings_sidebar &set_search_visible(bool visible);
        // Return whether sidebar search is shown (initially false).
        bool get_search_visible() const;
        // Filter titles and section labels without changing the active page.
        settings_sidebar &set_search_text(std::string text);
        // Return the retained UTF-8 query, including while search is hidden.
        const std::string &get_search_text() const;
        // Accept native edits, apply filtering, then notify observers.
        virtual void on_native_search(std::string text);
        // Emits user edits only; programmatic search setters remain silent.
        signal<std::string> on_search_change;
        // Return the page rectangle in this control's client coordinates.
        rect get_content_bounds() const;
        // Accept a distinct enabled user selection and emit its stable ID.
        virtual void on_native_selection(int id);
        // Emits the stable ID after user navigation; setters remain silent.
        signal<int> on_selection_change;

    protected:
        // Create native navigation or compose the backend's existing controls.
        void create_native() override;
        // Destroy navigation and pages, retaining descriptors for recreation.
        void destroy_native() override;
        // Resize navigation and the selected page.
        void on_bounds_changed() override;

    private:
        std::vector<settings_page> _items;
        std::unique_ptr<text_edit> _search;
        std::unique_ptr<tree_view> _navigation;
        std::unique_ptr<list> _list_navigation;
        std::vector<int> _navigation_ids;
        int _selected = 0;
        int _sidebar_width = 200;
        bool _search_visible = false;
        std::string _search_text;
        bool _refreshing = false;
        bool _items_dirty = true;
        void refresh();
        settings_page &find(int id);
        void choose_enabled();
    };
}
