//
// Exercises real OpenGL history, failed ticks and device/profile bounds.
// The test skips only when its platform has no usable OpenGL context.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "glsl_shader.h"
#include "glsl_context.h"
#include "shader_schedule.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void check(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    // Two passes: the first samples the later pass's retained previous
    // output.
    native::detail::glsl_pass pass(const char *name, int input, float red) {
        native::detail::glsl_pass result;
        result.name = name;
        result.history = true;
        result.viewport = true;
        result.vertex = "#version 330\nin vec4 qt_Vertex; void "
                        "main(){gl_Position=qt_Vertex;}";
        result.fragment =
            "#version 330\nlayout(std140) uniform ubuf { vec4 tint; };"
            "uniform sampler2D previous; out vec4 color;"
            "void main(){color=texture(previous,vec2(0.5))+tint;}";
        result.samplers.push_back({"previous", input, 0});
        result.uniforms.push_back({"tint", "increment", 4, {red, 0, 0, 0}});
        return result;
    }
    class inert_root final : public native::app_wnd
    {
    public:
        inert_root() : app_wnd("GLSL installation", 0, 0, 2, 2) {}

    protected:
        void create_native() override {}
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
    };
    class inert_view final : public native::shader_view
    {
    public:
        inert_view() : shader_view(native::rect(0, 0, 2, 2)) {}

    protected:
        void create_native() override {
            native::detail::assign_peer_state(
                *this, new native::detail::shader_view_state);
        }
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
    };
    // Install a real decoded package into an already-created view, then fail
    // a replacement while preserving the live program and output together.
    void live_installation() {
        const auto description = pass("identity", -1, 0);
        std::ostringstream text;
        text << "native-glsl-1 1 parameters 0 textures 0 passes 1 pass "
                "identity viewport 1 1 0 1 0 0 0 1 1 "
             << "vertex " << std::quoted(description.vertex) << " fragment "
             << std::quoted(description.fragment)
             << " uniforms 1 uniform tint value 4 0 0 0 0 samplers 1 sampler "
                "previous -1 0 end";
        const auto encoded = text.str();
        auto package =
            native::shader_package::decode(std::as_bytes(std::span(encoded)));
        inert_root root;
        inert_view view;
        root.create();
        view.set_parent(&root);
        view.create();
        native::img image(2, 2);
        view.set_source(image, 1, 0);
        std::string error;
        check(view.set_effect(package, error),
              "live GLSL installation failed");
        check(view.get_shader_status() == native::shader_status::active &&
                  view.get_shader_capabilities().glsl_profile_1,
              "installed status/capability mismatch");
        auto *state =
            native::detail::peer_state<native::detail::shader_view_state>(
                view);
        auto *renderer = state->glsl.get();
        auto *output = state->output.get();
        auto bad_text = encoded;
        const auto begin =
            bad_text.find("#version 330", bad_text.find("fragment"));
        bad_text.insert(begin, "INVALID GLSL ");
        const auto invalid = native::shader_package::decode(
            std::as_bytes(std::span(bad_text)));
        check(!view.set_effect(invalid, error),
              "broken GLSL replacement accepted");
        check(state->glsl.get() == renderer && state->output.get() == output,
              "failed installation discarded old resources");
        view.set_source(image, 2, 0.1);
        native::img target(2, 2);
        view.on_native_paint({native::rect(0, 0, 2, 2), target.get_gpx()});
        check(state->glsl.get() == renderer &&
                  view.get_shader_capabilities().glsl_profile_1,
              "next source recompiled a successfully prepared renderer");
        view.destroy();
        root.destroy();
    }
    void history_and_failure() {
        native::detail::shader_package_data data;
        data.glsl_passes = {pass("earlier", -3, 0), pass("later", 0, 0)};
        // Use a parameter binding so removing it fails in the later pass,
        // after the earlier pass has already written its scratch target.
        data.glsl_passes[0].uniforms[0].binding = "value";
        auto renderer = native::detail::make_glsl_renderer(data);
        native::img source(2, 2);
        std::map<std::string, native::shader_value, std::less<>> values;
        values["increment"] = std::array<float, 4>{0.25f, 0, 0, 0};
        auto render = [&](unsigned extent, bool reset) {
            return renderer->render(source, values, extent, extent, 0, 0, 0,
                                    reset);
        };
        auto first = render(2, true);
        check(first->pixels()[0].r == 64 && first->pixels()[0].a == 0,
              "later-pass history was not transparent black on first tick");
        auto second = render(2, false);
        check(second->pixels()[0].r == 128,
              "history did not advance on success");
        values.clear();
        bool failed = false;
        try {
            render(2, true);
        } catch (const std::out_of_range &) {
            failed = true;
        }
        check(failed, "later-pass fault was not exercised");
        values["increment"] = std::array<float, 4>{0.25f, 0, 0, 0};
        auto retry = render(2, false);
        check(retry->pixels()[0].r == 192,
              "failed reset tick changed committed history");
        auto resized = render(3, false);
        check(resized->pixels()[0].r == 64 && resized->pixels()[0].a == 0,
              "resize retained stale later-pass history");
        check(render(3, true)->pixels()[0].r == 64,
              "explicit reset retained history");
        bool oversized = false;
        try {
            render(4097, false);
        } catch (const std::invalid_argument &) {
            oversized = true;
        }
        check(oversized, "oversized viewport accepted");
        native::img wide(4097, 1);
        oversized = false;
        try {
            renderer->render(wide, values, 2, 2, 0, 0, 0, false);
        } catch (const std::invalid_argument &) {
            oversized = true;
        }
        check(oversized, "oversized source accepted");
        data.glsl_passes.resize(16, data.glsl_passes.front());
        auto bounded = native::detail::make_glsl_renderer(data);
        bool budget = false;
        try {
            bounded->render(source, values, 4096, 4096, 0, 0, 0, true);
        } catch (const std::invalid_argument &) {
            budget = true;
        }
        check(budget, "oversized graph allocation accepted");
    }
} // namespace
int main() {
    try {
        auto context = native::detail::make_glsl_context();
    } catch (const std::exception &error) {
        std::cout << "SKIP: " << error.what() << '\n';
        return 77;
    }
    try {
        history_and_failure();
        live_installation();
        std::cout << "GLSL history, retry and bounds passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
