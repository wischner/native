//
// Implements the GEMix radio control through the GEM theme painter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/radio.h>
#include "globals.h"
namespace native
{
    void radio::apply_text() {
        invalidate();
    }
    void radio::apply_selected() {
        invalidate();
    }
    void radio::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "GEMix: radio requires a created parent.");
        auto *self = const_cast<radio *>(this);
        linux::gemix::radios.push_back(self);
        _created = true;
        self->on_wnd_create.emit();
    }
    void radio::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: radio is not created.");
        invalidate();
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        self->on_native_destroy();
        linux::gemix::radios.erase(
            std::remove(linux::gemix::radios.begin(),
                        linux::gemix::radios.end(),
                        self),
            linux::gemix::radios.end());
    }
} // namespace native
