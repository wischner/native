//
// Implements the GEM themed combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <native/combo_box.h>

#include "globals.h"

namespace linux::gemix
{
    bool handle_combo_key(native::app_wnd *parent, WORD, WORD key) {
        for (auto *control : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(control);
            if (!control || !state || control->get_parent() != parent ||
                !state->focused) continue;
            const unsigned char character = key & 0xff;
            if (control->get_style() ==
                native::combo_box_style::editable) {
                std::string text = control->get_text();
                if (character == 8 && !text.empty())
                    text.pop_back();
                else if (std::isprint(character))
                    text.push_back(static_cast<char>(character));
                else
                    return false;
                control->on_native_text(text);
                parent->invalidate();
                return true;
            }
            return false;
        }
        return false;
    }
}

namespace native
{
    void combo_box::apply_items() { invalidate(); }
    void combo_box::apply_selected_index() { invalidate(); }
    void combo_box::apply_text() { invalidate(); }
    void combo_box::apply_style() { invalidate(); }

    void combo_box::create() const {
        if (_created) return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: combo box requires a created parent.");
        auto *self = const_cast<combo_box *>(this);
        auto *state = new linux::gemix::gem_combo_box;
        linux::gemix::combo_box_bindings.register_pair(self, state);
        linux::gemix::combo_boxes.push_back(self);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: combo box is not created.");
        invalidate();
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = linux::gemix::combo_box_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::gemix::combo_boxes.erase(std::remove(
            linux::gemix::combo_boxes.begin(),
            linux::gemix::combo_boxes.end(), self),
            linux::gemix::combo_boxes.end());
        linux::gemix::combo_box_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
