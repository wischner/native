//
// Demonstrates compact property editing and menu-themed toolbars at
// all four edges. Tool images are ordinary Native graphics assets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_window.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace
{
    // Demonstrate a custom value type and a canvas hosted in a dropdown.
    std::shared_ptr<const native::property_drop_down> color_editor() {
        auto editor = std::make_shared<native::property_drop_down>();
        editor->content_size = {192, 64};
        editor->to_text = [](const native::property_value &value) {
            const auto color = std::any_cast<native::rgba>(std::get<std::any>(value));
            std::ostringstream text;
            text << '#' << std::hex << std::setfill('0') << std::setw(2) << int(color.r)
                 << std::setw(2) << int(color.g) << std::setw(2) << int(color.b);
            return text.str();
        };
        editor->create_content = [](const native::property_value &,
                                     native::property_drop_down::commit commit) {
            const std::vector<native::rgba> colors = {{0, 0, 0, 255}, {255, 255, 255, 255},
                {190, 40, 40, 255}, {40, 150, 60, 255}, {40, 80, 190, 255}, {220, 170, 30, 255}};
            auto surface = std::make_unique<native::canvas>();
            surface->on_wnd_paint.connect([colors](native::wnd_paint_event event) {
                for (int index = 0; index < int(colors.size()); ++index)
                    event.g.set_ink(colors[index]).draw_rect(native::rect(index * 32, 0, 32, 64), true);
                return true;
            });
            surface->on_mouse_click.connect([colors, commit](native::mouse_event event) {
                if (event.button == native::mouse_button::left &&
                    event.action == native::mouse_action::release &&
                    event.position.x >= 0 && event.position.x < 192)
                    commit(std::any(colors[event.position.x / 32]));
                return true;
            });
            return surface;
        };
        return editor;
    }

    // Produce a small monochrome tool image that remains clear in GEM.
    std::shared_ptr<const native::img> tool_icon(bool selection) {
        auto image = std::make_shared<native::img>(32, 32);
        auto &graphics = image->get_gpx();
        graphics.clear(native::rgba(0, 0, 0, 0))
            .set_ink(native::rgba(0, 0, 0, 255)).set_pen(2);
        if (selection) {
            graphics.draw_polygon({{7, 5}, {7, 26}, {13, 20},
                {18, 29}, {22, 27}, {17, 18}, {25, 18}}, true);
        } else {
            graphics.draw_polygon({{7, 22}, {22, 5}, {27, 10},
                {12, 27}}, false).draw_line({7, 22}, {5, 29})
                .draw_line({5, 29}, {12, 27});
        }
        return image;
    }
}

namespace vision
{
    feature_properties::feature_properties(native::app_wnd &owner)
        : modeless_wnd(owner, "Vision Properties and Toolbars", 100, 60, 650, 470),
          _preview("Preview border"), _top(*this), _second(*this),
          _left(*this, native::window_edge::left),
          _right(*this, native::window_edge::right),
          _bottom(*this, native::window_edge::bottom), _status(*this) {
        using native::property_kind;
        _properties.set_items({
            {"name", "Name", property_kind::text, std::string("Preview border"), {}, false},
            {"width", "Width", property_kind::number, 180.0, {}, false},
            {"snap", "Snap to grid", property_kind::boolean, false, {}, false},
            {"mode", "Tool", property_kind::choice, std::string("Select"), {"Select", "Draw"}, false},
            {"icons", "Icon size", property_kind::choice, std::string("16 x 16"),
                {"16 x 16", "24 x 24", "32 x 32"}, false},
            {"top", "Top border", property_kind::boolean, true, {}, false},
            {"bottom", "Bottom border", property_kind::boolean, true, {}, false},
            {"left", "Left border", property_kind::boolean, true, {}, false},
            {"right", "Right border", property_kind::boolean, true, {}, false},
            {"color", "Custom canvas", property_kind::drop_down,
                std::any(native::rgba(40, 80, 190, 255)), {}, false, color_editor()}});
        _properties.set_parent(this);
        _preview.set_parent(this);
        _properties.on_change.connect(this, &feature_properties::on_property);
        on_wnd_create.connect(this, &feature_properties::on_create);
        on_wnd_paint.connect([this](native::wnd_paint_event event) {
            auto appearance = native::theme::create(event.g);
            appearance->draw_surface(get_client_bounds(), native::surface_kind::panel, {});
            event.g.set_ink(appearance->native_palette().content_text)
                .draw_text("Edit values; toggle each border edge.",
                    native::point(get_client_bounds().x1() + 12, get_client_bounds().y1() + 8));
            return true;
        });
        for (auto *bar : {&_top, &_second, &_left, &_right, &_bottom})
            bar->on_command.connect([this](native::tool_command command) {
                if (command.id == "select" || command.id == "draw")
                    _properties.set_value("mode", std::string(command.id == "select" ? "Select" : "Draw"));
                else if (command.id == "snap") _properties.set_value("snap", command.checked);
                else if (command.id == "reset") {
                    for (const auto *edge : {"top", "bottom", "left", "right"})
                        _properties.set_value(edge, true);
                    _preview.set_border_sides(native::border_sides::all);
                }
                _status.set_text("Tool: " + command.id);
                return true;
            });
    }

    bool feature_properties::on_create() {
        using native::tool_kind;
        const auto select = tool_icon(true), draw = tool_icon(false);
        _top.set_items({{"new", "New", tool_kind::button, {}, false, true, select},
            {"save", "Save", tool_kind::button, {}, false, true, draw}});
        _second.set_items({{"snap", "Snap to grid", tool_kind::toggle, {}, false, true, {}},
            {"guides", "Guides", tool_kind::toggle, {}, true, true, {}}});
        _left.set_items({{"select", "", tool_kind::exclusive, "tools", true, true, select},
            {"draw", "", tool_kind::exclusive, "tools", false, true, draw}});
        _right.set_items({{"inspect", "", tool_kind::button, {}, false, true, select}});
        _bottom.set_items({{"reset", "Reset borders", tool_kind::button, {}, false, true, {}},
            {"details", "Details", tool_kind::toggle, {}, false, true, {}}});
        _properties.create(); _properties.show();
        _preview.create(); _preview.show();
        _status.set_text("16 x 16 icons; compact property rows");
        arrange();
        return true;
    }

    void feature_properties::on_bounds_changed() {
        if (get_created()) arrange();
    }

    void feature_properties::arrange() {
        const auto client = get_client_bounds();
        _properties.set_bounds(native::rect(client.x1() + 12, client.y1() + 32,
            std::min(330, std::max(0, int(client.w()) - 24)),
            std::max(0, int(client.h()) - 44)));
        const auto width = std::get<double>(_properties.get_item("width").value);
        _preview.set_bounds(native::rect(client.x1() + 356, client.y1() + 52,
            std::min(int(std::clamp(width, 1.0, 300.0)), std::max(0, int(client.w()) - 368)), 30));
    }

    bool feature_properties::on_property(native::property_change event) {
        if (event.id == "name") _preview.set_text(std::get<std::string>(event.value));
        else if (event.id == "width") arrange();
        else if (event.id == "snap") _second.set_checked("snap", std::get<bool>(event.value));
        else if (event.id == "mode")
            _left.set_checked(std::get<std::string>(event.value) == "Select" ? "select" : "draw", true);
        else if (event.id == "color") {
            _status.set_text("Color: " + _properties.get_item("color").drop_down->to_text(event.value));
            return true;
        } else if (event.id == "icons") {
            const auto text = std::get<std::string>(event.value);
            const native::dim extent = text == "16 x 16" ? 16 : text == "24 x 24" ? 24 : 32;
            for (auto *bar : {&_top, &_left, &_right}) {
                bar->set_icon_size({extent, extent});
            }
            arrange();
        } else {
            native::border_sides sides = native::border_sides::none;
            if (std::get<bool>(_properties.get_item("top").value)) sides = sides | native::border_sides::top;
            if (std::get<bool>(_properties.get_item("bottom").value)) sides = sides | native::border_sides::bottom;
            if (std::get<bool>(_properties.get_item("left").value)) sides = sides | native::border_sides::left;
            if (std::get<bool>(_properties.get_item("right").value)) sides = sides | native::border_sides::right;
            _preview.set_border_sides(sides);
        }
        _status.set_text("Changed: " + event.id);
        return true;
    }
}
