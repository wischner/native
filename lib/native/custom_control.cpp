//
// Implements focus caching and one shared active-theme lookup for painted
// controls and collections.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/custom_control.h>

#include <stdexcept>

namespace native
{
    custom_control::~custom_control() = default;

    bool custom_control::get_focused() const {
        return _focused;
    }

    void custom_control::on_native_focus(bool focused) {
        if (_focused == focused)
            return;
        _focused = focused;
        invalidate();
    }

    void custom_control::synchronize_theme_metrics() {
        wnd *root = this;
        while (root->get_parent())
            root = root->get_parent();
        try {
            auto appearance = theme::create(root->get_gpx());
            _theme_metrics = appearance->defaults();
        } catch (const std::runtime_error &) {
            // Xt and WINGs can finish realization after child creation.
            // Portable defaults remain valid until a later sync.
        }
    }
} // namespace native
