//
// Implements GEM tree_view lifecycle through the AES-window collection
// registry while VDI semantic painting supplies the classic outline.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native.h>

#include "globals.h"

namespace native
{
    void tree_view::apply_items() { invalidate(); }
    void tree_view::apply_selection() { invalidate(); }
    void tree_view::apply_expansion(tree_item_id, bool) { invalidate(); }
    void tree_view::apply_scroll_offset() { invalidate(); }

    void tree_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: tree_view requires a created parent.");
        auto *self = this;
        linux::gemix::tree_views.push_back(self);
        self->synchronize_theme_metrics();
    }

    void tree_view::show_native() {
        if (!_created)
            throw std::runtime_error("GEMix: tree_view is not created.");
        invalidate();
    }

    void tree_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        linux::gemix::tree_views.erase(
            std::remove(linux::gemix::tree_views.begin(),
                        linux::gemix::tree_views.end(),
                        self),
            linux::gemix::tree_views.end());
        linux::gemix::forget_tree_click(self);
    }
} // namespace native
