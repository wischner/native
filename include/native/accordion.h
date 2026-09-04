//
// Declares a portable stack of collapsible disclosure sections.
// Section state is owned by the accordion while application content
// windows remain borrowed and keep their normal Native lifecycle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "collection_view.h"

namespace native
{
    class accordion;
    class img;

    // Describes one accordion section for builder-style appending.
    struct accordion_section
    {
        // Construct a text-only section borrowing its content window.
        accordion_section(std::string title, wnd &content);

        // Construct a section borrowing an icon and content window.
        accordion_section(std::string title,
                          const img &icon,
                          wnd &content);

        std::string title;
        const img *icon;
        wnd *content;
    };

    namespace detail
    {
        void draw_accordion(accordion &control, gpx &graphics);
    }

    // Selects whether one or several accordion sections may be open.
    enum class accordion_mode
    {
        single,
        multiple
    };

    // Identifies a portable accordion-header keyboard command.
    enum class accordion_navigation
    {
        previous,
        next,
        first,
        last,
        toggle
    };

    // Stores the portable state of one accordion header and body.
    class accordion_item final
    {
    public:
        // Return the UTF-8 header title.
        const std::string &get_title() const;

        // Change the UTF-8 header title without emitting an action.
        accordion_item &set_title(std::string title);

        // Return whether this section is expanded.
        bool get_expanded() const;

        // Expand or collapse this section without emitting an action.
        accordion_item &set_expanded(bool expanded);

        // Return whether the header accepts user input.
        bool get_enabled() const;

        // Enable or disable header input without changing expansion.
        accordion_item &set_enabled(bool enabled);

        // Return the borrowed content window for this section.
        wnd &get_content() const;

        // Return the borrowed optional header icon, or null.
        const img *get_icon() const;

    private:
        friend class accordion;

        accordion_item(accordion &owner,
                       std::string title,
                       const img *icon,
                       wnd &content);

        accordion *_owner;
        std::string _title;
        const img *_icon;
        wnd *_content;
        dim _preferred_height;
        bool _expanded = false;
        bool _enabled = true;
    };

    // Presents vertically stacked headers and borrowed content bodies.
    class accordion : public collection_view
    {
    public:
        // Construct an empty single-expansion accordion from bounds.
        accordion(coord x = 0,
                  coord y = 0,
                  dim width = 240,
                  dim height = 320);

        // Construct an empty accordion from position and dimensions.
        accordion(const point &position, const size &dimensions);

        // Construct an empty accordion from complete bounds.
        explicit accordion(const rect &bounds);

        // Destroy native resources and detach every borrowed body.
        ~accordion() override;

        // Set single- or multiple-expansion behavior.
        accordion &set_mode(accordion_mode mode);

        // Return the current expansion behavior.
        accordion_mode get_mode() const;

        // Show or hide the complete outer control border.
        accordion &set_border_visible(bool visible);

        // Return whether the complete outer border is visible.
        bool get_border_visible() const;

        // Return the number of sections in display order.
        std::size_t get_item_count() const;

        // Return a section by index or throw std::out_of_range.
        accordion_item &get_item(std::size_t index) const;

        // Append a section with no header icon and borrow its content.
        accordion_item &add_item(const std::string &title,
                                 wnd &content);

        // Append a section borrowing both its icon and content window.
        // Both borrowed objects must outlive the section.
        accordion_item &add_item(const std::string &title,
                                 const img &icon,
                                 wnd &content);

        // Append one section descriptor.
        accordion &operator<<(accordion_section section);

        // Remove a section by index or throw std::out_of_range.
        accordion &remove_item(std::size_t index);

        // Remove all sections and detach their borrowed content.
        accordion &clear_items();

        // Return the first expanded index, or -1 when all are closed.
        int get_expanded_index() const;

        // Expand one index, or collapse all with -1, without a signal.
        accordion &set_expanded_index(int index);

        // Return the client-relative bounds of a section header.
        rect get_header_bounds(std::size_t index) const;

        // Return the client-relative body bounds for a section.
        rect get_content_bounds(std::size_t index) const;

        // Toggle a header after a backend-originated user action.
        virtual void on_native_toggle(std::size_t index);

        // Return the header currently carrying keyboard focus.
        int get_focused_index() const;

        // Cache backend focus entry or departure without a signal.
        void on_native_focus(bool focused) override;

        // Apply one backend-originated header navigation command.
        virtual void on_native_navigation(
            accordion_navigation navigation);

    protected:
        // Create the backend accordion resource.
        void create_native() override;

        // Destroy the backend accordion resource.
        void destroy_native() override;

        // Show the backend accordion resource.
        void show_native() override;

    public:

        // Emits the newly expanded index, or -1 after a collapse.
        signal<int> on_expanded_change;

    protected:
        // Recalculate body geometry after this control is resized.
        void on_bounds_changed() override;

        // Apply all item state to the created backend resource.
        virtual void apply_items();

        // Recalculate child placement and repaint the control.
        virtual void refresh();

        // Refresh dimensions from the current native theme.
        void synchronize_theme_metrics() override;

        // Draw the complete accordion background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one section header's native surface.
        virtual void draw_header_background(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const accordion_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one section header's native disclosure indicator.
        virtual void draw_header_disclosure(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const accordion_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one section header's optional image.
        virtual void draw_header_image(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const accordion_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one section header's text.
        virtual void draw_header_text(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const accordion_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one completed section header's focus and border.
        virtual void draw_header_border(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const accordion_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw the complete accordion border after every child part.
        virtual void draw_border(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

    private:
        friend class accordion_item;
        friend void detail::draw_accordion(
            accordion &control, gpx &graphics);

        accordion_mode _mode = accordion_mode::single;
        std::vector<std::unique_ptr<accordion_item>> _items;
        int _header_height = 24;
        int _focused_index = -1;
        bool _border_visible = true;

        void validate_index(int index, bool allow_none) const;
        void set_item_expanded(std::size_t index, bool expanded);
        void detach_item(accordion_item &item);
    };
} // namespace native
