//
// Declares the SDL2 desktop-scale boundary. Native geometry is in
// logical pixels; SDL window, screen and pointer coordinates may be
// device pixels. Exactly one layer applies the desktop factor: SDL
// itself when its video driver reports points (Wayland high-DPI), or
// this module when the driver works in device pixels (X11). The
// renderer scale is always measured from the real output size, never
// multiplied on top of either factor.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <SDL2/SDL.h>

#include <native/geometry.h>

namespace linux::sdl2
{
    //
    // Return the factor from logical pixels to SDL coordinates.
    //
    // Returns:
    //      Xft.dpi / 96 rounded to a quarter step and at least 1 under
    //      the X11 video driver, otherwise 1. The value is computed
    //      once after SDL video is initialized; 1 is returned (and not
    //      cached) before that.
    //
    // Notes:
    //      Xft.dpi is the factor the desktop publishes for X clients
    //      that scale themselves. Compositors that already upscale X
    //      windows publish 96 there, so the factor is never doubled.
    //
    double coordinate_scale();

    // Return whether SDL should allocate a high-DPI drawable itself.
    // False whenever this module applies a coordinate scale.
    bool sdl_owns_scaling();

    // Convert a logical length or screen position to SDL coordinates.
    int to_sdl(int logical);

    // Convert an SDL length or screen position to logical pixels.
    int to_logical(int value);

    // Convert an SDL pointer position to the logical pixel it lies in.
    int pointer_to_logical(int value);

    // Return a window's size in logical pixels.
    native::size logical_window_size(SDL_Window *window);

    // Return the focused-window pointer position in logical pixels.
    native::point logical_mouse_state();

    // Return the global pointer position in logical pixels.
    native::point logical_global_mouse_state();

    //
    // Poll one SDL event and convert its coordinates to logical pixels.
    //
    // Notes:
    //      Every backend event loop polls through this, so each queued
    //      event is converted exactly once. Relative motion deltas stay
    //      in device counts as the mouse contract requires.
    //
    bool poll_event(SDL_Event &event);

    //
    // Set a window renderer's scale to output pixels per logical pixel.
    //
    // Notes:
    //      Called whenever a window renderer is created and before each
    //      frame, so output-size changes (resize, moving to a display
    //      with another scale) take effect on the next paint.
    //
    void apply_render_scale(SDL_Window *window, SDL_Renderer *renderer);
} // namespace linux::sdl2
