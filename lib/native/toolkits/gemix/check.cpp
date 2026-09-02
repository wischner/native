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
    void check::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "GEMix: check requires a created parent.");
        auto *self = const_cast<check *>(this);
        linux::gemix::checks.push_back(self);
        _created = true;
        self->on_native_create();
    }
    void check::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: check is not created.");
        invalidate();
    }
    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        self->on_native_destroy();
        linux::gemix::checks.erase(
            std::remove(linux::gemix::checks.begin(),
                        linux::gemix::checks.end(),
                        self),
            linux::gemix::checks.end());
    }
} // namespace native
