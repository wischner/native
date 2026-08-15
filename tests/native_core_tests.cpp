//
// Tests backend-independent color, geometry, and signal behavior.
// The executable avoids display access so every hosted backend can run it.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

#include <native/control_paint.h>
#include <native/geometry.h>
#include <native/signal.h>
#include <bindings.h>

namespace
{
    static_assert(
        std::is_same_v<native::theme, native::control_paint>,
        "theme must expose the portable control painter");

    int failure_count = 0;

    // Record a failed condition without stopping the remaining tests.
    void expect(bool condition, const std::string &description) {
        if (condition)
            return;

        std::cerr << "FAILED: " << description << '\n';
        ++failure_count;
    }

    // Verify explicit channel packing and unpacking.
    void test_rgba() {
        constexpr std::uint32_t packed = 0x44332211U;
        constexpr native::rgba color(packed);

        expect(color.r == 0x11U, "packed color red channel");
        expect(color.g == 0x22U, "packed color green channel");
        expect(color.b == 0x33U, "packed color blue channel");
        expect(color.a == 0x44U, "packed color alpha channel");
        expect(
            static_cast<std::uint32_t>(color) == packed,
            "color round trip");
    }

    // Verify line containment and half-open rectangle operations.
    void test_geometry() {
        const native::line diagonal(0, 0, 10, 10);
        expect(
            diagonal.contains(native::point(5, 5)),
            "line contains a point on its segment");
        expect(
            !diagonal.contains(native::point(5, 6)),
            "line rejects a non-collinear point");
        expect(
            !diagonal.contains(native::point(11, 11)),
            "line rejects a collinear point beyond its endpoint");

        const native::rect first(0, 0, 10, 10);
        const native::rect second(5, 4, 10, 8);
        const native::rect overlap = first.intersect(second);
        expect(overlap.p.x == 5, "intersection left coordinate");
        expect(overlap.p.y == 4, "intersection top coordinate");
        expect(overlap.d.w == 5, "intersection width");
        expect(overlap.d.h == 6, "intersection height");
        expect(
            first.contains(native::point(9, 9)),
            "rectangle includes its final interior point");
        expect(
            !first.contains(native::point(10, 10)),
            "rectangle excludes its lower-right boundary");
    }

    // Verify lazy initialization, reverse order, and propagation stops.
    void test_signal() {
        int initialization_count = 0;
        native::signal<int> event(
            [&initialization_count]() {
                ++initialization_count;
            });

        int observed = 0;
        const int first_id = event.connect(
            [&observed](int value) {
                observed += value;
                return false;
            });
        event.connect(
            [&observed](int value) {
                observed += value * 10;
                return true;
            });

        event.emit(2);
        expect(initialization_count == 1, "signal initializes once");
        expect(observed == 20, "signal stops reverse-order dispatch");

        event.disconnect(first_id);
        event.disconnect_all();
        event.emit(2);
        expect(observed == 20, "disconnected signal has no callbacks");
    }

    // Verify replacing either side preserves a bijective binding.
    void test_bindings() {
        int first = 0;
        int second = 0;
        native::bindings<int, int *> registry;

        registry.register_pair(1, &first);
        registry.register_pair(2, &first);
        expect(
            registry.object_from_handle(1) == nullptr,
            "reusing an object removes its former handle");
        expect(
            registry.handle_from_object(&first) == 2,
            "reused object retains its current handle");

        registry.register_pair(2, &second);
        expect(
            registry.handle_from_object(&first) == 0,
            "reusing a handle removes its former object");
        expect(
            registry.object_from_handle(2) == &second,
            "reused handle retains its current object");
    }
}

int main() {
    test_rgba();
    test_geometry();
    test_signal();
    test_bindings();
    return failure_count == 0 ? 0 : 1;
}
