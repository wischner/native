//
// Implements backend-neutral application-window title state. Backends
// provide native creation, destruction, display, and title updates.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <native/app_wnd.h>
#include <native/modal_wnd.h>
#include <native/owned_wnd.h>

namespace native
{
    app_wnd::app_wnd(
        std::string title, coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height)
        , _title(std::move(title)) {}

    app_wnd::app_wnd(const std::string &title,
                     const point &position,
                     const size &dimensions)
        : app_wnd(title,
                  position.x,
                  position.y,
                  dimensions.w,
                  dimensions.h) {}

    app_wnd::app_wnd(const std::string &title, const rect &bounds)
        : app_wnd(title, bounds.p, bounds.d) {}

    app_wnd::~app_wnd() {
        destroy();

        while (!_owned_windows.empty())
            _owned_windows.back()->detach_owner();
    }

    const std::string &app_wnd::get_title() const {
        return _title;
    }

    app_wnd &app_wnd::set_title(const std::string &title) {
        _title = title;
        if (_created)
            apply_title();
        return *this;
    }

    app_wnd *app_wnd::get_owner() const {
        return nullptr;
    }

    app_wnd &app_wnd::center_to_parent() {
        app_wnd *owner = get_owner();
        if (!owner)
            return *this;
        const rect parent = owner->get_bounds();
        const size dimensions = get_dimensions();
        set_position(point(
            static_cast<coord>(parent.p.x +
                (static_cast<int>(parent.d.w)-dimensions.w)/2),
            static_cast<coord>(parent.p.y +
                (static_cast<int>(parent.d.h)-dimensions.h)/2)));
        return *this;
    }

    bool app_wnd::get_modal() const {
        return false;
    }

    bool app_wnd::get_native_title_visible() const {
        return true;
    }

    bool app_wnd::get_input_enabled() const {
        if (get_active_modal())
            return false;

        const app_wnd *branch = this;
        for (app_wnd *owner = get_owner(); owner;
             branch = owner, owner = owner->get_owner()) {
            modal_wnd *active = owner->get_active_modal();
            if (active && active != branch)
                return false;
        }

        return true;
    }

    modal_wnd *app_wnd::get_active_modal() const {
        return _modal_windows.empty() ? nullptr
                                      : _modal_windows.back();
    }

    void app_wnd::on_native_destroy() {
        destroy_owned_windows();
        menu.detach();
        wnd::on_native_destroy();
    }

    void app_wnd::on_native_menu(int command) {
        on_menu.emit(command);
    }

    void app_wnd::attach_owned_window(owned_wnd *window) {
        if (!window)
            return;

        const auto item = std::find(
            _owned_windows.begin(), _owned_windows.end(), window);
        if (item == _owned_windows.end())
            _owned_windows.push_back(window);
    }

    void app_wnd::detach_owned_window(owned_wnd *window) {
        _owned_windows.erase(
            std::remove(_owned_windows.begin(),
                        _owned_windows.end(),
                        window),
            _owned_windows.end());

        auto *modal = dynamic_cast<modal_wnd *>(window);
        if (modal)
            end_modal(modal);
    }

    void app_wnd::begin_modal(modal_wnd *window) {
        if (!window)
            return;

        end_modal(window);
        _modal_windows.push_back(window);
    }

    void app_wnd::end_modal(modal_wnd *window) {
        _modal_windows.erase(
            std::remove(_modal_windows.begin(),
                        _modal_windows.end(),
                        window),
            _modal_windows.end());
    }

    void app_wnd::destroy_owned_windows() const {
        const std::vector<owned_wnd *> windows = _owned_windows;
        for (auto item = windows.rbegin(); item != windows.rend();
             ++item) {
            owned_wnd *window = *item;
            if (window && window->get_owner() == this &&
                window->get_created()) {
                window->destroy();
            }
        }
    }

    void app_wnd::validate_owner_created() const {
        app_wnd *owner = get_owner();
        if (owner && !owner->get_created())
            throw std::logic_error(
                "An owned window requires a created owner.");
    }
} // namespace native
