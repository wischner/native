//
// Embeds the existing SDL text editor inside editable combo fields,
// sharing selection, UTF-8 input, caret and clipboard behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"

namespace linux::sdl2
{
    void update_combo_editor(native::combo_box &owner) {
        auto *state = combo_box_bindings.object_from_handle(&owner);
        if (!state || !owner.get_created())
            return;
        if (owner.get_style() != native::combo_box_style::editable) {
            state->editor.reset();
            return;
        }
        if (!state->editor) {
            state->editor = std::make_unique<native::text_edit>();
            auto &editor = *state->editor;
            editor.set_parent(&owner);
            editor.set_border_sides(native::border_sides::none);
            editor.on_change.connect([&owner](const std::string &text) {
                owner.on_native_text(text);
                return true;
            });
        }
        auto &editor = *state->editor;
        const auto dimensions = owner.get_dimensions();
        const int width =
            std::max(1, int(dimensions.w) - int(dimensions.h));
        if (editor.get_dimensions().w != width ||
            editor.get_dimensions().h != dimensions.h)
            editor.set_bounds(native::rect(0, 0, width, dimensions.h));
        if (editor.get_text() != owner.get_text())
            editor.set_text(owner.get_text());
        if (!editor.get_created()) {
            editor.create();
            if (state->visible)
                editor.show();
        }
    }
} // namespace linux::sdl2
