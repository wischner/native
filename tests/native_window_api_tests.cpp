//
// Tests backend-neutral window properties and hierarchy behavior.
// Native resources are not created, so the test requires no display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <native.h>

namespace
{
    static_assert(std::is_same_v<
        decltype(std::declval<const native::font_t &>().get_metrics()),
        native::font_metrics>);
    static_assert(std::is_same_v<
        decltype(std::declval<const native::gpx &>().measure_text(
            std::declval<const std::string &>())),
        native::text_metrics>);

    int failure_count = 0;

    class recording_gpx final : public native::gpx
    {
    public:
        native::gpx &set_clip(const native::rect &bounds) override {
            clip = bounds;
            return *this;
        }

        native::rect get_clip() const override {
            return clip;
        }

        native::gpx &clear(native::rgba) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_line(native::point, native::point) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_rect(native::rect, bool) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_text(
            const std::string &,
            native::point) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_img(
            const native::img &,
            native::point) override {
            painted = true;
            return *this;
        }

        native::rect clip = native::rect(0, 0, 40, 20);
        bool painted = false;
    };

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

    // Verify the selected backend supplies a usable portable-target theme.
    void test_theme_factory() {
        recording_gpx graphics;
        auto painter = native::theme::create(graphics);

        expect(painter != nullptr, "backend creates a theme implementation");
        if (!painter)
            return;

        const native::theme::metrics metrics = painter->defaults();
        expect(metrics.menu_bar_height > 0, "theme reports usable metrics");

        painter->draw_menu_bar(native::rect(0, 0, 40, 20));
        expect(graphics.painted, "theme paints a portable graphics target");
    }

    // Verify image contexts and both supported encoded representations.
    void test_image_io() {
        const native::rgba source_color(32, 96, 208, 255);
        native::img source(8, 8);
        source.get_gpx().clear(source_color);
        for (std::size_t index = 0; index < 64; ++index) {
            expect(
                source.pixels()[index].r == source_color.r &&
                    source.pixels()[index].g == source_color.g &&
                    source.pixels()[index].b == source_color.b &&
                    source.pixels()[index].a == source_color.a,
                "image context begins with a full-image clip");
        }

        native::img clipped(4, 4);
        clipped.get_gpx().clear(native::rgba(0, 0, 0, 255))
            .set_clip(native::rect(1, 1, 2, 2))
            .clear(native::rgba(255, 255, 255, 255));
        int white_pixels = 0;
        for (std::size_t index = 0; index < 16; ++index) {
            if (clipped.pixels()[index].r == 255)
                ++white_pixels;
        }
        expect(
            white_pixels == 4,
            "image context uses half-open clipping boundaries");

        const native::rgba alpha_color(255, 0, 255, 79);
        source.pixels()[1] = alpha_color;
        const std::vector<std::uint8_t> png =
            source.encode(native::image_format::png);
        expect(
            png.size() >= 8 && png[0] == 0x89 && png[1] == 'P' &&
                png[2] == 'N' && png[3] == 'G',
            "PNG encoding has the expected signature");
        native::img png_copy = native::img::decode(png.data(), png.size());
        expect(
            png_copy.w() == source.w() && png_copy.h() == source.h(),
            "PNG memory round trip preserves dimensions");
        expect(
            static_cast<std::uint32_t>(png_copy.pixels()[0]) ==
                static_cast<std::uint32_t>(source_color),
            "PNG memory round trip preserves RGBA pixels");
        expect(
            static_cast<std::uint32_t>(png_copy.pixels()[1]) ==
                static_cast<std::uint32_t>(alpha_color),
            "PNG memory round trip preserves alpha");

        source.get_gpx().set_clip(native::rect(0, 0, 8, 8))
            .clear(source_color);
        const std::vector<std::uint8_t> jpeg =
            source.encode(native::image_format::jpeg, 95);
        expect(
            jpeg.size() >= 2 && jpeg[0] == 0xff && jpeg[1] == 0xd8,
            "JPEG encoding has the expected signature");
        native::img jpeg_copy = native::img::decode(jpeg.data(), jpeg.size());
        expect(
            jpeg_copy.w() == source.w() && jpeg_copy.h() == source.h(),
            "JPEG memory round trip preserves dimensions");
        const native::rgba jpeg_pixel = jpeg_copy.pixels()[0];
        expect(
            std::abs(static_cast<int>(jpeg_pixel.r) - source_color.r) <= 8 &&
                std::abs(static_cast<int>(jpeg_pixel.g) - source_color.g) <= 8 &&
                std::abs(static_cast<int>(jpeg_pixel.b) - source_color.b) <= 8 &&
                jpeg_pixel.a == 255,
            "JPEG memory round trip preserves an opaque solid color");

        const std::string path = "native-image-codec-test.png";
        source.save(path);
        native::img file_copy = native::img::load(path);
        std::remove(path.c_str());
        expect(
            file_copy.w() == source.w() && file_copy.h() == source.h(),
            "PNG file round trip preserves dimensions");

        bool rejected_quality = false;
        try {
            (void)source.encode(native::image_format::jpeg, 0);
        }
        catch (const std::invalid_argument &) {
            rejected_quality = true;
        }
        expect(rejected_quality, "JPEG encoding validates quality");

        const std::uint8_t broken_jpeg[] = {0xff, 0xd8};
        bool rejected_image = false;
        try {
            (void)native::img::decode(
                broken_jpeg, sizeof(broken_jpeg));
        }
        catch (const std::runtime_error &) {
            rejected_image = true;
        }
        expect(rejected_image, "image decoding rejects malformed input");

        bool rejected_character = false;
        native::font_t invalid;
        const native::font_metrics invalid_metrics = invalid.get_metrics();
        const native::text_metrics invalid_text = invalid.measure_text("x");
        expect(
            invalid_metrics.height == 0 &&
                invalid_metrics.max_advance == 0 &&
                invalid_text.width == 0 && invalid_text.advance == 0,
            "invalid fonts have empty measurements");
        try {
            (void)invalid.measure_character(0x110000);
        }
        catch (const std::invalid_argument &) {
            rejected_character = true;
        }
        expect(
            rejected_character,
            "font measurement rejects an invalid Unicode character");
    }
}

int main() {
    test_cached_properties();
    test_parent_lifetime();
    test_screen_snapshot();
    test_theme_factory();
    test_image_io();
    return failure_count == 0 ? 0 : 1;
}
