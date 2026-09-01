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

    void tree_view::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: tree_view requires a created parent.");
        auto *self = const_cast<tree_view *>(this);
        linux::gemix::tree_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void tree_view::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: tree_view is not created.");
        invalidate();
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        self->on_native_destroy();
        linux::gemix::tree_views.erase(
            std::remove(linux::gemix::tree_views.begin(),
                        linux::gemix::tree_views.end(),
                        self),
            linux::gemix::tree_views.end());
        linux::gemix::forget_tree_click(self);
    }
} // namespace native
