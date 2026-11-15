//
// Orders explicit tab stops within one window's live child hierarchy.
// Native focus transfers remain in the backend; ordering is portable.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/wnd.h>
#include <native/app_wnd.h>
#include <native/text_edit.h>
#include <native/tab_view.h>
#include "input_state.h"
#include "focus_redirect.h"

#include <algorithm>
#include <functional>

namespace native
{
    unsigned wnd::get_tab_order() const {
        return _tab_order;
    }

    wnd &wnd::set_tab_order(unsigned order) {
        _tab_order = order;
        return *this;
    }

    void wnd::focus() {
        for (wnd *ancestor = this; ancestor; ancestor = ancestor->_parent)
            if (!ancestor->get_created() || !ancestor->get_visible() ||
                !ancestor->get_input_enabled()) return;
        const detail::window_guard guard(*this);
        on_native_focus(true);
        if (!guard.alive()) return;
        if (auto *redirect = detail::peer_state<detail::focus_redirect>(*this)) {
            const auto apply = redirect->apply;
            apply();
        } else focus_native();
    }

    bool wnd::on_native_tab(key_event event) {
        if (!get_created() || !get_input_enabled()) return false;
        if ((event.modifiers & (key_modifiers::ctrl | key_modifiers::alt |
                                key_modifiers::meta)) != key_modifiers::none)
            return false;
        bool traverse = event.key == key_code::tab;
        if (event.key == key_code::enter || event.key == key_code::keypad_enter) {
            const auto *editor = dynamic_cast<const text_edit *>(this);
            traverse = editor && editor->get_mode() == text_edit_mode::single_line &&
                       editor->get_enter_as_tab();
        }
        if (!traverse) return false;
        if (event.action != key_action::press) return true;

        wnd *root = this;
        while (root->_parent && !dynamic_cast<app_wnd *>(root))
            root = root->_parent;
        std::vector<wnd *> stops;
        std::function<void(wnd &)> collect = [&](wnd &parent) {
            if (!parent.get_created() || !parent.get_visible() ||
                !parent.get_input_enabled()) return;
            if (&parent != root && parent._tab_order) stops.push_back(&parent);
            for (wnd *child : parent._children) {
                if (!child || dynamic_cast<app_wnd *>(child)) continue;
                if (auto *tabs = dynamic_cast<tab_view *>(&parent)) {
                    bool inactive = false;
                    for (std::size_t i = 0; i < tabs->get_item_count(); ++i) {
                        const auto &item = tabs->get_item(i);
                        if (item.has_content() && &item.get_content() == child)
                            inactive = static_cast<int>(i) != tabs->get_selected_index();
                    }
                    if (inactive) continue;
                }
                collect(*child);
            }
        };
        collect(*root);
        if (stops.empty()) return true;
        std::stable_sort(stops.begin(), stops.end(), [](wnd *a, wnd *b) {
            return a->_tab_order < b->_tab_order;
        });
        const bool backward =
            (event.modifiers & key_modifiers::shift) != key_modifiers::none;
        wnd *origin = this;
        auto current = std::find(stops.begin(), stops.end(), origin);
        while (current == stops.end() && origin != root && origin->_parent) {
            origin = origin->_parent;
            current = std::find(stops.begin(), stops.end(), origin);
        }
        wnd *next = backward ? stops.back() : stops.front();
        if (current != stops.end()) {
            const auto index = static_cast<std::size_t>(current - stops.begin());
            next = stops[(index + (backward ? stops.size() - 1 : 1)) % stops.size()];
        }
        next->focus();
        return true;
    }
}
