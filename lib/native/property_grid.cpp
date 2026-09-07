//
// Implements a compact typed property inspector using a panel, a
// scrolling canvas, and standard text, check, and combo controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/property_grid.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

#include <native/canvas.h>
#include <native/check.h>
#include <native/combo_box.h>
#include <native/font.h>
#include <native/text_edit.h>
#include "property_drop_down.h"

namespace
{
    // Parse a complete finite decimal without locale-dependent formatting.
    template<typename number>
    bool number_value(const std::string &text, number &value) {
        if (text.empty()) return false;
        if constexpr (requires { std::from_chars(text.data(), text.data() + text.size(), value); }) {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
                   std::isfinite(value);
        } else {
            // Older cross-toolchain standard libraries lack floating charconv.
            std::istringstream stream(text);
            stream.imbue(std::locale::classic());
            stream >> std::noskipws >> value;
            return !stream.fail() && stream.eof() && std::isfinite(value);
        }
    }

    template<typename number>
    std::string number_text(number value) {
        char buffer[64];
        if constexpr (requires { std::to_chars(buffer, buffer + sizeof(buffer), value); }) {
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            return std::string(buffer, result.ptr);
        } else {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(std::numeric_limits<number>::max_digits10) << value;
            return stream.str();
        }
    }

    // Permit incomplete numeric edits while retaining the last valid value.
    bool number_input(const std::string &text) {
        if (text.empty() || text == "-" || text == "." || text == "-.")
            return true;
        double value;
        return number_value(text, value);
    }

    std::string display_value(const native::property_value &value) {
        if (const auto *text = std::get_if<std::string>(&value)) return *text;
        if (const auto *checked = std::get_if<bool>(&value))
            return *checked ? "True" : "False";
        return number_text(std::get<double>(value));
    }

    void validate(const native::property_item &item) {
        using native::property_kind;
        if (item.id.empty()) throw std::invalid_argument("Property ID is empty.");
        bool valid = false;
        switch (item.kind) {
        case property_kind::text:
            valid = std::holds_alternative<std::string>(item.value);
            break;
        case property_kind::number:
            valid = std::holds_alternative<double>(item.value) &&
                    std::isfinite(std::get<double>(item.value));
            break;
        case property_kind::boolean:
            valid = std::holds_alternative<bool>(item.value);
            break;
        case property_kind::choice:
            valid = std::holds_alternative<std::string>(item.value) &&
                std::find(item.choices.begin(), item.choices.end(),
                    std::get<std::string>(item.value)) != item.choices.end();
            break;
        case property_kind::drop_down:
            valid = item.drop_down && item.drop_down->to_text &&
                item.drop_down->create_content && item.drop_down->content_size.w &&
                item.drop_down->content_size.h && item.drop_down->content_size.w <= 32759 &&
                item.drop_down->content_size.h <= 32729;
            if (valid) item.drop_down->to_text(item.value);
            break;
        }
        if (!valid) throw std::invalid_argument("Invalid property value: " + item.id);
    }
}

namespace native
{
    struct property_grid::row
    {
        property_item item;
        std::unique_ptr<wnd> editor;
    };

    property_grid::property_grid(coord x, coord y, dim width, dim height)
        : panel(x, y, width, height), _viewport(std::make_unique<canvas>()),
          _labels(std::make_unique<canvas>()), _top_frame(std::make_unique<canvas>()),
          _bottom_frame(std::make_unique<canvas>()) {
        for (auto *surface : {_viewport.get(), _labels.get(),
                              _top_frame.get(), _bottom_frame.get()}) {
            surface->set_parent(this);
            surface->set_horizontal_scrollbar_policy(scrollbar_policy::never);
            surface->set_vertical_scrollbar_policy(scrollbar_policy::never);
        }
        _viewport->set_vertical_scrollbar_policy(scrollbar_policy::automatic);
        _labels->on_wnd_paint.connect([this](wnd_paint_event event) {
            paint_rows(event); return true;
        });
        _labels->on_mouse_wheel.connect([this](mouse_wheel_event event) {
            _viewport->on_native_mouse_wheel(event); return true;
        });
        const auto frame = [this](canvas *surface, border_sides edge) {
            surface->on_wnd_paint.connect([this, surface, edge](wnd_paint_event event) {
                auto appearance = theme::create(event.g);
                event.g.clear(appearance->native_palette().button_bg);
                event.g.set_pen(1).set_ink(appearance->native_palette().button_border)
                    .draw_border(rect(point(), surface->get_dimensions()),
                                 get_border_sides() & edge);
                return true;
            });
        };
        frame(_viewport.get(), border_sides::right);
        frame(_top_frame.get(), border_sides::top);
        frame(_bottom_frame.get(), border_sides::bottom);
        _viewport->on_scroll.connect([this](canvas_scroll_position) {
            refresh(); return true;
        });
    }

    property_grid::property_grid(const rect &bounds)
        : property_grid(bounds.x1(), bounds.y1(), bounds.w(), bounds.h()) {}

    property_grid::~property_grid() { destroy(); }

    property_grid &property_grid::add_item(property_item item) {
        validate(item);
        for (const auto &row : _rows)
            if (row->item.id == item.id)
                throw std::invalid_argument("Duplicate property ID: " + item.id);
        auto entry = std::make_unique<row>();
        entry->item = std::move(item);
        _rows.push_back(std::move(entry));
        refresh();
        return *this;
    }

    property_grid &property_grid::operator<<(property_item item) {
        return add_item(std::move(item));
    }

    property_grid &property_grid::set_items(std::vector<property_item> items) {
        std::set<std::string> ids;
        for (const auto &item : items) {
            validate(item);
            if (!ids.insert(item.id).second)
                throw std::invalid_argument("Duplicate property ID: " + item.id);
        }
        std::vector<std::unique_ptr<row>> rows;
        for (auto &item : items) {
            auto entry = std::make_unique<row>();
            entry->item = std::move(item);
            rows.push_back(std::move(entry));
        }
        _rows.swap(rows);
        rows.clear();
        refresh();
        return *this;
    }

    std::vector<property_item> property_grid::get_items() const {
        std::vector<property_item> result;
        for (const auto &row : _rows) result.push_back(row->item);
        return result;
    }

    property_grid::row &property_grid::find_row(const std::string &id) const {
        for (const auto &row : _rows)
            if (row->item.id == id) return *row;
        throw std::out_of_range("Unknown property: " + id);
    }

    const property_item &property_grid::get_item(const std::string &id) const {
        return find_row(id).item;
    }

    property_grid &property_grid::set_value(const std::string &id,
                                            property_value value) {
        auto &entry = find_row(id);
        auto item = entry.item;
        item.value = std::move(value);
        validate(item);
        entry.item.value = std::move(item.value);
        synchronize_editor(entry);
        invalidate();
        return *this;
    }

    bool property_grid::on_native_value(const std::string &id,
                                        property_value value) {
        auto &entry = find_row(id);
        if (entry.item.read_only) return false;
        auto proposed = entry.item;
        proposed.value = std::move(value);
        try { validate(proposed); }
        catch (const std::invalid_argument &) { return false; }
        catch (const std::bad_any_cast &) { return false; }
        catch (const std::bad_variant_access &) { return false; }
        if (entry.item.value.index() == proposed.value.index()) {
            if (const auto *text = std::get_if<std::string>(&proposed.value)) {
                if (*text == std::get<std::string>(entry.item.value)) return true;
            } else if (const auto *number = std::get_if<double>(&proposed.value)) {
                if (*number == std::get<double>(entry.item.value)) return true;
            } else if (const auto *flag = std::get_if<bool>(&proposed.value)) {
                if (*flag == std::get<bool>(entry.item.value)) return true;
            }
        }
        entry.item.value = std::move(proposed.value);
        if (auto *box = dynamic_cast<check *>(entry.editor.get()))
            box->set_text(display_value(entry.item.value));
        if (auto *dropdown = dynamic_cast<detail::property_drop_down_editor *>(entry.editor.get()))
            dropdown->set_value(entry.item.value);
        const property_change event{id, entry.item.value};
        invalidate();
        on_change.emit(event);
        return true;
    }

    property_grid &property_grid::set_row_height(int height) {
        if (height < 0) throw std::invalid_argument("Negative property row height.");
        _row_height = height;
        refresh();
        return *this;
    }

    int property_grid::get_row_height() const { return _resolved_height; }

    property_grid &property_grid::set_label_width(int width) {
        if (width < 0) throw std::invalid_argument("Negative property label width.");
        _label_width = width;
        refresh();
        return *this;
    }

    int property_grid::get_label_width() const { return _label_width; }

    property_grid &property_grid::set_first_visible_row(std::size_t row) {
        row = std::min(row, _rows.size());
        const auto offset = std::min<std::uint64_t>(
            std::uint64_t(row) * _resolved_height,
            std::numeric_limits<std::int32_t>::max());
        _viewport->set_scroll_position({0, static_cast<std::int32_t>(offset)});
        refresh();
        return *this;
    }

    std::size_t property_grid::get_first_visible_row() const {
        return std::max(0, _viewport->get_scroll_position().y) / _resolved_height;
    }

    void property_grid::make_editor(row &entry) {
        const auto id = entry.item.id;
        if (entry.item.kind == property_kind::drop_down) {
            entry.editor = std::make_unique<detail::property_drop_down_editor>(entry.item,
                [this, id](property_value value) { on_native_value(id, std::move(value)); });
        } else if (entry.item.kind == property_kind::boolean && !entry.item.read_only) {
            auto control = std::make_unique<check>(display_value(entry.item.value));
            control->on_change.connect([this, id](bool value) {
                return on_native_value(id, value);
            });
            entry.editor = std::move(control);
        } else if (entry.item.kind == property_kind::choice && !entry.item.read_only) {
            auto control = std::make_unique<combo_box>(entry.item.choices);
            control->on_selection_change.connect([this, id](int index) {
                const auto choices = get_item(id).choices;
                if (index < 0 || std::size_t(index) >= choices.size()) return false;
                return on_native_value(id, choices[index]);
            });
            entry.editor = std::move(control);
        } else {
            auto control = std::make_unique<text_edit>(display_value(entry.item.value));
            control->set_read_only(entry.item.read_only);
            if (entry.item.kind == property_kind::number)
                control->set_validator(number_input);
            control->on_change.connect([this, id](const std::string &text) {
                if (get_item(id).kind == property_kind::number) {
                    double value;
                    return !number_value(text, value) || on_native_value(id, value);
                }
                return on_native_value(id, text);
            });
            entry.editor = std::move(control);
        }
        entry.editor->set_border_sides(border_sides::none);
        entry.editor->set_parent(this);
        synchronize_editor(entry);
    }

    void property_grid::synchronize_editor(row &entry) {
        if (auto *dropdown = dynamic_cast<detail::property_drop_down_editor *>(entry.editor.get()))
            dropdown->set_value(entry.item.value);
        else if (auto *text = dynamic_cast<text_edit *>(entry.editor.get()))
            text->set_text(display_value(entry.item.value));
        else if (auto *box = dynamic_cast<check *>(entry.editor.get())) {
            box->set_checked(std::get<bool>(entry.item.value));
            box->set_text(display_value(entry.item.value));
        } else if (auto *combo = dynamic_cast<combo_box *>(entry.editor.get())) {
            const auto found = std::find(entry.item.choices.begin(),
                entry.item.choices.end(), std::get<std::string>(entry.item.value));
            combo->set_selected_index(static_cast<int>(found - entry.item.choices.begin()));
        }
    }

    void property_grid::create_native() {
        panel::create_native();
        for (auto *surface : {_viewport.get(), _labels.get(),
                              _top_frame.get(), _bottom_frame.get()}) {
            surface->create();
            surface->show();
        }
        refresh();
    }

    void property_grid::destroy_native() {
        destroy_children();
        panel::destroy_native();
    }

    void property_grid::show_native() {
        panel::show_native();
        // A dropdown value is itself a canvas. Flattened hosts must raise
        // it after the structural panel, just like the label canvases.
        for (auto &entry : _rows)
            if (entry->item.kind == property_kind::drop_down && entry->editor &&
                entry->editor->get_created()) entry->editor->show();
        for (auto *surface : {_viewport.get(), _labels.get(),
                              _top_frame.get(), _bottom_frame.get()})
            if (surface->get_created()) surface->show();
    }

    void property_grid::on_bounds_changed() { refresh(); }

    void property_grid::refresh() {
        if (_refreshing) return;
        _refreshing = true;
        try {
            if (get_created()) {
                const int font_height = font_t::stock(font_role::control).get_metrics().height;
                // Native value editors need their line descent plus a small
                // inset above and below; retain compact, font-sized rows.
                _resolved_height = std::max(font_height + 6, _row_height);
            } else _resolved_height = std::max(20, _row_height);
            const auto area = get_client_bounds();
            const int height = std::max(0, int(area.h()) - 2);
            const auto total = std::min<std::uint64_t>(
                std::uint64_t(_rows.size()) * _resolved_height,
                std::numeric_limits<std::int32_t>::max());
            int scroll_width = 1;
            if (total > std::uint64_t(height)) {
                wnd *root = this;
                while (root->get_parent()) root = root->get_parent();
                scroll_width = 16;
                if (root->get_created())
                    scroll_width = theme::create(root->get_gpx())->defaults().scrollbar_extent;
            }
            scroll_width = std::clamp(scroll_width, 1, std::max(1, int(area.w())));
            const int width = std::max(0, int(area.w()) - scroll_width);
            _resolved_label_width = std::clamp(_label_width ? _label_width : width / 2,
                                                0, width);
            const int label = _resolved_label_width;
            // These hosts never overlap. Native toolkits can realize siblings
            // in different stacking orders, and XView hosts are flattened.
            // Neither label painting nor its input window may cover an editor.
            _viewport->set_bounds(rect(area.x1() + width, area.y1() + 1, scroll_width, height));
            _labels->set_bounds(rect(area.x1(), area.y1() + 1, label, height));
            _top_frame->set_bounds(rect(area.x1(), area.y1(), width, 1));
            _bottom_frame->set_bounds(rect(area.x1(), area.y1() + std::max(0, int(area.h()) - 1), width, 1));
            _viewport->set_content_bounds({0, 0, 0, static_cast<std::uint32_t>(total)});
            for (std::size_t index = 0; index < _rows.size(); ++index) {
                auto &entry = *_rows[index];
                const auto y = std::int64_t(index) * _resolved_height -
                               _viewport->get_scroll_position().y;
                if (!get_created() || y < 0 || y + _resolved_height > height ||
                    width <= label + 3) {
                    if (entry.editor) entry.editor->destroy();
                    continue;
                }
                if (!entry.editor) make_editor(entry);
                int editor_width = width - label - 2;
                if (dynamic_cast<check *>(entry.editor.get()))
                    editor_width = std::min(editor_width,
                        font_t::stock(font_role::control).measure_text("False").width + 30);
                entry.editor->set_bounds(rect(area.x1() + label + 1,
                    area.y1() + static_cast<coord>(y) + 1,
                    editor_width, _resolved_height));
                if (!entry.editor->get_created()) {
                    synchronize_editor(entry);
                    entry.editor->create();
                    entry.editor->show();
                }
            }
            _labels->invalidate();
            _viewport->invalidate();
            _top_frame->invalidate();
            _bottom_frame->invalidate();
            invalidate();
        } catch (...) { _refreshing = false; throw; }
        _refreshing = false;
    }

    void property_grid::paint_rows(wnd_paint_event event) {
        auto appearance = theme::create(event.g);
        const auto client = _labels->get_client_bounds();
        appearance->draw_surface(rect(0, 0, client.w(), client.h()),
                                 surface_kind::panel, {});
        for (std::size_t index = get_first_visible_row(); index < _rows.size(); ++index) {
            const auto y = std::int64_t(index) * _resolved_height -
                           _viewport->get_scroll_position().y;
            if (y >= client.h()) break;
            draw_row(event.g, *appearance, rect(0, static_cast<coord>(y),
                     client.w(), _resolved_height), _rows[index]->item);
        }
        event.g.set_pen(1).set_ink(appearance->native_palette().button_border)
            .draw_border(rect(0, 0, client.w(), client.h()),
                         get_border_sides() & border_sides::left);
    }

    void property_grid::draw_row(gpx &graphics, theme &appearance,
                                  const rect &bounds, const property_item &item) {
        const int label = _resolved_label_width;
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(appearance.native_palette().content_text)
            .draw_text(item.label, rect(bounds.x1() + 4, bounds.y1(),
                std::max(0, label - 8), bounds.h()),
                {text_align::start, text_valign::center, text_overflow::ellipsis, true});
        graphics.set_ink(appearance.native_palette().separator)
            .draw_rect(rect(std::max(0, label - 1), bounds.y1(), 1, bounds.h()), true)
            .draw_rect(rect(bounds.x1(), bounds.y2() - 1, bounds.w(), 1), true);
    }
}
