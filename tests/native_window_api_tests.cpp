//
// Tests backend-neutral window properties and hierarchy behavior.
// Native resources are not created, so the test requires no display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <native.h>

namespace
{
    int failure_count = 0;

    // Record a failed condition without stopping the remaining tests.
    void expect(bool condition, const std::string &description) {
        if (condition)
            return;

        std::cerr << "FAILED: " << description << '\n';
        ++failure_count;
    }

    // Verify property caching and the setter/getter naming contract.
    void test_cached_properties() {
        native::app_wnd window(
            "Initial",
            native::point(10, 20),
            native::size(300, 200));

        window.set_position({30, 40})
            .set_dimensions({640, 480});
        window.set_title("Updated");

        expect(
            window.get_position().x == 30 &&
                window.get_position().y == 40,
            "window caches its position");
        expect(
            window.get_dimensions().w == 640 &&
                window.get_dimensions().h == 480,
            "window caches its dimensions");
        expect(
            window.get_title() == "Updated",
            "window caches its title");
        expect(
            !window.get_created(),
            "construction does not create a window");

        window.on_native_move({50, 60});
        window.on_native_resize({800, 600});
        expect(
            window.get_position().x == 50 &&
                window.get_position().y == 60,
            "native moves update the position cache");
        expect(
            window.get_dimensions().w == 800 &&
                window.get_dimensions().h == 600,
            "native resizes update the dimensions cache");

        window.set_layout(
            std::make_unique<native::absolute_layout_manager>());
        expect(
            window.get_layout() != nullptr,
            "window owns its layout");
    }

    // Verify parent links remain safe in either lifetime order.
    void test_parent_lifetime() {
        auto child = std::make_unique<native::button>(
            "Action",
            native::rect(0, 0, 80, 24));

        {
            auto parent = std::make_unique<native::app_wnd>(
                "Parent",
                native::rect(0, 0, 320, 200));
            child->set_parent(parent.get());
            expect(
                child->get_parent() == parent.get(),
                "child caches its parent");

            bool rejected_cycle = false;
            try {
                parent->set_parent(child.get());
            } catch (const std::invalid_argument &) {
                rejected_cycle = true;
            }
            expect(rejected_cycle, "parent hierarchy rejects cycles");
        }

        expect(
            child->get_parent() == nullptr,
            "destroying a parent detaches surviving children");
    }

    // Verify screen queries use only the current normalized snapshot.
    void test_screen_snapshot() {
        expect(native::screen::count() == 0,
               "screen snapshot starts empty");

        native::rect virtual_desktop = native::screen::virtual_bounds();
        expect(
            virtual_desktop.w() == 0 && virtual_desktop.h() == 0,
            "empty virtual bounds do not trigger native detection");

        native::screen clipped(
            7,
            native::rect(100, 100, 200, 100),
            native::rect(50, 50, 200, 100),
            false);
        expect(
            clipped.work_area().x1() == 100 &&
                clipped.work_area().y1() == 100 &&
                clipped.work_area().w() == 150 &&
                clipped.work_area().h() == 50,
            "screen clips its work area to its bounds");

        native::screen fallback(
            3,
            native::rect(10, 20, 100, 100),
            native::rect(),
            false);
        expect(
            fallback.work_area().x1() == fallback.bounds().x1() &&
                fallback.work_area().y1() == fallback.bounds().y1() &&
                fallback.work_area().w() == fallback.bounds().w() &&
                fallback.work_area().h() == fallback.bounds().h(),
            "screen falls back to bounds for an unavailable work area");
        expect(
            fallback.is_landscape(),
            "a square screen is landscape");
    }
}

int main() {
    test_cached_properties();
    test_parent_lifetime();
    test_screen_snapshot();
    return failure_count == 0 ? 0 : 1;
}
