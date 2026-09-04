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
#include <native/button.h>

#include "globals.h"

namespace native
{
    void button::apply_text() {
        invalidate();
    }

    void button::create_native() {
        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "GEMix: button requires a parent.");
        if (!parent->get_created())
            throw std::runtime_error(
                "GEMix: button parent is not created.");

        linux::gemix::buttons.push_back(this);
    }

    void button::show_native() {
        if (!_created)
            throw std::runtime_error(
                "GEMix: Cannot show button before it is created.");
        invalidate();
    }

    void button::destroy_native() {
        if (!_created)
            return;

        auto *self = this;

        linux::gemix::buttons.erase(
            std::remove(linux::gemix::buttons.begin(),
                        linux::gemix::buttons.end(),
                        self),
            linux::gemix::buttons.end());
    }
} // namespace native
