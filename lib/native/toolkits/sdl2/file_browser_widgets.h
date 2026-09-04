//
// Declares compact private SDL2 file-browser widgets: flat semantic toolbar
// buttons and a clickable, horizontally eliding filesystem breadcrumb.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <native/button.h>
#include <native/canvas.h>
#include <native/signal.h>

namespace linux::sdl2
{
    // Identifies one compact file-browser toolbar command image.
    enum class file_browser_button_icon
    {
        back,
        forward,
        up
    };

    // Draws one compact icon-only command on the standard button peer.
    class file_browser_icon_button final : public native::button
    {
    public:
        // Construct one icon-only button at the supplied bounds.
        file_browser_icon_button(file_browser_button_icon icon,
                                 const native::rect &bounds);

    protected:
        // Paint a flat toolbar background with hover feedback.
        void draw_background(native::gpx &graphics,
                             native::theme &appearance,
                             const native::rect &bounds,
                             const native::theme::state &state) override;

        // Paint a border only while the command is hot or pressed.
        void draw_border(native::gpx &graphics,
                         native::theme &appearance,
                         const native::rect &bounds,
                         const native::theme::state &state) override;

        // Paint the semantic line icon instead of a text label.
        void draw_text(native::gpx &graphics,
                       native::theme &appearance,
                       const native::rect &bounds,
                       const native::theme::state &state) override;

    private:
        file_browser_button_icon _icon;
    };

    // Displays path components as a clickable horizontal breadcrumb trail.
    class file_browser_breadcrumb final : public native::canvas
    {
    public:
        // Construct an empty, non-scrolling breadcrumb bar.
        explicit file_browser_breadcrumb(const native::rect &bounds);

        // Replace the current normalized path and repaint its segments.
        file_browser_breadcrumb &set_path(
            const std::filesystem::path &path);

        // Emits the target path of a clicked breadcrumb component.
        native::signal<const std::filesystem::path &> on_navigate;

    private:
        struct segment
        {
            std::string label;
            std::filesystem::path target;
            native::rect bounds;
        };

        std::filesystem::path _path;
        std::vector<segment> _segments;

        void paint(native::wnd_paint_event event);
        bool click(native::mouse_event event);
    };
} // namespace linux::sdl2
