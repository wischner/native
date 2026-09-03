//
// Declares a portable paintable child surface. Applications provide all
// client-area painting through the inherited window paint event.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>

#include "scrollbar.h"
#include "theme.h"
#include "wnd.h"

namespace native
{
    // Stores a signed, large-range content-space scroll position.
    struct canvas_scroll_position
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
    };

    // Stores half-open content bounds independently of screen geometry.
    struct canvas_content_bounds
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    // Provides a concrete child window for application-defined painting.
    class canvas : public wnd
    {
    public:
        // Construct a canvas from scalar bounds.
        canvas(coord x = 0,
               coord y = 0,
               dim width = 320,
               dim height = 240);

        // Construct a canvas from a position and dimensions.
        canvas(const point &position, const size &dimensions);

        // Construct a canvas from complete bounds.
        explicit canvas(const rect &bounds);

        // Destroy the canvas and its native resource if it exists.
        ~canvas() override;

        // Set the scrollable bounds in application content pixels.
        canvas &set_content_bounds(canvas_content_bounds bounds);

        // Return the configured scrollable content bounds.
        canvas_content_bounds get_content_bounds() const;

        // Set and clamp the content position at the viewport's leading edge.
        canvas &set_scroll_position(canvas_scroll_position position);

        // Return the effective content position at the viewport's leading edge.
        canvas_scroll_position get_scroll_position() const;

        // Set horizontal scrollbar visibility policy.
        canvas &set_horizontal_scrollbar_policy(scrollbar_policy policy);

        // Return horizontal scrollbar visibility policy.
        scrollbar_policy get_horizontal_scrollbar_policy() const;

        // Set vertical scrollbar visibility policy.
        canvas &set_vertical_scrollbar_policy(scrollbar_policy policy);

        // Return vertical scrollbar visibility policy.
        scrollbar_policy get_vertical_scrollbar_policy() const;

        // Return whether each scrollbar currently reserves canvas space.
        bool get_horizontal_scrollbar_visible() const;
        bool get_vertical_scrollbar_visible() const;

        // Accept a backend-originated absolute scroll position.
        virtual void on_native_scroll(canvas_scroll_position position);

        // Paint the client viewport, non-client strips, and scrollbars.
        void on_native_paint(wnd_paint_event event) override;

        // Route pointer input to scrollbar chrome before the client.
        void on_native_mouse_move(const point &position) override;
        void on_native_mouse_click(mouse_event event) override;
        void on_native_mouse_wheel(mouse_wheel_event event) override;

        // Create the backend child drawing surface.
        void create() const override;

        // Destroy the backend child drawing surface.
        void destroy() const override;

        // Show an already-created canvas.
        void show() const override;

        // Emits the effective position after a user-originated scroll.
        signal<canvas_scroll_position> on_scroll;

    protected:
        // Reserve the visible scrollbar edges before rulers and client.
        rect get_chrome_bounds() const override;

        // Reclamp the position after a geometry change.
        void on_bounds_changed() override;

        // Refresh cached scrollbar extents from the active theme.
        virtual void synchronize_theme_metrics();

        // Draw one scrollbar track and thumb through the active theme.
        virtual void draw_scrollbar(gpx &graphics,
                                    const rect &track,
                                    const rect &thumb,
                                    scrollbar_orientation orientation,
                                    const theme::state &element_state);

    private:
        // Stores one resolved scrollbar and viewport arrangement.
        struct scroll_geometry
        {
            bool horizontal_visible = false;
            bool vertical_visible = false;
            rect viewport;
            rect horizontal_track;
            rect vertical_track;
            rect corner;
        };

        // Identifies the scrollbar part under a pointer position.
        enum class hit_part
        {
            none,
            horizontal_track,
            horizontal_thumb,
            vertical_track,
            vertical_thumb
        };

        // Resolve visibility, viewport, and track geometry together.
        scroll_geometry resolve_geometry() const;

        // Return the thumb inside a resolved track for one axis.
        rect thumb_bounds(const scroll_geometry &geometry,
                          scrollbar_orientation orientation) const;

        // Return the part occupied by a canvas-local pointer position.
        hit_part hit_test(const scroll_geometry &geometry,
                          const point &position) const;

        // Store a clamped position and report whether it moved.
        bool apply_scroll(canvas_scroll_position position);

        // Map a track pixel offset back to a content coordinate.
        std::int32_t position_from_track(
            const scroll_geometry &geometry,
            scrollbar_orientation orientation,
            int offset) const;

        canvas_content_bounds _content;
        canvas_scroll_position _scroll;
        scrollbar_policy _horizontal_policy = scrollbar_policy::automatic;
        scrollbar_policy _vertical_policy = scrollbar_policy::automatic;
        int _scrollbar_extent = 16;
        int _scrollbar_min_thumb = 16;
        hit_part _hot = hit_part::none;
        hit_part _pressed = hit_part::none;
        int _drag_offset = 0;
    };
} // namespace native
