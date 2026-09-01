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
    void code_edit::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: code_edit requires a created parent.");
        auto *self = const_cast<code_edit *>(this);
        linux::gemix::code_edits.push_back(self);
        _created = true;
        self->invalidate();
        self->on_wnd_create.emit();
    }

    void code_edit::show() const {
        if (!_created)
            throw std::runtime_error(
                "GEMix: code_edit is not created.");
        invalidate();
    }

    void code_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<code_edit *>(this);
        self->on_native_destroy();
        linux::gemix::code_edits.erase(
            std::remove(linux::gemix::code_edits.begin(),
                        linux::gemix::code_edits.end(),
                        self),
            linux::gemix::code_edits.end());
    }
} // namespace native
