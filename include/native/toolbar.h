//
// Declares menu-themed edge toolbars with momentary, toggle, and
// exclusive tools. Each application-owned bar reserves its own strip.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "non_client.h"
#include "events.h"
#include "signal.h"
#include "theme.h"

namespace native
{
    class canvas;
    class img;

    enum class tool_kind { button, toggle, exclusive, separator };

    struct tool_item
    {
        std::string id;
        std::string text;
        tool_kind kind = tool_kind::button;
        std::string group;
        bool checked = false;
        bool enabled = true;
        std::shared_ptr<const img> icon;
    };

    struct tool_command
    {
        std::string id;
        bool checked = false;
    };

    class toolbar : public non_client
    {
    public:
        // Attach another bar at any edge; extent is its strip thickness.
        explicit toolbar(wnd &owner, window_edge edge = window_edge::top,
                         int extent = 24);

        // Disconnect from the owner and release the input surface.
        ~toolbar() override;

        // Append a tool with a unique ID; separators may have an empty ID.
        toolbar &add_item(tool_item item);

        // Append a tool using construction syntax.
        toolbar &operator<<(tool_item item);

        // Replace all tools after validating IDs and exclusive groups.
        toolbar &set_items(std::vector<tool_item> items);

        // Return the ordered tool descriptors.
        const std::vector<tool_item> &get_items() const;

        // Set the image box, including 16x16, 24x24, or 32x32 pixels.
        // Grow or shrink the strip while retaining at least four pixels of padding.
        toolbar &set_icon_size(size dimensions);

        // Return the configured icon dimensions; the default is 16x16.
        size get_icon_size() const;

        // Enable or disable a tool silently.
        toolbar &set_enabled(const std::string &id, bool enabled);

        // Set a sticky tool silently, unchecking its exclusive peers.
        toolbar &set_checked(const std::string &id, bool checked);

        // Return a tool's checked state, or throw for an unknown ID.
        bool get_checked(const std::string &id) const;

        // Activate an enabled tool as a user action and emit one command.
        virtual bool on_native_command(const std::string &id);

        // Emitted after push activation or a sticky selection change.
        signal<tool_command> on_command;

    protected:
        // Retain the strip paint hook; the child canvas owns its painting.
        void draw(gpx &graphics, const rect &bounds) override;

        // Move or hide the input surface when the strip changes.
        void on_configuration_changed() override;

        // Draw a tool using the same primitive as a main-menu title.
        virtual void draw_tool(gpx &graphics, theme &appearance,
                               const rect &bounds, const tool_item &item,
                               const theme::state &state);

    private:
        std::vector<tool_item> _items;
        std::unique_ptr<canvas> _surface;
        std::string _pressed;
        int _hot = -1;
        size _icon_size{16, 16};

        // Create, relocate, or destroy the private input surface.
        void synchronize();
        // Invalidate both the strip and its native child surface.
        void repaint();
        // Paint menu chrome and the visible tools.
        void paint(wnd_paint_event event);
        // Track a press and activate only its matching release.
        void mouse_click(mouse_event event);
        // Update the drag target from a surface-local pointer position.
        void mouse_move(point position);
        // Resolve item rectangles, clipping overflow at the strip's end.
        std::vector<rect> item_bounds() const;
        // Return the enabled tool under the pointer, or -1.
        int item_at(point position) const;
        // Find a tool by ID or throw for an unknown ID.
        std::size_t find_item(const std::string &id) const;
    };
}
