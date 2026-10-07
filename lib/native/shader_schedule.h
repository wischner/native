//
// Declares peer-local shader resources and weak paced delivery endpoints.
// Workers queue invalidations but never access windows or graphics directly.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "glsl_shader.h"
#include "input_state.h"
#include <atomic>
#include <chrono>

namespace native::detail
{
    struct shader_schedule_entry
    {
        wnd *owner = nullptr;
        std::weak_ptr<wnd_lifetime> lifetime;
        std::atomic<bool> enabled{false};
        std::atomic<bool> pending{false};
    };
    struct shader_view_state
    {
        std::vector<shader_frame> history;
        std::unique_ptr<glsl_renderer> glsl;
        const shader_package_data *glsl_package = nullptr;
        std::unique_ptr<img> output;
        std::shared_ptr<shader_schedule_entry> schedule;
        shader_status status = shader_status::active;
        bool dirty = true, started = false, reported = false;
        std::chrono::steady_clock::time_point start, last;
    };
    // Register a weak endpoint with the single, stop-aware animation clock.
    std::shared_ptr<shader_schedule_entry> schedule_shader(
        wnd &owner, std::weak_ptr<wnd_lifetime> lifetime);
}
