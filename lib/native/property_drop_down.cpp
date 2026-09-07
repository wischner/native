//
// Hosts arbitrary property editors in an owned popup. Values stay in the
// grid until the content commits; cancelling discards uncommitted edits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "property_drop_down.h"

#include <algorithm>
#include <stdexcept>
#include <native/app.h>
#include <native/button.h>
#include <native/font.h>
#include <native/modeless_wnd.h>
#include <native/screen.h>

namespace native::detail
{
    class property_drop_down_editor::popup final : public modeless_wnd
    {
    public:
        // Own one user-supplied control plus an explicit cancellation action.
        popup(app_wnd &owner, point position, size dimensions,
              std::unique_ptr<wnd> content)
            : modeless_wnd(owner, "Property value", position.x, position.y,
                           dimensions.w + 8, dimensions.h + 38),
              _content(std::move(content)), _cancel("Cancel") {
            _content->set_parent(this);
            _content->set_bounds(rect(4, 4, dimensions.w, dimensions.h));
            _cancel.set_parent(this);
            _cancel.set_bounds(rect(std::max(4, int(dimensions.w) - 68),
                                   dimensions.h + 9, 72, 24));
            _cancel.on_click.connect([this] { request_close(); return true; });
            on_wnd_create.connect([this] {
                _content->create(); _content->show();
                _cancel.create(); _cancel.show();
                return true;
            });
        }

        bool get_native_title_visible() const override { return false; }

    private:
        std::unique_ptr<wnd> _content;
        button _cancel;
    };

    property_drop_down_editor::property_drop_down_editor(
        property_item item, property_drop_down::commit commit)
        : _item(std::move(item)), _commit(std::move(commit)),
          _lifetime(std::make_shared<int>(0)) {
        set_horizontal_scrollbar_policy(scrollbar_policy::never);
        set_vertical_scrollbar_policy(scrollbar_policy::never);
        on_wnd_paint.connect([this](wnd_paint_event event) { paint(event); return true; });
        on_mouse_click.connect([this](mouse_event event) {
            if (!_item.read_only && event.button == mouse_button::left &&
                event.action == mouse_action::release) open(event.position);
            return true;
        });
    }

    property_drop_down_editor::~property_drop_down_editor() {
        _lifetime.reset();
        destroy();
    }

    void property_drop_down_editor::destroy_native() {
        _session.reset();
        if (_popup) _popup->destroy();
        canvas::destroy_native();
    }

    void property_drop_down_editor::set_value(property_value value) {
        _item.value = std::move(value);
        invalidate();
    }

    void property_drop_down_editor::paint(wnd_paint_event event) {
        auto saved = event.g.save_state();
        auto appearance = theme::create(event.g);
        const auto bounds = get_client_bounds();
        appearance->draw_surface(bounds, surface_kind::panel, {});
        event.g.set_font(font_t::stock(font_role::control))
            .set_ink(appearance->native_palette().content_text)
            .draw_text(_item.drop_down->to_text(_item.value),
                rect(3, 0, std::max(0, int(bounds.w()) - 22), bounds.h()),
                {text_align::start, text_valign::center, text_overflow::ellipsis, true});
        if (!_item.read_only) {
            const int x = std::max(0, int(bounds.w()) - 12), y = bounds.h() / 2;
            event.g.set_pen(1).draw_polygon({point(x - 4, y - 2),
                point(x + 4, y - 2), point(x, y + 2)}, true);
        }
    }

    void property_drop_down_editor::open(point position) {
        if (_popup && _popup->get_created()) {
            _session.reset(); _popup->destroy(); return;
        }
        wnd *root = this;
        while (root->get_parent()) root = root->get_parent();
        auto *owner = dynamic_cast<app_wnd *>(root);
        if (!owner) return;
        const std::weak_ptr<int> lifetime = _lifetime;
        _session = std::make_shared<int>(0);
        const std::weak_ptr<int> session = _session;
        auto content = _item.drop_down->create_content(_item.value,
            [this, lifetime, session](property_value value) {
                // A native event must finish before its originating control
                // disappears; stale callbacks cannot reach a removed row.
                app::post([this, lifetime, session, value = std::move(value)]() mutable {
                    if (lifetime.expired() || session.expired() || !get_created() ||
                        !_popup || !_popup->get_created()) return;
                    _session.reset();
                    _popup->destroy();
                    _commit(std::move(value));
                });
            });
        if (!content || content->get_created())
            throw std::invalid_argument("A property dropdown needs uncreated content.");
        const auto screen_position = get_mouse_screen_position();
        point origin(screen_position.x - position.x,
                     screen_position.y - position.y + get_dimensions().h);
        for (const auto &display : screen::detect()) {
            const auto area = display.work_area();
            if (!area.contains(screen_position)) continue;
            const int width = _item.drop_down->content_size.w + 8;
            const int height = _item.drop_down->content_size.h + 38;
            if (origin.y + height > area.y2()) origin.y -= height + get_dimensions().h;
            origin.x = std::clamp(int(origin.x), int(area.x1()), std::max(int(area.x1()), area.x2() - width));
            origin.y = std::clamp(int(origin.y), int(area.y1()), std::max(int(area.y1()), area.y2() - height));
            break;
        }
        _popup = std::make_unique<popup>(*owner, origin,
            _item.drop_down->content_size, std::move(content));
        _popup->create(); _popup->show();
    }
}
