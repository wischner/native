//
// Implements the GEMix list control through the GEM theme painter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/list.h>
#include "globals.h"
namespace native
{
    void list::apply_items() {
        invalidate();
    }
    void list::apply_selected_index() {
        invalidate();
    }
    void list::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "GEMix: list requires a created parent.");
        auto *self = const_cast<list *>(this);
        linux::gemix::lists.push_back(self);
        _created = true;
        self->on_native_create();
    }
    void list::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: list is not created.");
        invalidate();
    }
    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        self->on_native_destroy();
        linux::gemix::lists.erase(
            std::remove(linux::gemix::lists.begin(),
                        linux::gemix::lists.end(),
                        self),
            linux::gemix::lists.end());
    }
} // namespace native
