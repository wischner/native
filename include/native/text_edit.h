//
// Declares the portable native or emulated text editor with single-line
// and multiline modes, live validation, selection, and clipboard actions.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <functional>
#include <string>

#include "wnd.h"

namespace native
{
    // Selects the immutable editing behavior of a text_edit control.
    enum class text_edit_mode
    {
        single_line,
        multi_line
    };

    // Validates a complete proposed UTF-8 editor value.
    using text_validator =
        std::function<bool(const std::string &proposed_text)>;

    // Represents a single-line or multiline text editing window.
    class text_edit : public wnd
    {
    public:
        // Construct an editor from text, mode, and scalar bounds.
        text_edit(std::string text = {},
                  text_edit_mode mode = text_edit_mode::single_line,
                  coord x = 0,
                  coord y = 0,
                  dim width = 180,
                  dim height = 28);

        // Construct an editor from text, mode, position, and size.
        text_edit(const std::string &text,
                  text_edit_mode mode,
                  const point &position,
                  const size &dimensions);

        // Construct an editor from text, mode, and complete bounds.
        text_edit(const std::string &text,
                  text_edit_mode mode,
                  const rect &bounds);

        // Destroy the editor and its native resource if it exists.
        ~text_edit() override;

        // Return the cached portable UTF-8 text.
        const std::string &get_text() const;

        // Replace text after validation without emitting on_change.
        text_edit &set_text(const std::string &text);

        // Return whether this is a single-line or multiline editor.
        text_edit_mode get_mode() const;

        // Return whether user edits are currently disabled.
        bool get_read_only() const;

        // Enable or disable user edits on a created control.
        text_edit &set_read_only(bool read_only);

        // Return the live complete-value validator, if one is set.
        const text_validator &get_validator() const;

        // Set the validator after it accepts the current value.
        text_edit &set_validator(text_validator validator);

        // Remove the current live validator.
        text_edit &clear_validator();

        // Return whether text satisfies encoding, mode, and validator.
        bool validate(const std::string &text) const;

        // Accept or reject a complete native-originated value.
        bool on_native_text(const std::string &text);

        // Copy the current selection to the shared clipboard.
        bool copy() const;

        // Copy and remove the current selection when it is editable.
        bool cut();

        // Replace the current selection with clipboard text.
        bool paste();

        // Select the complete editor value.
        void select_all() const;

        // Create the backend text-edit resource.
        void create() const override;

        // Destroy the backend text-edit resource.
        void destroy() const override;

        // Show the backend text-edit resource.
        void show() const override;

        // Emits validated text after a user-originated change.
        signal<std::string> on_change;

    private:
        std::string _text;
        text_edit_mode _mode;
        bool _read_only = false;
        text_validator _validator;

        // Apply cached text to a created backend control.
        void apply_text();

        // Apply cached read-only state to a created backend control.
        void apply_read_only();

        // Return a copied portable form of the native selection.
        std::string selected_text() const;

        // Replace the native selection after complete-value validation.
        bool replace_selected_text(const std::string &text);

        // Select the complete native control value.
        void select_all_native() const;
    };
} // namespace native
