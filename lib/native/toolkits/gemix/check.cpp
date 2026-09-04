//
// Implements the GEMix check control through the GEM theme painter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/check.h>
#include "globals.h"
namespace native
{
    void check::apply_text() {
        invalidate();
    }
    void check::apply_checked() {
        invalidate();
    }
    void check::create_native() {
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "GEMix: check requires a created parent.");
        auto *self = this;
        linux::gemix::checks.push_back(self);
    }
    void check::show_native() {
        if (!_created)
            throw std::runtime_error("GEMix: check is not created.");
        invalidate();
    }
    void check::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        linux::gemix::checks.erase(
            std::remove(linux::gemix::checks.begin(),
                        linux::gemix::checks.end(),
                        self),
            linux::gemix::checks.end());
    }
} // namespace native
