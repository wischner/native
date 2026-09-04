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
    void list::create_native() {
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "GEMix: list requires a created parent.");
        auto *self = this;
        linux::gemix::lists.push_back(self);
    }
    void list::show_native() {
        if (!_created)
            throw std::runtime_error("GEMix: list is not created.");
        invalidate();
    }
    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        linux::gemix::lists.erase(
            std::remove(linux::gemix::lists.begin(),
                        linux::gemix::lists.end(),
                        self),
            linux::gemix::lists.end());
    }
} // namespace native
