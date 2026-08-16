//
// Declares the Vision feature window and its independent modeless and
// modal children. All objects use only the public Native interface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <native.h>

namespace vision
{
    // Shows custom themed primitives without blocking the main window.
    class feature_inspector final : public native::modeless_wnd
    {
    public:
        // Construct an independently positioned modeless inspector.
        explicit feature_inspector(native::app_wnd &owner);

    private:
        // Paint the modeless-window explanation and theme samples.
        bool on_paint(native::wnd_paint_event event);
    };

    // Demonstrates owner modality and explicit dialog results.
    class feature_dialog final : public native::modal_wnd
    {
    public:
        // Construct the reusable modal dialog and its child buttons.
        explicit feature_dialog(native::app_wnd &owner);

    private:
        native::button _accept;
        native::button _cancel;

        // Create and show the dialog's native child controls.
        bool on_create();

        // Paint explanatory content behind the native buttons.
        bool on_paint(native::wnd_paint_event event);

        // Close the dialog with an accepted result.
        bool on_accept();

        // Close the dialog with a cancelled result.
        bool on_cancel();
    };

    // Presents every portable control, service, and drawing surface.
    class vision_window final : public native::app_wnd
    {
    public:
        // Construct the main demonstration and connect all events.
        vision_window();

    private:
        native::button _action;
        native::check _editing_enabled;
        native::radio _compact;
        native::radio _detailed;
        native::list _features;
        native::text_edit _single_line;
        native::text_edit _multi_line;
        native::button _copy_text;
        native::button _paste_text;
        native::button _open_file;
        native::button _save_file;
        native::button _show_modeless;
        native::button _show_modal;

        std::unique_ptr<native::img> _image;
        native::font_t _file_font;
        native::font_t _memory_font;
        std::string _font_name;
        std::string _status;
        std::size_t _installed_font_count = 0;
        std::size_t _png_size = 0;
        std::size_t _jpeg_size = 0;
        int _activation_count = 0;

        int _open_image_command = 0;
        int _copy_text_command = 0;
        int _modeless_command = 0;
        int _reset_image_command = 0;

        // Owned windows are last so they are destroyed before state
        // observed by their modal-close callbacks.
        feature_inspector _inspector;
        feature_dialog _dialog;
        native::open_file_dialog _open_image;
        native::save_file_dialog _save_image;
        native::open_file_dialog _open_font;

        // Populate the portable menu and remember automatic IDs.
        void configure_menu();

        // Configure the native image and font chooser properties.
        void configure_file_dialogs();

        // Connect every signal after all member objects exist.
        void connect_events();

        // Create the main window's native child controls.
        bool on_create();

        // Paint images, fonts, metrics, and theme primitives.
        bool on_paint(native::wnd_paint_event event);

        // Handle one command from the portable menu model.
        bool on_menu_command(int command);

        // Record an activation from the demonstration button.
        bool on_action();

        // Toggle read-only state on both text editors.
        bool on_editing_enabled(bool enabled);

        // Repaint after a radio selection changes.
        bool on_mode_changed(bool selected);

        // Report a native list selection.
        bool on_feature_selected(int index);

        // Report a live-validated text change.
        bool on_text_changed(std::string text);

        // Copy the single-line editor through its direct API.
        bool on_copy_text();

        // Paste clipboard text through the multiline editor API.
        bool on_paste_text();

        // Present the native file-open example.
        bool on_open_file();

        // Present the native file-save example.
        bool on_save_file();

        // Present the independently positioned modeless example.
        bool on_show_modeless();

        // Present the owner-blocking modal dialog example.
        bool on_show_modal();

        // Process completion of the modal demonstration dialog.
        bool on_dialog_closed(native::dialog_result result);

        // Load the image selected by the native open panel.
        bool on_image_opened(native::dialog_result result);

        // Save the current image after native save-panel completion.
        bool on_image_saved(native::dialog_result result);

        // Load the TrueType face selected by the native open panel.
        bool on_font_opened(native::dialog_result result);

        // Draw, encode, and decode the procedural demonstration image.
        bool reset_image();

        // Load one face through both the file and memory font APIs.
        bool load_font(const std::string &path,
                       std::uint32_t face_index = 0);

        // Enumerate installed faces and load the first usable one.
        bool load_first_installed_font();

        // Copy the current image and text representation atomically.
        void copy_image();

        // Read an image representation from the system clipboard.
        void paste_image();

        // Open and show the independent modeless inspector.
        void show_inspector();

        // Open and show the owner-modal demonstration dialog.
        void show_dialog();

        // Replace the visible status message and request repainting.
        void set_status(const std::string &status);
    };
} // namespace vision
