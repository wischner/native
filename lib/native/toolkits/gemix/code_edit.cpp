//
// Implements code_edit lifecycle in the GEM painted-control host.
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
    void code_edit::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: code_edit requires a created parent.");
        auto *self = this;
        linux::gemix::code_edits.push_back(self);
        self->invalidate();
    }

    void code_edit::show_native() {
        if (!_created)
            throw std::runtime_error(
                "GEMix: code_edit is not created.");
        invalidate();
    }

    void code_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        linux::gemix::code_edits.erase(
            std::remove(linux::gemix::code_edits.begin(),
                        linux::gemix::code_edits.end(),
                        self),
            linux::gemix::code_edits.end());
    }
} // namespace native
