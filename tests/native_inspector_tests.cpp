//
// Tests typed inspector values, edge reservations, sticky tools, and
// exact rectangular toolbar icon rendering without native windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#if defined(__HAIKU__)
#include <Application.h>
#endif

namespace
{
    void expect(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }

    template<class function> void rejects(function action) {
        bool rejected = false;
        try { action(); }
        catch (const std::invalid_argument &) { rejected = true; }
        expect(rejected, "invalid model mutation was accepted");
    }

    class toolbar_probe : public native::toolbar
    {
    public:
        using native::toolbar::toolbar;
        using native::toolbar::draw_tool;
    };

    void borders() {
        using native::border_sides;
        native::button button("Test");
        expect(button.get_border_sides() == border_sides::all, "all edges default");
        native::tree_view tree;
        tree.set_border_sides(border_sides::left | border_sides::bottom);
        expect(tree.get_border_visible(), "partial tree frame stays visible");
        tree.set_border_visible(false);
        expect(tree.get_border_sides() == border_sides::none, "legacy border false alias");
        tree.set_border_visible(true);
        expect(tree.get_border_sides() == border_sides::all, "legacy border true alias");
        const native::rgba paper(255, 255, 255, 255), ink(0, 0, 0, 255);
        native::img image(12, 12);
        for (unsigned mask = 0; mask < 16; ++mask) {
            const auto sides = static_cast<border_sides>(mask);
            image.get_gpx().clear(paper).set_ink(ink).set_pen(1)
                .draw_border(native::rect(1, 1, 10, 10), sides);
            const auto *pixels = image.pixels();
            expect((pixels[1 * 12 + 5] == ink) == native::has_border(sides, border_sides::top), "top edge mask");
            expect((pixels[10 * 12 + 5] == ink) == native::has_border(sides, border_sides::bottom), "bottom edge mask");
            expect((pixels[5 * 12 + 1] == ink) == native::has_border(sides, border_sides::left), "left edge mask");
            expect((pixels[5 * 12 + 10] == ink) == native::has_border(sides, border_sides::right), "right edge mask");
            expect(pixels[5 * 12 + 5] == paper, "border preserves interior");
        }
    }

    void properties() {
        using native::property_kind;
        native::property_grid grid;
        grid.set_items({{"name", "Name", property_kind::text, std::string("Shape"), {}, false},
            {"width", "Width", property_kind::number, 12.5, {}, false},
            {"visible", "Visible", property_kind::boolean, true, {}, false},
            {"mode", "Mode", property_kind::choice, std::string("Fill"), {"Fill", "Outline"}, false},
            {"locked", "Locked", property_kind::text, std::string("Fixed"), {}, true}});
        expect(grid.get_row_height() == 20, "compact precreation rows");
        int changes = 0;
        grid.on_change.connect([&](native::property_change event) {
            ++changes;
            std::visit([&](const auto &value) {
                using value_type = std::decay_t<decltype(value)>;
                if constexpr (!std::is_same_v<value_type, std::any>)
                    expect(std::get<value_type>(grid.get_item(event.id).value) == value,
                           "store value before event");
            }, event.value);
            return true;
        });
        grid.set_value("name", std::string("New"));
        expect(changes == 0, "programmatic properties are silent");
        expect(grid.on_native_value("width", 42.5), "numeric user value");
        expect(grid.on_native_value("visible", false), "boolean user value");
        expect(grid.on_native_value("mode", std::string("Outline")), "choice user value");
        expect(changes == 3, "one event per changed value");
        expect(grid.on_native_value("visible", false) && changes == 3, "unchanged value stays silent");
        expect(!grid.on_native_value("locked", std::string("Changed")), "read-only property rejects input");
        expect(!grid.on_native_value("width", std::string("bad")), "wrong user type rejected");
        rejects([&] { grid.set_value("mode", std::string("missing")); });
        rejects([&] { grid.set_value("width", std::numeric_limits<double>::infinity()); });
        rejects([&] { grid.add_item(grid.get_item("name")); });
        auto duplicate = grid.get_items();
        duplicate.push_back(duplicate.front());
        rejects([&] { grid.set_items(duplicate); });
        expect(grid.get_items().size() == 5, "failed replacement preserves model");
        grid.set_bounds(native::rect(0, 0, 280, 40));
        grid.set_first_visible_row(4);
        expect(grid.get_first_visible_row() > 0, "property viewport scrolls");
        grid.set_items({});
        expect(grid.get_first_visible_row() == 0, "empty model reclamps scroll");
        auto custom = std::make_shared<native::property_drop_down>();
        custom->to_text = [](const native::property_value &value) {
            return std::to_string(std::any_cast<int>(std::get<std::any>(value)));
        };
        custom->create_content = [](const native::property_value &, native::property_drop_down::commit) {
            return std::make_unique<native::canvas>();
        };
        grid.add_item({"custom", "Custom", property_kind::drop_down, std::any(12), {}, false, custom});
        grid.set_value("custom", std::any(24));
        expect(custom->to_text(grid.get_item("custom").value) == "24", "custom value converter receives the stored type");
        expect(!grid.on_native_value("custom", std::any(std::string("wrong"))), "custom converter rejects a wrong payload type");
        expect(!grid.on_native_value("custom", std::string("wrong")), "custom converter rejects a wrong variant alternative");
        expect(custom->to_text(grid.get_item("custom").value) == "24", "invalid custom edits preserve the model");
    }

    void tools() {
        using native::tool_kind;
        native::app_wnd owner("Tools", 0, 0, 400, 300);
        toolbar_probe top(owner);
        native::toolbar second(owner), bottom(owner, native::window_edge::bottom),
            left(owner, native::window_edge::left), right(owner, native::window_edge::right);
        expect(second.get_bounds().y1() == top.get_bounds().y2(), "multiple bars stack");
        expect(top.get_bounds().x1() == 0 && top.get_bounds().w() == 400 &&
               bottom.get_bounds().x1() == 0 && bottom.get_bounds().w() == 400,
               "horizontal toolbars span above and below shorter side strips");
        expect(owner.get_client_bounds().w() == 352 && owner.get_client_bounds().h() == 228,
               "all edge bars reserve independent strips");
        top.set_items({{"run", "Run", tool_kind::button, {}, false, true, {}},
            {"snap", "Snap", tool_kind::toggle, {}, false, true, {}},
            {"pen", "Pen", tool_kind::exclusive, "drawing", true, true, {}},
            {"select", "Select", tool_kind::exclusive, "drawing", false, true, {}}});
        int commands = 0;
        top.on_command.connect([&](native::tool_command command) {
            ++commands;
            expect(top.get_checked(command.id) == command.checked, "store sticky state before command");
            return true;
        });
        top.set_checked("snap", true);
        expect(commands == 0, "programmatic tools are silent");
        expect(top.on_native_command("snap") && !top.get_checked("snap"), "toggle unchecks");
        expect(top.on_native_command("select") && top.get_checked("select") &&
            !top.get_checked("pen"), "exclusive selection clears peer");
        expect(!top.on_native_command("select") && commands == 2, "selected exclusive tool stays selected");
        top.on_native_command("run");
        top.set_enabled("run", false);
        expect(!top.on_native_command("run") && commands == 3, "disabled push does not activate");
        rejects([&] { top.set_checked("run", true); });
        const native::rgba red(255, 0, 0, 255);
        auto icon = std::make_shared<native::img>(8, 8);
        icon->get_gpx().clear(red);
        for (const native::size dimensions : {native::size(16, 16), native::size(24, 24), native::size(32, 32)}) {
            top.set_icon_size(dimensions);
            expect(top.get_extent() >= dimensions.h + 4, "icon height expands bar");
            native::img target(dimensions.w + 4, dimensions.h + 4);
            auto &graphics = target.get_gpx();
            auto appearance = native::theme::create(graphics);
            top.draw_tool(graphics, *appearance, native::rect(0, 0, target.w(), target.h()),
                {"image", "", tool_kind::button, {}, false, true, icon}, {});
            for (int y = 0; y < target.h(); ++y)
                for (int x = 0; x < target.w(); ++x)
                    expect((target.pixels()[y * target.w() + x] == red) ==
                        (x >= 2 && x < dimensions.w + 2 && y >= 2 && y < dimensions.h + 2),
                        "icon fills exact configured rectangular size");
        }
        rejects([&] { top.set_icon_size({0, 16}); });
        second.set_visible(false);
        expect(owner.get_client_bounds().h() == 300 - top.get_extent() - bottom.get_extent(),
               "hidden toolbar releases its strip");
    }
}

int main() {
#if defined(__HAIKU__)
    BApplication application("application/x-vnd.native-inspector-tests");
#endif
    try { borders(); properties(); tools(); }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
