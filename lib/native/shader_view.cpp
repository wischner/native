//
// Implements cached shader-view properties and canvas composition.
// Programs and previous-frame outputs live in the existing window peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/shader_view.h>
#include "shader_program.h"
#include "shader_schedule.h"
#include "mouse_state.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace native
{
    shader_view::shader_view(const rect &bounds) : canvas(bounds) {
        set_horizontal_scrollbar_policy(scrollbar_policy::never);
        set_vertical_scrollbar_policy(scrollbar_policy::never);
    }
    shader_view::~shader_view() { destroy(); }
    void shader_view::create_native() {
        canvas::create_native();
        auto *state = new detail::shader_view_state;
        detail::assign_peer_state(*this, state);
        state->schedule = detail::schedule_shader(*this,
            detail::mouse_access::lifetime(*this));
        mark_shader_dirty();
    }
    void shader_view::show_native() { canvas::show_native(); }
    void shader_view::destroy_native() {
        auto *state = detail::peer_state<detail::shader_view_state>(*this);
        if (state && state->schedule) state->schedule->enabled = false;
        canvas::destroy_native();
    }
    shader_capabilities shader_view::get_shader_capabilities() const {
        if (!get_created()) return {};
        shader_capabilities caps{true, true, true, false, 4096, 8};
        auto *state = detail::peer_state<detail::shader_view_state>(*this);
        caps.glsl_profile_1 = state && bool(state->glsl);
        caps.accelerated = state && state->glsl && state->glsl->accelerated();
        if (caps.glsl_profile_1) caps.max_passes = 16;
        return caps;
    }
    const shader_package &shader_view::get_effect() const { return _effect; }
    bool shader_view::set_effect(const shader_package &effect, std::string &error) {
        error.clear();
        if (!effect.get_valid()) { error = "Cannot install an empty shader package."; return false; }
        std::map<std::string, shader_value, std::less<>> parameters;
        for (const auto &parameter : effect.get_parameters())
            parameters.emplace(parameter.name, parameter.default_value);
        std::unique_ptr<img> prepared;
        std::vector<detail::shader_frame> history;
        try {
            if (_source && get_created()) {
                const auto client = get_client_bounds();
                if (!effect._data->glsl_passes.empty()) {
                    auto renderer = detail::make_glsl_renderer(*effect._data);
                    prepared = renderer->render(*_source, parameters,
                        std::max(1u, unsigned(client.d.w)), std::max(1u, unsigned(client.d.h)),
                        _source_time, 0, 0, true);
                } else prepared = detail::execute_shader(*effect._data, *_source, parameters,
                    std::max(1u, unsigned(client.d.w)), std::max(1u, unsigned(client.d.h)),
                    _sequence, _source_time, 0, 0, history);
            }
        } catch (const std::exception &failure) { error = failure.what(); return false; }
        _effect = effect;
        _parameters.swap(parameters);
        clear_history();
        mark_shader_dirty();
        if (auto *state = detail::peer_state<detail::shader_view_state>(*this)) {
            state->glsl.reset();
            state->glsl_package = nullptr;
            state->output = std::move(prepared);
            state->history = std::move(history);
            state->status = shader_status::active;
            state->dirty = !state->output;
        }
        return true;
    }
    void shader_view::clear_effect() {
        _effect = shader_package();
        _parameters.clear();
        clear_history();
        mark_shader_dirty();
    }
    shader_view &shader_view::set_source(const img &image,
        std::uint64_t sequence, double source_time_seconds) {
        if (!std::isfinite(source_time_seconds) ||
            std::abs(source_time_seconds) > std::numeric_limits<float>::max())
            throw std::invalid_argument("Shader source time must fit a finite float.");
        auto source = std::make_shared<img>(image.w(), image.h());
        std::copy_n(image.pixels(), std::size_t(image.w()) * image.h(), source->pixels());
        if (_source && (_source->w() != image.w() || _source->h() != image.h()))
            clear_history();
        _source = std::move(source);
        _sequence = sequence;
        _source_time = source_time_seconds;
        mark_shader_dirty();
        return *this;
    }
    std::uint64_t shader_view::get_source_sequence() const { return _sequence; }
    double shader_view::get_source_time() const { return _source_time; }
    shader_view &shader_view::set_parameter(std::string_view name, shader_value value) {
        const auto &descriptors = _effect.get_parameters();
        const auto descriptor = std::find_if(descriptors.begin(), descriptors.end(),
            [&](const auto &parameter) { return parameter.name == name; });
        if (descriptor == descriptors.end() ||
            descriptor->default_value.index() != value.index())
            throw std::invalid_argument("Unknown shader parameter or wrong type.");
        const auto components = detail::shader_components(value);
        const unsigned count = std::max(1u, unsigned(value.index()));
        for (unsigned i = 0; i < count; ++i)
            if (components[i] < descriptor->minimum || components[i] > descriptor->maximum)
                throw std::invalid_argument("Shader parameter is outside its range.");
        _parameters.find(name)->second = std::move(value);
        mark_shader_dirty();
        return *this;
    }
    const shader_value &shader_view::get_parameter(std::string_view name) const {
        const auto parameter = _parameters.find(name);
        if (parameter == _parameters.end()) throw std::invalid_argument("Unknown shader parameter.");
        return parameter->second;
    }
    shader_view &shader_view::set_animation(shader_animation mode) {
        if (mode != shader_animation::source_updates && mode != shader_animation::continuous)
            throw std::invalid_argument("Invalid shader animation mode.");
        _animation = mode;
        clear_history();
        mark_shader_dirty();
        return *this;
    }
    shader_animation shader_view::get_animation() const { return _animation; }
    shader_view &shader_view::set_fallback(shader_fallback policy) {
        if (policy != shader_fallback::original_image && policy != shader_fallback::require_effect)
            throw std::invalid_argument("Invalid shader fallback policy.");
        _fallback = policy;
        invalidate();
        return *this;
    }
    shader_fallback shader_view::get_fallback() const { return _fallback; }
    shader_view &shader_view::set_fit(shader_fit policy) {
        if (policy != shader_fit::fit && policy != shader_fit::fill && policy != shader_fit::stretch)
            throw std::invalid_argument("Invalid shader fit policy.");
        _fit = policy;
        invalidate();
        return *this;
    }
    shader_fit shader_view::get_fit() const { return _fit; }
    rect shader_view::get_image_viewport() const {
        const rect client = get_client_bounds();
        if (!_source || _fit == shader_fit::stretch || !client.d.w || !client.d.h)
            return client;
        const double sx = double(client.d.w) / _source->w();
        const double sy = double(client.d.h) / _source->h();
        const double scale = _fit == shader_fit::fit ? std::min(sx, sy) : std::max(sx, sy);
        const auto width = static_cast<dim>(std::clamp(std::lround(_source->w() * scale), 1l, 32767l));
        const auto height = static_cast<dim>(std::clamp(std::lround(_source->h() * scale), 1l, 32767l));
        return rect(static_cast<coord>(client.p.x + (int(client.d.w) - width) / 2),
                    static_cast<coord>(client.p.y + (int(client.d.h) - height) / 2), width, height);
    }
    shader_status shader_view::get_shader_status() const {
        if (!_effect.get_valid()) return shader_status::no_effect;
        auto *state = detail::peer_state<detail::shader_view_state>(*this);
        return state ? state->status : shader_status::pending;
    }
    void shader_view::clear_history() {
        if (auto *state = detail::peer_state<detail::shader_view_state>(*this)) {
            state->history.clear();
            state->output.reset();
            state->started = false;
            state->dirty = true;
        }
        invalidate();
    }
    void shader_view::mark_shader_dirty() {
        if (auto *state = detail::peer_state<detail::shader_view_state>(*this)) {
            state->dirty = true;
            state->reported = false;
            if (state->schedule) state->schedule->enabled =
                _animation == shader_animation::continuous && _effect.get_valid() && bool(_source);
        }
        invalidate();
    }
    void shader_view::on_bounds_changed() {
        canvas::on_bounds_changed();
        clear_history();
        mark_shader_dirty();
    }
    void shader_view::draw_background(gpx &graphics, const rect &bounds) {
        graphics.set_ink(rgba(0, 0, 0, 255)).draw_rect(bounds, true);
    }
    void shader_view::draw_image(gpx &graphics, const img &image, const rect &viewport) {
        graphics.draw_img(image, viewport, image_filter::linear);
    }
    void shader_view::on_native_shader_error(const std::string &error) {
        on_shader_error.emit(error);
    }
    void shader_view::on_native_paint(wnd_paint_event event) {
        auto *state = detail::peer_state<detail::shader_view_state>(*this);
        if (!state) return;
        const auto weak = detail::mouse_access::lifetime(*this);
        const auto client = get_client_bounds();
        if (_source && _effect.get_valid() && client.d.w && client.d.h &&
            (state->dirty || _animation == shader_animation::continuous)) {
            const auto now = std::chrono::steady_clock::now();
            if (!state->started || now - state->last > std::chrono::milliseconds(250)) {
                state->history.clear();
                state->start = now;
                state->last = now;
                state->started = true;
            }
            const double elapsed = std::chrono::duration<double>(now - state->start).count();
            const double delta = std::chrono::duration<double>(now - state->last).count();
            try {
                std::unique_ptr<img> output;
                if (!_effect._data->glsl_passes.empty()) {
                    if (state->glsl_package != _effect._data.get()) {
                        state->glsl = detail::make_glsl_renderer(*_effect._data);
                        state->glsl_package = _effect._data.get();
                    }
                    output = state->glsl->render(*_source, _parameters, client.d.w, client.d.h,
                        _source_time, elapsed, delta, state->history.empty());
                    state->history.resize(1);
                } else output = detail::execute_shader(*_effect._data, *_source, _parameters,
                    client.d.w, client.d.h, _sequence, _source_time, elapsed, delta, state->history);
                state->output = std::move(output);
                state->last = now;
                state->status = shader_status::active;
                state->dirty = false;
                state->reported = false;
            } catch (const std::exception &failure) {
                state->status = shader_status::unavailable;
                state->output.reset();
                state->dirty = false;
                if (state->schedule) state->schedule->enabled = false;
                if (!state->reported) {
                    state->reported = true;
                    on_native_shader_error(failure.what());
                    auto lifetime = weak.lock();
                    if (!lifetime || !lifetime->alive) return;
                }
            }
        }
        {
            auto saved = event.g.save_state();
            event.g.set_clip(event.g.get_clip().intersect(client));
            draw_background(event.g, client);
            const img *image = state->output ? state->output.get() :
                (_fallback == shader_fallback::original_image ? _source.get() : nullptr);
            if (image) draw_image(event.g, *image, get_image_viewport());
        }
        canvas::on_native_paint(event);
    }
}
