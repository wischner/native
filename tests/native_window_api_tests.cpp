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
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <native.h>

namespace
{
    static_assert(
        std::is_same_v<decltype(std::declval<const native::font_t &>()
                                    .get_metrics()),
                       native::font_metrics>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const native::gpx &>().measure_text(
                std::declval<const std::string &>())),
            native::text_metrics>);
    static_assert(std::is_base_of_v<native::app_wnd,
                                    native::owned_wnd>);
    static_assert(std::is_base_of_v<native::owned_wnd,
                                    native::modeless_wnd>);
    static_assert(std::is_base_of_v<native::owned_wnd,
                                    native::modal_wnd>);
    static_assert(std::is_base_of_v<native::modal_wnd,
                                    native::file_dialog>);
    static_assert(std::is_base_of_v<native::file_dialog,
                                    native::open_file_dialog>);
    static_assert(std::is_base_of_v<native::file_dialog,
                                    native::save_file_dialog>);
    static_assert(std::is_base_of_v<native::wnd, native::text_edit>);
    static_assert(!std::is_copy_constructible_v<native::clipboard>);
    static_assert(std::is_move_constructible_v<native::clipboard>);

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

        native::gpx &draw_native_text(
            const std::string &,
            native::point) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_img(const native::img &,
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
            "Initial", native::point(10, 20), native::size(300, 200));

        window.set_position({30, 40}).set_dimensions({640, 480});
        window.set_title("Updated");

        expect(window.get_position().x == 30 &&
                   window.get_position().y == 40,
               "window caches its position");
        expect(window.get_dimensions().w == 640 &&
                   window.get_dimensions().h == 480,
               "window caches its dimensions");
        expect(window.get_title() == "Updated",
               "window caches its title");
        expect(!window.get_created(),
               "construction does not create a window");

        window.on_native_move({50, 60});
        window.on_native_resize({800, 600});
        expect(window.get_position().x == 50 &&
                   window.get_position().y == 60,
               "native moves update the position cache");
        expect(window.get_dimensions().w == 800 &&
                   window.get_dimensions().h == 600,
               "native resizes update the dimensions cache");

        window.set_layout(
            std::make_unique<native::absolute_layout_manager>());
        expect(window.get_layout() != nullptr,
               "window owns its layout");
    }

    // Verify parent links remain safe in either lifetime order.
    void test_parent_lifetime() {
        auto child = std::make_unique<native::button>(
            "Action", native::rect(0, 0, 80, 24));

        {
            auto parent = std::make_unique<native::app_wnd>(
                "Parent", native::rect(0, 0, 320, 200));
            child->set_parent(parent.get());
            expect(child->get_parent() == parent.get(),
                   "child caches its parent");

            bool rejected_cycle = false;
            try {
                parent->set_parent(child.get());
            } catch (const std::invalid_argument &) {
                rejected_cycle = true;
            }
            expect(rejected_cycle, "parent hierarchy rejects cycles");
        }

        expect(child->get_parent() == nullptr,
               "destroying a parent detaches surviving children");
    }

    // Verify that independent ownership never enters child layout and
    // remains safe when C++ objects are destroyed in either order.
    void test_owned_window_lifetime() {
        auto owner = std::make_unique<native::app_wnd>(
            "Owner", native::rect(0, 0, 320, 200));
        auto modeless = std::make_unique<native::modeless_wnd>(
            *owner, "Palette", native::rect(360, 0, 180, 240));
        auto modal = std::make_unique<native::modal_wnd>(
            *owner, "Dialog", native::rect(80, 60, 200, 120));

        expect(modeless->get_owner() == owner.get() &&
                   modal->get_owner() == owner.get(),
               "independent windows cache their top-level owner");
        expect(modeless->get_parent() == nullptr &&
                   modal->get_parent() == nullptr,
               "independent windows are not layout children");
        expect(!modeless->get_modal() && modal->get_modal(),
               "owned window types expose distinct modal semantics");
        expect(owner->get_input_enabled() &&
                   modeless->get_input_enabled() &&
                   modal->get_input_enabled(),
               "unshown owned windows do not block input");

        bool rejected_result = false;
        try {
            modal->close(native::dialog_result::none);
        } catch (const std::invalid_argument &) {
            rejected_result = true;
        }
        expect(rejected_result,
               "modal close requires a final dialog result");

        owner.reset();
        expect(modeless->get_owner() == nullptr &&
                   modal->get_owner() == nullptr,
               "destroying an owner detaches surviving owned windows");
    }

    // Verify file-dialog properties remain portable before a native
    // system panel is created.
    void test_file_dialog_properties() {
        native::app_wnd owner(
            "Owner", native::rect(0, 0, 320, 200));
        native::open_file_dialog open(owner, "Choose Source");
        open.set_initial_path("/documents")
            .add_filter({"Images", {"*.png", "*.jpg"}})
            .add_filter({"Text", {"*.txt"}});
        open.set_allow_multiple(true);

        expect(open.get_owner() == &owner && open.get_modal(),
               "open dialog is an owner-modal object");
        expect(open.get_initial_path() == "/documents" &&
                   open.get_filters().size() == 2 &&
                   open.get_filters()[0].patterns.size() == 2,
               "open dialog caches its path and filter groups");
        expect(open.get_allow_multiple(),
               "open dialog caches multiple-selection state");
        expect(open.get_path().empty() && open.get_paths().empty() &&
                   open.get_result() == native::dialog_result::none,
               "unshown open dialog has no selection or result");

        open.clear_filters();
        expect(open.get_filters().empty(),
               "file dialog clears its filters");

        native::save_file_dialog save(owner, "Export");
        save.set_suggested_name("drawing")
            .set_default_extension("png")
            .set_confirm_overwrite(false);
        save.set_initial_path("/exports");
        save.set_filters({{"PNG image", {"*.png"}}});

        expect(save.get_suggested_name() == "drawing" &&
                   save.get_default_extension() == "png" &&
                   !save.get_confirm_overwrite(),
               "save dialog caches filename and overwrite options");
        expect(save.get_initial_path() == "/exports" &&
                   save.get_filters().size() == 1,
               "save dialog shares path and filter state");

        open.on_native_accept({"ignored.txt"});
        save.on_native_cancel();
        expect(open.get_paths().empty() &&
                   open.get_result() == native::dialog_result::none &&
                   save.get_result() == native::dialog_result::none,
               "inactive dialogs ignore stale native completion");
    }

    // Verify portable selection state before native resources are
    // created.
    void test_selection_controls() {
        native::app_wnd parent("Controls",
                               native::rect(0, 0, 320, 240));

        native::check enabled("Enabled");
        int check_events = 0;
        bool last_check_value = true;
        enabled.on_change.connect([&](bool checked) {
            ++check_events;
            last_check_value = checked;
            return false;
        });
        enabled.set_parent(&parent).set_bounds(
            native::rect(8, 8, 120, 24));
        enabled.set_checked(true);
        expect(enabled.get_checked(),
               "check caches programmatic state");
        expect(check_events == 0,
               "programmatic check changes do not emit");
        enabled.on_native_checked(false);
        expect(
            !enabled.get_checked() && check_events == 1 &&
                !last_check_value,
            "native check changes update state and emit their value");

        native::radio first("First");
        native::radio second("Second");
        first.set_parent(&parent);
        second.set_parent(&parent);
        first.set_selected(true);
        second.on_native_selected();
        expect(!first.get_selected() && second.get_selected(),
               "sibling radios are mutually exclusive");

        native::list choices({"One", "Two", "Three"});
        int selected = -1;
        choices.on_selection_change.connect([&](int index) {
            selected = index;
            return false;
        });
        choices.set_parent(&parent).set_bounds(
            native::rect(8, 64, 160, 80));
        choices.set_selected_index(1);
        expect(choices.get_selected_index() == 1 && selected == -1,
               "list caches programmatic selection without emitting");
        choices.on_native_selection(2);
        expect(choices.get_selected_index() == 2 && selected == 2,
               "native list selection updates state and emits");
        choices.remove_item(0);
        expect(choices.get_selected_index() == 1,
               "list selection follows an item removed before it");
    }

    // Verify editor modes, cached values, and complete-value validation
    // without creating a backend widget.
    void test_text_edit_properties() {
        native::text_edit single("123");
        int changes = 0;
        single.on_change.connect([&](const std::string &) {
            ++changes;
            return false;
        });
        single.set_validator([](const std::string &text) {
            return text.size() <= 4;
        });
        expect(single.validate("1234") &&
                   !single.validate("12345") &&
                   !single.validate("one\ntwo") &&
                   !single.validate(std::string("a\0b", 3)),
               "single-line editors validate complete proposed values");
        expect(single.on_native_text("1234") &&
                   !single.on_native_text("12345") &&
                   single.get_text() == "1234" && changes == 1,
               "rejected native edits preserve cached editor text");
        single.set_text("12");
        expect(single.get_text() == "12" && changes == 1,
               "programmatic editor changes do not emit on_change");
        single.set_read_only(true);
        expect(single.get_read_only() &&
                   !single.on_native_text("13") &&
                   single.get_text() == "12" && changes == 1,
               "read-only editors reject native changes");

        native::text_edit multiline(
            "one\ntwo", native::text_edit_mode::multi_line);
        expect(multiline.get_mode() ==
                       native::text_edit_mode::multi_line &&
                   multiline.validate("three\nfour"),
               "multiline editors accept portable line feeds");
    }

    // Verify screen queries use only the current normalized snapshot.
    void test_screen_snapshot() {
        expect(native::screen::count() == 0,
               "screen snapshot starts empty");

        native::rect virtual_desktop = native::screen::virtual_bounds();
        expect(virtual_desktop.w() == 0 && virtual_desktop.h() == 0,
               "empty virtual bounds do not trigger native detection");

        native::screen clipped(7,
                               native::rect(100, 100, 200, 100),
                               native::rect(50, 50, 200, 100),
                               false);
        expect(clipped.work_area().x1() == 100 &&
                   clipped.work_area().y1() == 100 &&
                   clipped.work_area().w() == 150 &&
                   clipped.work_area().h() == 50,
               "screen clips its work area to its bounds");

        native::screen fallback(
            3, native::rect(10, 20, 100, 100), native::rect(), false);
        expect(
            fallback.work_area().x1() == fallback.bounds().x1() &&
                fallback.work_area().y1() == fallback.bounds().y1() &&
                fallback.work_area().w() == fallback.bounds().w() &&
                fallback.work_area().h() == fallback.bounds().h(),
            "screen falls back to bounds for an unavailable work area");
        expect(fallback.is_landscape(), "a square screen is landscape");
    }

    // Verify the selected backend supplies a usable portable-target
    // theme.
    void test_theme_factory() {
        recording_gpx graphics;
        auto painter = native::theme::create(graphics);

        expect(painter != nullptr,
               "backend creates a theme implementation");
        if (!painter)
            return;

        const native::theme::metrics metrics = painter->defaults();
        expect(metrics.menu_bar_height > 0 &&
                   metrics.check_height > 0 &&
                   metrics.radio_height > 0 &&
                   metrics.list_item_height > 0,
               "theme reports usable control metrics");

        painter->draw_menu_bar(native::rect(0, 0, 40, 20));
        native::theme::state selected;
        selected.selected = true;
        painter->draw_check(
            native::rect(0, 0, 80, 20), "Check", selected);
        painter->draw_radio(
            native::rect(0, 0, 80, 20), "Radio", selected);
        painter->draw_list(
            native::rect(0, 0, 80, 40), {"First", "Second"}, 1);
        painter->draw_text_edit_frame(
            native::rect(0, 0, 80, 24), selected);
        expect(graphics.painted,
               "theme paints a portable graphics target");
    }

    // Verify installed-font discovery and byte-identical portable font
    // creation, measurement, movement, and drawing.
    void test_portable_fonts() {
        constexpr native::font_role roles[] = {
            native::font_role::system,
            native::font_role::fixed,
            native::font_role::icon_label,
            native::font_role::title,
            native::font_role::small,
            native::font_role::control};
        for (native::font_role role : roles) {
            const native::font_t &stock = native::font_t::stock(role);
            const native::font_metrics metrics = stock.get_metrics();
            expect(stock.valid() && stock.spec().source ==
                       native::font_source::stock,
                   "every semantic stock-font role is valid");
            expect(metrics.ascent > 0 && metrics.descent > 0 &&
                       metrics.leading > 0 && metrics.height > 0 &&
                       metrics.max_advance > 0,
                   "stock fonts expose positive editor metrics");
        }

        const std::uint8_t malformed[] = {0, 1, 2, 3};
        native::font_t invalid = native::font_t::from_memory(
            malformed, sizeof(malformed), 16);
        expect(!invalid.valid(), "malformed font data is rejected");

        const std::vector<native::font_description> installed =
            native::font_t::enumerate_installed();
        expect(!installed.empty(), "installed fonts can be enumerated");

        native::font_t from_file;
        std::vector<std::uint8_t> bytes;
        for (const native::font_description &description : installed) {
            from_file = native::font_t::from_file(
                description.path, 18, description.face_index);
            if (!from_file.valid())
                continue;

            std::ifstream stream(description.path, std::ios::binary);
            bytes = std::vector<std::uint8_t>{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream.bad() && !bytes.empty())
                break;
            from_file = native::font_t();
        }
        expect(from_file.valid(),
               "an enumerated font can be created from its file");
        if (!from_file.valid())
            return;

        native::font_t from_memory = native::font_t::from_memory(
            bytes.data(),
            bytes.size(),
            18,
            from_file.spec().face_index);
        expect(from_memory.valid(),
               "the same font can be created from copied memory");
        if (!from_memory.valid())
            return;

        const native::font_metrics file_metrics =
            from_file.get_metrics();
        const native::font_metrics memory_metrics =
            from_memory.get_metrics();
        expect(file_metrics.height > 0 && file_metrics.ascent > 0 &&
                   file_metrics.descent > 0,
               "portable fonts expose positive editor metrics");
        expect(file_metrics.height == memory_metrics.height &&
                   file_metrics.max_advance ==
                       memory_metrics.max_advance,
               "file and memory fonts have identical metrics");

        const native::text_metrics file_text =
            from_file.measure_text("Native AV");
        const native::text_metrics memory_text =
            from_memory.measure_text("Native AV");
        expect(file_text.width > 0 && file_text.advance > 0 &&
                   file_text.width == memory_text.width &&
                   file_text.advance == memory_text.advance,
               "file and memory fonts measure UTF-8 identically");
        expect(from_memory.measure_text("").height ==
                   memory_metrics.height,
               "empty text retains the selected line height");
        expect(from_memory.measure_character(U'A').advance > 0,
               "portable fonts measure individual characters");

        const std::uint32_t memory_id = from_memory.id();
        native::font_t moved = std::move(from_memory);
        expect(moved.valid() && moved.id() == memory_id &&
                   !from_memory.valid(),
               "moving a portable font preserves its registration");

        recording_gpx graphics;
        graphics.set_font(moved).draw_text(
            "Portable", native::point(0, 0));
        expect(graphics.painted,
               "portable text is rasterized through image drawing");
    }

    // Verify image contexts and both supported encoded representations.
    void test_image_io() {
        const native::rgba source_color(32, 96, 208, 255);
        native::img source(8, 8);
        source.get_gpx().clear(source_color);
        for (std::size_t index = 0; index < 64; ++index) {
            expect(source.pixels()[index].r == source_color.r &&
                       source.pixels()[index].g == source_color.g &&
                       source.pixels()[index].b == source_color.b &&
                       source.pixels()[index].a == source_color.a,
                   "image context begins with a full-image clip");
        }

        native::img clipped(4, 4);
        clipped.get_gpx()
            .clear(native::rgba(0, 0, 0, 255))
            .set_clip(native::rect(1, 1, 2, 2))
            .clear(native::rgba(255, 255, 255, 255));
        int white_pixels = 0;
        for (std::size_t index = 0; index < 16; ++index) {
            if (clipped.pixels()[index].r == 255)
                ++white_pixels;
        }
        expect(white_pixels == 4,
               "image context uses half-open clipping boundaries");

        native::img alpha_target(1, 1);
        alpha_target.get_gpx().clear(native::rgba(0, 0, 255, 255));
        native::img alpha_source(1, 1);
        alpha_source.pixels()[0] = native::rgba(255, 0, 0, 128);
        alpha_target.get_gpx().draw_img(
            alpha_source, native::point(0, 0));
        const native::rgba blended = alpha_target.pixels()[0];
        expect(blended.r >= 127 && blended.r <= 128 &&
                   blended.g == 0 && blended.b >= 127 &&
                   blended.b <= 128 && blended.a == 255,
               "image drawing applies straight-alpha source-over");

        const native::rgba alpha_color(255, 0, 255, 79);
        source.pixels()[1] = alpha_color;
        const std::vector<std::uint8_t> png =
            source.encode(native::image_format::png);
        expect(png.size() >= 8 && png[0] == 0x89 && png[1] == 'P' &&
                   png[2] == 'N' && png[3] == 'G',
               "PNG encoding has the expected signature");
        native::img png_copy =
            native::img::decode(png.data(), png.size());
        expect(png_copy.w() == source.w() && png_copy.h() == source.h(),
               "PNG memory round trip preserves dimensions");
        expect(static_cast<std::uint32_t>(png_copy.pixels()[0]) ==
                   static_cast<std::uint32_t>(source_color),
               "PNG memory round trip preserves RGBA pixels");
        expect(static_cast<std::uint32_t>(png_copy.pixels()[1]) ==
                   static_cast<std::uint32_t>(alpha_color),
               "PNG memory round trip preserves alpha");

        source.get_gpx()
            .set_clip(native::rect(0, 0, 8, 8))
            .clear(source_color);
        const std::vector<std::uint8_t> jpeg =
            source.encode(native::image_format::jpeg, 95);
        expect(jpeg.size() >= 2 && jpeg[0] == 0xff && jpeg[1] == 0xd8,
               "JPEG encoding has the expected signature");
        native::img jpeg_copy =
            native::img::decode(jpeg.data(), jpeg.size());
        expect(jpeg_copy.w() == source.w() &&
                   jpeg_copy.h() == source.h(),
               "JPEG memory round trip preserves dimensions");
        const native::rgba jpeg_pixel = jpeg_copy.pixels()[0];
        expect(
            std::abs(static_cast<int>(jpeg_pixel.r) - source_color.r) <=
                    8 &&
                std::abs(static_cast<int>(jpeg_pixel.g) -
                         source_color.g) <= 8 &&
                std::abs(static_cast<int>(jpeg_pixel.b) -
                         source_color.b) <= 8 &&
                jpeg_pixel.a == 255,
            "JPEG memory round trip preserves an opaque solid color");

        const std::string path = "native-image-codec-test.png";
        source.save(path);
        native::img file_copy = native::img::load(path);
        std::remove(path.c_str());
        expect(file_copy.w() == source.w() &&
                   file_copy.h() == source.h(),
               "PNG file round trip preserves dimensions");

        bool rejected_quality = false;
        try {
            (void)source.encode(native::image_format::jpeg, 0);
        } catch (const std::invalid_argument &) {
            rejected_quality = true;
        }
        expect(rejected_quality, "JPEG encoding validates quality");

        const std::uint8_t broken_jpeg[] = {0xff, 0xd8};
        bool rejected_image = false;
        try {
            (void)native::img::decode(broken_jpeg, sizeof(broken_jpeg));
        } catch (const std::runtime_error &) {
            rejected_image = true;
        }
        expect(rejected_image,
               "image decoding rejects malformed input");

        bool rejected_character = false;
        native::font_t invalid;
        const native::font_metrics invalid_metrics =
            invalid.get_metrics();
        const native::text_metrics invalid_text =
            invalid.measure_text("x");
        expect(invalid_metrics.height == 0 &&
                   invalid_metrics.max_advance == 0 &&
                   invalid_text.width == 0 && invalid_text.advance == 0,
               "invalid fonts have empty measurements");
        try {
            (void)invalid.measure_character(0x110000);
        } catch (const std::invalid_argument &) {
            rejected_character = true;
        }
        expect(rejected_character,
               "font measurement rejects an invalid Unicode character");
    }
} // namespace

int main() {
    test_cached_properties();
    test_parent_lifetime();
    test_owned_window_lifetime();
    test_file_dialog_properties();
    test_selection_controls();
    test_text_edit_properties();
    test_screen_snapshot();
    test_theme_factory();
    test_portable_fonts();
    test_image_io();
    return failure_count == 0 ? 0 : 1;
}
