//
// Declares a compact property inspector composed of standard Native
// editors. Property values are typed; labels and choice lists are text.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <memory>
#include <any>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "panel.h"

namespace native
{
    class canvas;

    enum class property_kind { text, number, boolean, choice, drop_down };
    using property_value = std::variant<std::string, double, bool, std::any>;

    // Supplies arbitrary popup content and the closed value's display text.
    struct property_drop_down
    {
        using commit = std::function<void(property_value)>;
        std::function<std::string(const property_value &)> to_text;
        // Return an uncreated control; the grid owns, parents, and sizes it.
        // A derived panel/canvas can create and own any number of children.
        std::function<std::unique_ptr<wnd>(const property_value &, commit)> create_content;
        size content_size = {240, 160};
    };

    struct property_item
    {
        std::string id;
        std::string label;
        property_kind kind = property_kind::text;
        property_value value = std::string{};
        std::vector<std::string> choices;
        bool read_only = false;
        std::shared_ptr<const property_drop_down> drop_down = {};
    };

    struct property_change
    {
        std::string id;
        property_value value;
    };

    // A two-column, vertically scrolling inspector with compact rows.
    class property_grid : public panel
    {
    public:
        // Construct an empty grid; zero row height selects font height + 6.
        property_grid(coord x = 0, coord y = 0,
                      dim width = 280, dim height = 240);

        // Construct an empty grid from a rectangle.
        explicit property_grid(const rect &bounds);

        // Release owned editors and the child host.
        ~property_grid() override;

        // Append a validated property with a unique nonempty ID.
        property_grid &add_item(property_item item);

        // Append-only construction sugar, equivalent to add_item().
        property_grid &operator<<(property_item item);

        // Replace the complete model atomically after validating every item.
        property_grid &set_items(std::vector<property_item> items);

        // Return a snapshot of the ordered property model.
        std::vector<property_item> get_items() const;

        // Return the property identified by ID; throws if it is absent.
        const property_item &get_item(const std::string &id) const;

        // Set a typed value silently; invalid types/choices/numbers throw.
        property_grid &set_value(const std::string &id, property_value value);

        // Set an optional explicit row height; zero restores the compact default.
        property_grid &set_row_height(int height);

        // Return the resolved row height in pixels.
        int get_row_height() const;

        // Set the label-column width; zero selects half the available width.
        property_grid &set_label_width(int width);

        // Return the configured label width; zero means automatic.
        int get_label_width() const;

        // Scroll by row index, clamped to the available property rows.
        property_grid &set_first_visible_row(std::size_t row);

        // Return the first displayed property row.
        std::size_t get_first_visible_row() const;

        // Commit a user value after validation; false rejects a read-only row.
        virtual bool on_native_value(const std::string &id, property_value value);

        // Emitted once after a valid user change; programmatic setters are silent.
        signal<property_change> on_change;

    protected:
        // Create the native panel and its standard child editors.
        void create_native() override;

        // Show the panel before its disjoint native painting regions.
        void show_native() override;

        // Destroy all editor resources before the panel.
        void destroy_native() override;

        // Lay out the scrolling viewport and compact editor rows.
        void on_bounds_changed() override;

        // Paint row labels and separators with the active control theme.
        virtual void draw_row(gpx &graphics, theme &appearance,
                              const rect &bounds, const property_item &item);

    private:
        struct row;
        std::vector<std::unique_ptr<row>> _rows;
        std::unique_ptr<canvas> _viewport;
        std::unique_ptr<canvas> _labels;
        std::unique_ptr<canvas> _top_frame;
        std::unique_ptr<canvas> _bottom_frame;
        int _row_height = 0;
        int _resolved_height = 20;
        int _label_width = 0;
        int _resolved_label_width = 0;
        bool _refreshing = false;

        // Find a model row or throw for an unknown ID.
        row &find_row(const std::string &id) const;
        // Resolve viewport geometry and create only fully visible editors.
        void refresh();
        // Paint the label column and its row separators.
        void paint_rows(wnd_paint_event event);
        // Construct the standard editor appropriate to a property's kind.
        void make_editor(row &item);
        // Copy the committed model value into its editor silently.
        void synchronize_editor(row &item);
    };
}
