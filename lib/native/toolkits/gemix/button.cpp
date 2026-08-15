//
// Implements the GEMix button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>

#include "globals.h"

namespace native
{
    void button::apply_text() {
        invalidate();
    }

    void button::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error("GEMix: button requires a parent.");
        if (!parent->get_created())
            throw std::runtime_error(
                "GEMix: button parent is not created.");

        _created = true;
        linux::gemix::buttons.push_back(const_cast<button *>(this));
        const_cast<button *>(this)->on_wnd_create.emit();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error(
                "GEMix: Cannot show button before it is created.");
        invalidate();
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        self->on_native_destroy();

        linux::gemix::buttons.erase(
            std::remove(linux::gemix::buttons.begin(), linux::gemix::buttons.end(), self),
            linux::gemix::buttons.end());
    }
}
