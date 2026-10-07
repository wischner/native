//
// Declares an image-effect drawing leaf using Native's canvas host on every
// backend. Shader execution and properties use portable C++ values only.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "canvas.h"
#include "graphics.h"
#include "shader.h"
#include <cstdint>
#include <map>
#include <string_view>

namespace native
{
    // Displays programmable image effects without a graphics-API dependency.
    class shader_view : public canvas
    {
    public:
        // Construct an uncreated surface; default fit and original-image fallback.
        explicit shader_view(const rect &bounds = rect(0, 0, 320, 240));
        // Stop scheduled delivery and release resources before the canvas host.
        ~shader_view() override;
        // Return available execution resources; none before creation.
        shader_capabilities get_shader_capabilities() const;
        // Validate/cache a package, or install transactionally on a live view.
        // Failure keeps the previous effect and returns false with a diagnostic.
        bool set_effect(const shader_package &effect, std::string &error);
        // Return the cached immutable effect (empty when cleared).
        const shader_package &get_effect() const;
        // Remove the effect and reset its parameters and temporal state.
        void clear_effect();
        // Copy pixels before returning; finite source time is in seconds.
        shader_view &set_source(const img &image, std::uint64_t sequence = 0,
                                double source_time_seconds = 0);
        // Return the last submitted source identity.
        std::uint64_t get_source_sequence() const;
        // Return the last submitted finite source time.
        double get_source_time() const;
        // Validate an exact parameter type/range and invalidate without recompiling.
        shader_view &set_parameter(std::string_view name, shader_value value);
        // Return a cached typed value; unknown names throw invalid_argument.
        const shader_value &get_parameter(std::string_view name) const;
        // Select source-driven or shared, paced continuous animation.
        shader_view &set_animation(shader_animation mode);
        // Return the cached animation mode.
        shader_animation get_animation() const;
        // Select original pixels or a black unavailable surface on failure.
        shader_view &set_fallback(shader_fallback policy);
        // Return the cached fallback policy.
        shader_fallback get_fallback() const;
        // Select aspect-preserving fit/fill or stretch.
        shader_view &set_fit(shader_fit policy);
        // Return the cached fitting policy.
        shader_fit get_fit() const;
        // Return the local image rectangle; fill may extend beyond the client clip.
        rect get_image_viewport() const;
        // Return current installation status, independently of fallback pixels.
        shader_status get_shader_status() const;
        // Reset previous-frame inputs to transparent black and the time baseline.
        void clear_history();
        // Paint through virtual stages, then dispatch inherited paint/chrome.
        void on_native_paint(wnd_paint_event event) override;
        // Dispatch an execution diagnostic through the virtual event boundary.
        virtual void on_native_shader_error(const std::string &error);
        signal<std::string> on_shader_error;
    protected:
        // Create the ordinary backend canvas and its peer-owned executor.
        void create_native() override;
        // Show the ordinary backend canvas.
        void show_native() override;
        // Drop shader resources before releasing the backend surface.
        void destroy_native() override;
        // Reset viewport-dependent resources after resizing.
        void on_bounds_changed() override;
        // Fill the letterbox/unavailable surface; default opaque black.
        virtual void draw_background(gpx &graphics, const rect &bounds);
        // Draw the resolved shader output or original source.
        virtual void draw_image(gpx &graphics, const img &image,
                                const rect &viewport);
    private:
        shader_package _effect;
        std::shared_ptr<const img> _source;
        std::map<std::string, shader_value, std::less<>> _parameters;
        std::uint64_t _sequence = 0;
        double _source_time = 0;
        shader_animation _animation = shader_animation::source_updates;
        shader_fallback _fallback = shader_fallback::original_image;
        shader_fit _fit = shader_fit::fit;
        void mark_shader_dirty();
    };
}
