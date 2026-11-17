//
// Owns a GEM window's colour composition and presenter overlay. Native
// pixels remain RGBA; the transport buffer uses the presenter's BGRA order.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native.h>

namespace linux::gemix
{
    struct color_surface
    {
        // Release the presenter layer before its AES window is deleted.
        ~color_surface();

        std::uint32_t overlay = 0;
        native::rect work;
        std::unique_ptr<native::img> pixels;
        std::vector<std::uint8_t> bgra;
    };

    // Allocate or resize the retained work-area composition.
    void prepare_color_surface(native::app_wnd &owner);

    // Present a changed work-local rectangle, retaining all other pixels.
    void present_color_surface(native::app_wnd &owner,
                               const native::rect &dirty);

    // Borrows the colour target for a window-local graphics operation.
    class color_target
    {
    public:
        // Copy the caller's drawing state and resolve its clipped origin.
        color_target(native::wnd &window, native::point screen_offset,
                     native::gpx &source);

        // Lazily enable colour presentation for a non-monochrome image.
        void enable_color();

        // Present colour drawing and report whether VDI is superseded.
        bool present();

        native::gpx *graphics = nullptr;
        native::point offset;

    private:
        native::app_wnd *_owner = nullptr;
        native::rect _clip;
    };
} // namespace linux::gemix
