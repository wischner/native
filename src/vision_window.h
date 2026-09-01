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
#include <vector>

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

    // Demonstrates how layout managers arrange child controls.
    class feature_layout final : public native::modeless_wnd
    {
    public:
        // Construct the resizable layout demonstration window.
        explicit feature_layout(native::app_wnd &owner);

    private:
        native::button _toggle;
        native::button _sidebar;
        native::button _status;
        native::button _cell_1;
        native::button _cell_2;
        native::button _cell_3;
        native::button _cell_4;
        bool _using_grid = true;

        // Create the child controls and install the first layout.
        bool on_create();

        // Describe the installed layout above the arranged controls.
        bool on_paint(native::wnd_paint_event event);

        // Swap between the grid and absolute layout managers.
        bool on_toggle();

        // Install a grid of fixed and weighted tracks with a nested
        // grid in its content cell.
        void apply_grid_layout();

        // Install an absolute layout and restore explicit bounds.
        void apply_absolute_layout();
    };

    // Demonstrates disclosure sections containing scrolling icon grids.
    class feature_collections final : public native::modeless_wnd
    {
    public:
        // Construct the portable libraries-pane demonstration.
        explicit feature_collections(native::app_wnd &owner);

    private:
        // Content controls precede the borrowing accordion so the
        // accordion is destroyed and detaches them first.
        native::icon_view _shapes;
        native::icon_view _colors;
        native::icon_view _backgrounds;
        native::accordion _libraries;
        native::tree_view _tree;
        std::string _status;

        // Create the accordion; it creates only the expanded content.
        bool on_create();

        // Paint usage guidance and the last user action.
        bool on_paint(native::wnd_paint_event event);

        // Report a user-originated accordion expansion change.
        bool on_expanded(int index);

        // Report a user-originated icon selection.
        bool on_selected(int index);

        // Report a user-originated icon activation.
        bool on_activated(int index);

        // Report a user-originated tree selection.
        bool on_tree_selected(native::tree_item_id id);

        // Report a user-originated tree expansion change.
        bool on_tree_expanded(native::tree_item_id id, bool expanded);

        // Report a user-originated tree activation.
        bool on_tree_activated(native::tree_item_id id);
    };

    // Demonstrates materialized and million-row virtual tables.
    class feature_tables final : public native::modeless_wnd
    {
    public:
        // Construct the advanced table-view demonstration.
        explicit feature_tables(native::app_wnd &owner);

    private:
        std::vector<std::shared_ptr<const native::img>> _images;
        native::table_store _store;
        std::unique_ptr<native::table_model> _million_model;
        native::table_view _table;
        native::table_view _million_table;
        native::check _alternating;
        native::check _grid;
        native::check _multiple;
        native::text_edit _search;
        native::button _find;
        native::button _scroll;
        std::string _status;

        // Create and show both model-backed table controls.
        bool on_create();

        // Paint guidance and the most recent table action.
        bool on_paint(native::wnd_paint_event event);

        // Apply the alternating-row preference to both tables.
        bool on_alternating(bool enabled);

        // Apply the cell-grid preference to both tables.
        bool on_grid(bool enabled);

        // Apply single or multiple selection to both tables.
        bool on_multiple(bool enabled);

        // Find and reveal text in the materialized demonstration.
        bool on_find();

        // Jump deep into the million-row virtual model.
        bool on_scroll();

        // Report user selection from either table.
        bool on_selection(
            const std::vector<native::table_row_id> &rows);

        // Display a user-requested sort indicator.
        bool on_sort(native::table_sort sort);

        // Report a user-changed group disclosure.
        bool on_group(native::table_group_id group, bool expanded);
    };

    // Demonstrates portable source editing and application overlays.
    class feature_code_editor final : public native::modeless_wnd
    {
    public:
        // Construct the source-editor demonstration.
        explicit feature_code_editor(native::app_wnd &owner);

    private:
        std::unique_ptr<native::code_lexer> _lexer;
        native::code_edit _editor;
        native::button _show_completion;
        std::string _status;

        // Create and show the source editor and completion button.
        bool on_create();

        // Paint usage guidance and the latest editor action.
        bool on_paint(native::wnd_paint_event event);

        // Toggle an application-owned breakpoint for a gutter line.
        bool on_gutter(int line);

        // Report document changes from editing commands.
        bool on_text_change();

        // Display application-provided completion choices.
        bool on_show_completion();

        // Insert one accepted application completion.
        bool on_completion(native::completion_item item);
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
        native::button _show_layout;
        native::button _show_collections;
        native::button _show_tables;
        native::button _show_code_editor;

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
        feature_layout _layout;
        feature_collections _collections;
        feature_tables _tables;
        feature_code_editor _code_editor;
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

        // Present the resizable layout-manager example.
        bool on_show_layout();

        // Present the accordion and icon-view demonstration.
        bool on_show_collections();

        // Present the advanced materialized and virtual tables.
        bool on_show_tables();

        // Present the portable source-editor demonstration.
        bool on_show_code_editor();

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

        // Open and show the resizable layout demonstration window.
        void show_layout();

        // Open and show the reusable collection-controls window.
        void show_collections();

        // Open and show the advanced table-view demonstration.
        void show_tables();

        // Open and show the source-editor demonstration.
        void show_code_editor();

        // Replace the visible status message and request repainting.
        void set_status(const std::string &status);
    };
} // namespace vision
