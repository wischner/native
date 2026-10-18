//
// Implements the SDL2 desktop-scale boundary: reads the X11 Xft.dpi
// resource once, converts SDL window, screen and pointer coordinates
// to logical pixels, and keeps each window renderer scaled to the
// measured ratio of output pixels to logical pixels.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "scale.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

#include <X11/Xlib.h>
#include <X11/Xresource.h>

namespace
{
    constexpr double reference_dpi = 96.0;
    constexpr double maximum_scale = 4.0;

    // Read Xft.dpi from the root window's resource database.
    // Returns 0 when the display or the resource is unavailable.
    double read_xft_dpi() {
        Display *display = XOpenDisplay(nullptr);
        if (!display)
            return 0.0;

        double dpi = 0.0;
        if (const char *resources = XResourceManagerString(display)) {
            XrmInitialize();
            XrmDatabase database = XrmGetStringDatabase(resources);
            if (database) {
                char *type = nullptr;
                XrmValue value{};
                if (XrmGetResource(database,
                                   "Xft.dpi",
                                   "Xft.Dpi",
                                   &type,
                                   &value) &&
                    value.addr)
                    dpi = std::strtod(value.addr, nullptr);
                XrmDestroyDatabase(database);
            }
        }
        XCloseDisplay(display);
        return dpi;
    }

    // Derive the coordinate factor for the active SDL video driver.
    double detect_scale() {
        const char *driver = SDL_GetCurrentVideoDriver();
        if (!driver || std::string_view(driver) != "x11")
            return 1.0;

        const double dpi = read_xft_dpi();
        if (!(dpi > 0.0))
            return 1.0;
        // Quarter steps keep near-96 values (for example 100 DPI) at 1
        // and make logical/device round trips stable.
        const double factor =
            std::round(dpi / reference_dpi * 4.0) / 4.0;
        return std::clamp(factor, 1.0, maximum_scale);
    }

    // Convert a pointer position in an event to logical pixels.
    void convert_point(Sint32 &x, Sint32 &y) {
        x = linux::sdl2::pointer_to_logical(x);
        y = linux::sdl2::pointer_to_logical(y);
    }
} // namespace

namespace linux::sdl2
{
    double coordinate_scale() {
        static double scale = 0.0;
        if (scale > 0.0)
            return scale;
        if (!SDL_WasInit(SDL_INIT_VIDEO))
            return 1.0;
        scale = detect_scale();
        return scale;
    }

    bool sdl_owns_scaling() {
        return coordinate_scale() == 1.0;
    }

    int to_sdl(int logical) {
        return static_cast<int>(
            std::lround(logical * coordinate_scale()));
    }

    int to_logical(int value) {
        return static_cast<int>(
            std::lround(value / coordinate_scale()));
    }

    int pointer_to_logical(int value) {
        return static_cast<int>(
            std::floor(value / coordinate_scale()));
    }

    native::size logical_window_size(SDL_Window *window) {
        int width = 0;
        int height = 0;
        if (window)
            SDL_GetWindowSize(window, &width, &height);
        return native::size(
            static_cast<native::dim>(std::max(0, to_logical(width))),
            static_cast<native::dim>(std::max(0, to_logical(height))));
    }

    native::point logical_mouse_state() {
        int x = 0;
        int y = 0;
        SDL_GetMouseState(&x, &y);
        return native::point(
            static_cast<native::coord>(pointer_to_logical(x)),
            static_cast<native::coord>(pointer_to_logical(y)));
    }

    native::point logical_global_mouse_state() {
        int x = 0;
        int y = 0;
        SDL_GetGlobalMouseState(&x, &y);
        return native::point(
            static_cast<native::coord>(pointer_to_logical(x)),
            static_cast<native::coord>(pointer_to_logical(y)));
    }

    bool poll_event(SDL_Event &event) {
        if (!SDL_PollEvent(&event))
            return false;
        if (coordinate_scale() == 1.0)
            return true;

        switch (event.type) {
        case SDL_MOUSEMOTION:
            convert_point(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            convert_point(event.button.x, event.button.y);
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_MOVED ||
                event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                event.window.data1 = to_logical(event.window.data1);
                event.window.data2 = to_logical(event.window.data2);
            }
            break;
        default:
            break;
        }
        return true;
    }

    void apply_render_scale(SDL_Window *window, SDL_Renderer *renderer) {
        if (!window || !renderer)
            return;

        int window_width = 0;
        int window_height = 0;
        int output_width = 0;
        int output_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        if (SDL_GetRendererOutputSize(
                renderer, &output_width, &output_height) != 0 ||
            window_width <= 0 || window_height <= 0 ||
            output_width <= 0 || output_height <= 0) {
            const float scale = static_cast<float>(coordinate_scale());
            SDL_RenderSetScale(renderer, scale, scale);
            return;
        }

        // Output pixels per SDL unit is SDL's own high-DPI factor (1
        // under X11); the coordinate scale is ours (1 under Wayland).
        // At most one of them differs from 1.
        const double factor = coordinate_scale();
        SDL_RenderSetScale(
            renderer,
            static_cast<float>(output_width * factor / window_width),
            static_cast<float>(output_height * factor / window_height));
    }
} // namespace linux::sdl2
