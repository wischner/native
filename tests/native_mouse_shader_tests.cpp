//
// Exercises pointer transition/lifetime contracts and portable shader pixels.
// Tests use inert native hooks and real immutable packages, without a desktop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "mouse_state.h"
#include "shader_program.h"
#include "shader_schedule.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    void require(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }
    template <typename function_type>
    void rejects(function_type function, const char *message) {
        try { function(); } catch (const std::invalid_argument &) { return; }
        throw std::runtime_error(message);
    }
    native::shader_package package(std::string_view text) {
        return native::shader_package::decode(std::as_bytes(std::span(text)));
    }
    class root_window : public native::app_wnd
    {
    public:
        root_window() : app_wnd("probe", native::rect(0, 0, 80, 60)) {}
    protected:
        void create_native() override {}
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
        void apply_bounds() override {}
        void apply_position() override {}
        void apply_dimensions() override {}
    };
    class child_window : public native::wnd
    {
    public:
        explicit child_window(native::rect bounds) : wnd(bounds) {}
        bool enabled = true;
        bool get_input_enabled() const override { return enabled && wnd::get_input_enabled(); }
    protected:
        void create_native() override {}
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
        void apply_bounds() override {}
        void apply_position() override {}
        void apply_dimensions() override {}
    };
    class image_view : public native::shader_view
    {
    public:
        image_view() : shader_view(native::rect(0, 0, 2, 2)) {}
    protected:
        void create_native() override {
            native::detail::assign_peer_state(*this, new native::detail::shader_view_state);
        }
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
        void apply_bounds() override {}
        void apply_position() override {}
        void apply_dimensions() override {}
    };
    void pointer_boundaries() {
        using namespace native;
        root_window root;
        child_window first(rect(0, 0, 20, 20)), second(rect(20, 0, 20, 20));
        root.create(); root.show();
        first.set_parent(&root); second.set_parent(&root);
        first.create(); first.show(); second.create(); second.show();
        std::string order;
        first.on_mouse_enter.connect([&](point) { order += 'a'; return false; });
        first.on_mouse_leave.connect([&](point) { order += 'A'; return false; });
        second.on_mouse_enter.connect([&](point) { order += 'b'; return false; });
        second.on_mouse_leave.connect([&](point) { order += 'B'; return false; });
        detail::pointer_position(root, point(1, 1));
        detail::pointer_position(root, point(2, 2));
        detail::pointer_position(root, point(21, 1));
        require(order == "aAb", "Boundary transitions duplicated or out of order.");
        require(!first.get_mouse_inside() && second.get_mouse_inside(), "Hover cache mismatch.");
        detail::pointer_position(root, point(-1, -1));
        require(order == "aAbB", "Outside root kept hover.");
        first.set_cursor(mouse_cursor::hidden);
        require(first.get_cursor_at(point(1, 1)) == mouse_cursor::hidden, "Hidden policy lost.");
        require(first.get_cursor_at(point(-1, 1)) == mouse_cursor::arrow, "Outside hidden policy.");
        first.enabled = false;
        require(first.get_cursor_at(point(1, 1)) == mouse_cursor::arrow, "Disabled cursor hidden.");
        detail::pointer_position(root, point(1, 1));
        require(!root.get_mouse_inside() && !first.get_mouse_inside(), "Disabled child exposed parent.");
        first.enabled = true;
        int cancels = 0;
        first.on_mouse_cancel.connect([&](mouse_cancel_reason) { ++cancels; return false; });
        first.on_native_mouse_move(point(1, 1));
        first.on_native_mouse_click(mouse_event(mouse_button::left, mouse_action::press, point(1, 1)));
        detail::pointer_position(root, point(40, 40));
        require(cancels == 1, "Hover exit did not cancel held buttons once.");
        float delta = 99;
        first.on_mouse_motion.connect([&](mouse_motion_event event) { delta = event.dx; return false; });
        first.on_native_mouse_move(point(1, 1)); require(delta == 0, "Re-entry baseline jumped.");
        first.on_native_mouse_move(point(3, 1)); require(delta == 2, "Absolute delta mismatch.");
        std::string error;
        auto token = first.capture_mouse({mouse_capture_mode::relative, true}, error);
        require(!token && !error.empty(), "Unsupported raw input claimed success.");
        // Leave callbacks are allowed to destroy the next resource generation.
        first.on_mouse_leave.connect([&](point) { second.destroy(); return false; });
        detail::pointer_position(root, point(21, 1));
        require(!second.get_mouse_inside(), "Entered a destroyed next target.");
        first.destroy(); require(cancels == 1, "Destruction dispatched cancellation into teardown.");
        root.destroy();
    }
    void pointer_focus_transfer() {
        using namespace native;
        root_window root;
        child_window first(rect(0, 0, 20, 20)), second(rect(20, 0, 20, 20));
        root.create(); root.show();
        first.set_parent(&root); second.set_parent(&root);
        first.create(); first.show(); second.create(); second.show();
        int cancelled = 0;
        first.on_mouse_cancel.connect([&](mouse_cancel_reason reason) {
            require(reason == mouse_cancel_reason::focus_lost, "Wrong focus cancellation reason.");
            ++cancelled; return false;
        });
        first.on_native_focus(true);
        first.on_native_mouse_move(point(1, 1));
        first.on_native_mouse_click(mouse_event(mouse_button::left, mouse_action::press, point(1, 1)));
        second.on_native_focus(true);
        require(cancelled == 1 && !first.get_mouse_inside(),
            "Focus transfer retained previous pointer interaction.");
        first.destroy(); second.destroy(); root.destroy();
    }
    std::unique_ptr<native::img> run(const native::shader_package &effect,
        const native::img &source, std::vector<native::detail::shader_frame> &history) {
        std::map<std::string, native::shader_value, std::less<>> parameters;
        for (const auto &p : effect.get_parameters()) parameters[p.name] = p.default_value;
        return native::detail::execute_shader(native::detail::shader_package_data::get(effect),
            source, parameters, source.w(), source.h(), 1, 0, 0, 0, history);
    }
    void shader_pixels() {
        auto integer = package("native-image-1 1 parameters 1 parameter count 0 -2147483648 2147483648 2147483647 "
            "passes 1 pass identity source 1 0 20 1 sample 20 -1 0 end");
        require(std::get<int>(integer.get_parameters()[0].default_value) == 2147483647,
            "Integer default lost precision during decoding.");

        using namespace native;
        const auto identity = shader_package::load(
            std::filesystem::path(NATIVE_TEST_DATA) / "shaders/identity.nshader");
        img source(2, 2);
        source.pixels()[0] = rgba(255, 0, 0, 255);
        source.pixels()[1] = rgba(0, 255, 0, 255);
        source.pixels()[2] = rgba(0, 0, 255, 255);
        source.pixels()[3] = rgba(64, 128, 191, 128);
        std::vector<detail::shader_frame> history;
        auto output = run(identity, source, history);
        for (unsigned i = 0; i < 4; ++i)
            require(std::uint32_t(output->pixels()[i]) == std::uint32_t(source.pixels()[i]),
                "Identity changed pixel orientation/color/alpha.");
        const auto temporal = package("native-image-1 1 parameters 0 passes 1 "
            "pass decay source 1 1 23 4 sample 20 -1 0 sample 21 -2 0 "
            "const 22 0.5 0.5 0.5 0.5 mix 23 20 21 22 end");
        auto first = run(temporal, source, history);
        require(first->pixels()[0].a == 128 && first->pixels()[0].r == 255,
            "Initial history was not transparent/premultiplied.");
        source.pixels()[0] = rgba(0, 0, 0, 255);
        auto second = run(temporal, source, history);
        require(second->pixels()[0].a == 191 && second->pixels()[0].r > 150,
            "Completed-tick history did not advance.");
        const auto crt = shader_package::load(
            std::filesystem::path(NATIVE_TEST_DATA) / "shaders/crt.nshader");
        history.clear();
        auto processed = run(crt, source, history);
        require(processed->w() == 2 && processed->h() == 2 && history.size() == 3,
            "Multipass demo did not execute.");
        rejects([&] { package("native-image-1 2"); }, "Unknown version accepted.");
        rejects([&] { package("native-image-1 1 parameters 0 passes 1 pass bad source 1 0 20 1 sample 20 0 0 end"); },
            "Forward pass dependency accepted.");
        rejects([&] { package("native-image-1 1 parameters 0 passes 1 pass bad source 1 0 20 1 sample 20 -2 0 end"); },
            "Unretained history accepted.");
        rejects([&] { package("native-image-1 1 parameters 0 passes 1 pass bad source 1 0 20 1 add 20 50 0 end"); },
            "Uninitialized register accepted.");
        const auto huge = package("native-image-1 1 parameters 0 passes 1 pass huge viewport 1 1 20 1 sample 20 -1 0 end");
        rejects([&] { detail::validate_shader_extent(detail::shader_package_data::get(huge), 2, 2, 4096, 4096); },
            "Unbounded render targets accepted.");
    }
    void view_properties() {
        using namespace native;
        const auto effect = package("native-image-1 1 parameters 1 parameter gain 1 0 2 1 "
            "passes 1 pass gain source 1 0 21 2 sample 20 -1 0 mul 21 20 3 end");
        root_window root;
        image_view view;
        std::string error;
        require(view.set_effect(effect, error), "Pre-create package rejected.");
        require(view.get_shader_status() == shader_status::pending &&
            !view.get_shader_capabilities().image_profile_1, "Pre-create resources claimed.");
        rejects([&] { view.set_parameter("gain", 1); }, "Wrong parameter type accepted.");
        rejects([&] { view.set_parameter("gain", 3.0f); }, "Out-of-range parameter accepted.");
        root.create(); root.show(); view.set_parent(&root); view.create(); view.show();
        img source(2, 2), target(2, 2);
        source.pixels()[0] = rgba(255, 0, 0, 255);
        view.set_source(source, 7, 1.25);
        source.pixels()[0] = rgba(0, 255, 0, 255);
        view.on_native_paint(wnd_paint_event(rect(0, 0, 2, 2), target.get_gpx()));
        require(view.get_shader_status() == shader_status::active && target.pixels()[0].r == 255,
            "Source pixels were borrowed or effect was not painted.");
        const auto previous = view.get_effect();
        require(!view.set_effect(shader_package(), error) && view.get_effect().get_valid(),
            "Failed replacement lost a valid effect.");
        view.clear_effect();
        require(view.get_shader_status() == shader_status::no_effect, "Clear retained effect status.");
        view.destroy(); view.create(); view.show();
        require(view.get_source_sequence() == 7 && view.get_source_time() == 1.25,
            "Recreation lost portable source state.");
        view.destroy(); root.destroy();
        (void)previous;
    }
}
int main(int argc, char **) {
    try {
        pointer_boundaries(); pointer_focus_transfer(); shader_pixels(); view_properties();
        if (argc > 1) {
            auto effect = native::shader_package::load(
                std::filesystem::path(NATIVE_TEST_DATA) / "shaders/crt.nshader");
            for (auto dimensions : {native::size(256, 192), native::size(1920, 1080)}) {
                native::img source(dimensions.w, dimensions.h);
                std::vector<native::detail::shader_frame> history;
                auto start = std::chrono::steady_clock::now();
                auto result = run(effect, source, history);
                auto elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                std::cout << dimensions.w << 'x' << dimensions.h << " CRT CPU tick: "
                    << elapsed << " ms (includes linearization and output conversion)\n";
            }
        }
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
