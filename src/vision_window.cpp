//
// Implements the main Vision feature showcase. It exercises native
// controls, dialogs, clipboard streams, images, fonts, and painters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    constexpr int command_save_image = 101;
    constexpr int command_open_font = 102;
    constexpr int command_exit = 103;
    constexpr int command_paste_text = 201;
    constexpr int command_select_all = 202;
    constexpr int command_copy_image = 203;
    constexpr int command_paste_image = 204;
    constexpr int command_modal = 301;
    constexpr int command_layout = 302;
    constexpr int command_collections = 303;
    constexpr int command_tables = 304;
    constexpr int command_code_editor = 305;
    constexpr int command_splitter = 306;
    constexpr int command_input_chrome = 307;
    constexpr int command_installed_font = 401;

    // Create and show a native child after its parent exists.
    void create_child(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    // Describe one portable modal result for the status area.
    const char *result_name(native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            return "accepted";
        if (result == native::dialog_result::cancelled)
            return "cancelled";
        return "none";
    }
} // namespace

namespace vision
{
    vision_window::vision_window(bool open_splitter_on_start,
                                 bool open_input_chrome_on_start)
        : native::app_wnd("Vision Native Feature Gallery",
                          36, 36, 820, 660)
        , _action("Activate", 20, 20, 120, 32)
        , _editing_enabled("Editing enabled", 20, 62, 150, 24)
        , _compact("Compact", 20, 94, 140, 24)
        , _detailed("Detailed", 20, 122, 140, 24)
        , _features({"Controls", "Images", "Fonts", "Clipboard"},
                    180, 20, 170, 130)
        , _single_line("Live validation",
                       native::text_edit_mode::single_line,
                       20, 170, 330, 28)
        , _multi_line("Multiline editor\nCtrl+C, X, and V work here.",
                      native::text_edit_mode::multi_line,
                      20, 210, 330, 112)
        , _copy_text("Copy field", 20, 334, 104, 30)
        , _paste_text("Paste text", 136, 334, 104, 30)
        , _open_file("Open file...", 20, 378, 84, 30)
        , _save_file("Save file...", 112, 378, 84, 30)
        , _show_modeless("Modeless", 204, 378, 84, 30)
        , _show_modal("Modal", 296, 378, 84, 30)
        , _show_layout("Layout managers...", 20, 566, 170, 30)
        , _show_collections("Collections...", 204, 566, 130, 30)
        , _show_tables("Tables...", 348, 566, 110, 30)
        , _show_code_editor("Code editor...", 472, 566, 130, 30)
        , _show_splitter("Split view...", 616, 566, 150, 30)
        , _show_input_chrome("Input and window chrome...",
                             20, 606, 210, 30)
        , _status("Starting portable feature gallery...")
        , _open_splitter_on_start(open_splitter_on_start)
        , _open_input_chrome_on_start(open_input_chrome_on_start)
        , _inspector(*this)
        , _layout(*this)
        , _collections(*this)
        , _tables(*this)
        , _code_editor(*this)
        , _splitter(*this)
        , _input_chrome(*this)
        , _dialog(*this)
        , _open_image(*this, "Open PNG or JPEG")
        , _save_image(*this, "Save PNG or JPEG")
        , _open_font(*this, "Open TrueType Font") {
        _editing_enabled.set_checked(true);
        _compact.set_selected(true);
        _features.set_selected_index(0);
        _single_line.set_validator([](const std::string &text) {
            return text.size() <= 36 && text.find('\t') ==
                                             std::string::npos;
        });

        configure_menu();
        configure_file_dialogs();
        connect_events();
    }

    void vision_window::configure_menu() {
        menu << "&File"
             << (native::menu_items("&Open image...\tCtrl+O")
                 << std::pair<int, std::string>(
                        command_save_image, "&Save image...\tCtrl+S")
                 << std::pair<int, std::string>(
                        command_open_font, "Load &TTF/OTF...")
                 << native::menu_separator
                 << std::pair<int, std::string>(
                        command_exit, "E&xit\tAlt+F4"))
             << "&Edit"
             << (native::menu_items("&Copy text\tCtrl+C")
                 << std::pair<int, std::string>(
                        command_paste_text, "&Paste text\tCtrl+V")
                 << std::pair<int, std::string>(
                        command_select_all, "Select &all\tCtrl+A")
                 << native::menu_separator
                 << std::pair<int, std::string>(
                        command_copy_image, "Copy &image")
                 << std::pair<int, std::string>(
                        command_paste_image, "Paste i&mage"))
             << "&Window"
             << (native::menu_items("&Modeless inspector")
                 << std::pair<int, std::string>(
                        command_modal, "Modal &dialog")
                 << native::menu_separator
                 << std::pair<int, std::string>(
                        command_layout, "&Layout managers")
                 << std::pair<int, std::string>(
                        command_collections, "&Collection controls")
                 << std::pair<int, std::string>(
                        command_tables, "&Advanced tables")
                 << std::pair<int, std::string>(
                        command_code_editor, "Code &editor")
                 << std::pair<int, std::string>(
                        command_splitter, "&Split view")
                 << std::pair<int, std::string>(
                        command_input_chrome,
                        "&Input and window chrome"))
             << "&Demo"
             << (native::menu_items("&Reset image")
                 << native::menu_separator
                 << std::pair<int, std::string>(
                        command_installed_font,
                        "Load first installed &font"));

        const auto &tops = menu.tops();
        _open_image_command = tops[0].items[0].id;
        _copy_text_command = tops[1].items[0].id;
        _modeless_command = tops[2].items[0].id;
        _reset_image_command = tops[3].items[0].id;
    }

    void vision_window::configure_file_dialogs() {
        _open_image.set_filters({
            {"Images", {"*.png", "*.jpg", "*.jpeg"}},
            {"PNG images", {"*.png"}},
            {"JPEG images", {"*.jpg", "*.jpeg"}},
            {"All files", {"*"}}
        });
        _save_image.set_filters(_open_image.get_filters());
        _save_image.set_suggested_name("vision.png")
            .set_default_extension("png")
            .set_confirm_overwrite(true);
        _open_font.set_filters({
            {"TrueType/OpenType fonts",
             {"*.ttf", "*.otf", "*.ttc"}},
            {"All files", {"*"}}
        });
    }

    void vision_window::connect_events() {
        on_wnd_create.connect(this, &vision_window::on_create);
        on_wnd_paint.connect(this, &vision_window::on_paint);
        on_menu.connect(this, &vision_window::on_menu_command);
        _action.on_click.connect(this, &vision_window::on_action);
        _editing_enabled.on_change.connect(
            this, &vision_window::on_editing_enabled);
        _compact.on_change.connect(this,
                                   &vision_window::on_mode_changed);
        _detailed.on_change.connect(this,
                                    &vision_window::on_mode_changed);
        _features.on_selection_change.connect(
            this, &vision_window::on_feature_selected);
        _single_line.on_change.connect(
            this, &vision_window::on_text_changed);
        _multi_line.on_change.connect(
            this, &vision_window::on_text_changed);
        _copy_text.on_click.connect(this, &vision_window::on_copy_text);
        _paste_text.on_click.connect(
            this, &vision_window::on_paste_text);
        _open_file.on_click.connect(
            this, &vision_window::on_open_file);
        _save_file.on_click.connect(
            this, &vision_window::on_save_file);
        _show_modeless.on_click.connect(
            this, &vision_window::on_show_modeless);
        _show_modal.on_click.connect(
            this, &vision_window::on_show_modal);
        _show_layout.on_click.connect(
            this, &vision_window::on_show_layout);
        _show_collections.on_click.connect(
            this, &vision_window::on_show_collections);
        _show_tables.on_click.connect(
            this, &vision_window::on_show_tables);
        _show_code_editor.on_click.connect(
            this, &vision_window::on_show_code_editor);
        _show_splitter.on_click.connect(
            this, &vision_window::on_show_splitter);
        _show_input_chrome.on_click.connect(
            this, &vision_window::on_show_input_chrome);
        _dialog.on_modal_close.connect(
            this, &vision_window::on_dialog_closed);
        _open_image.on_modal_close.connect(
            this, &vision_window::on_image_opened);
        _save_image.on_modal_close.connect(
            this, &vision_window::on_image_saved);
        _open_font.on_modal_close.connect(
            this, &vision_window::on_font_opened);
    }

    bool vision_window::on_create() {
        create_child(_action, *this);
        create_child(_editing_enabled, *this);
        create_child(_compact, *this);
        create_child(_detailed, *this);
        create_child(_features, *this);
        create_child(_single_line, *this);
        create_child(_multi_line, *this);
        create_child(_copy_text, *this);
        create_child(_paste_text, *this);
        create_child(_open_file, *this);
        create_child(_save_file, *this);
        create_child(_show_modeless, *this);
        create_child(_show_modal, *this);
        create_child(_show_layout, *this);
        create_child(_show_collections, *this);
        create_child(_show_tables, *this);
        create_child(_show_code_editor, *this);
        create_child(_show_splitter, *this);
        create_child(_show_input_chrome, *this);

        try {
            reset_image();
            load_first_installed_font();
        } catch (const std::exception &error) {
            set_status(std::string("Startup feature error: ") +
                       error.what());
        }
        if (_open_splitter_on_start)
            native::app::post([this] { show_splitter(); });
        if (_open_input_chrome_on_start)
            native::app::post([this] { show_input_chrome(); });
        return true;
    }

    bool vision_window::on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.set_font(
            native::font_t::stock(native::font_role::system));
        event.g.draw_text(
            "Validation: single-line input accepts 36 bytes.",
                          native::point(20, 418));
        event.g.draw_text(
            "Visible examples: file panels and owned windows.",
                          native::point(20, 440));

        const native::rect image_area(390, 30, 180, 120);
        if (_image) {
            const native::rect old_clip = event.g.get_clip();
            event.g.set_clip(old_clip.intersect(image_area));
            event.g.draw_img(*_image, image_area.p);
            event.g.set_clip(old_clip);
        }
        event.g.draw_rect(native::rect(389, 29, 182, 122));
        event.g.draw_text(
            "PNG memory: " + std::to_string(_png_size) +
                " bytes; JPEG memory: " +
                std::to_string(_jpeg_size) + " bytes",
            native::point(390, 158));

        if (_file_font.valid() && _memory_font.valid()) {
            event.g.set_font(_file_font);
            event.g.draw_text("File TTF: Vision Aa0",
                              native::point(390, 190));
            event.g.set_font(_memory_font);
            event.g.draw_text("Memory TTF: Vision Aa0",
                              native::point(390, 220));
            const native::text_metrics font_text =
                _file_font.measure_text("Vision Aa0");
            const native::text_metrics font_character =
                _file_font.measure_character(U'W');
            const native::text_metrics gpx_text =
                event.g.measure_text("Vision Aa0");
            const native::text_metrics gpx_character =
                event.g.measure_character(U'W');
            event.g.set_font(
                native::font_t::stock(native::font_role::system));
            event.g.draw_text(
                "Font/gpx text " +
                    std::to_string(font_text.width) + "/" +
                    std::to_string(gpx_text.width) +
                    "; W " +
                    std::to_string(font_character.advance) + "/" +
                    std::to_string(gpx_character.advance),
                native::point(390, 250));
        } else {
            event.g.draw_text("No usable installed TTF/OTF was found.",
                              native::point(390, 202));
        }

        std::unique_ptr<native::theme> appearance =
            native::theme::create(event.g);
        native::theme::state selected;
        selected.selected = true;
        appearance->draw_button(native::rect(390, 286, 150, 30),
                                "Theme primitive");
        appearance->draw_check(native::rect(390, 326, 160, 24),
                               "Custom check", selected);
        appearance->draw_radio(native::rect(390, 356, 160, 24),
                               "Custom radio", selected);
        appearance->draw_list(
            native::rect(570, 286, 210, 100),
            {"Themed list", "Native look", "Portable API"}, 1,
            selected);

        event.g.set_font(
            native::font_t::stock(native::font_role::system));
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Installed fonts: " +
                std::to_string(_installed_font_count) +
                "; active: " +
                (_font_name.empty()
                     ? std::string("stock")
                     : _font_name),
            native::point(20, 468));
        event.g.draw_text(
            "Image: " +
                (_image ? std::to_string(_image->w()) + "x" +
                              std::to_string(_image->h())
                        : std::string("none")),
            native::point(20, 494));
        event.g.draw_text("Status: " + _status,
                          native::point(20, 520));
        event.g.draw_text(
            "Menus: files, clipboard, owned windows, and demos.",
            native::point(20, 546));
        return true;
    }

    bool vision_window::on_menu_command(int command) {
        if (command == _open_image_command) {
            on_open_file();
        } else if (command == command_save_image) {
            on_save_file();
        } else if (command == command_open_font) {
            _open_font.create();
            _open_font.show();
        } else if (command == command_exit) {
            destroy();
        } else if (command == _copy_text_command) {
            on_copy_text();
        } else if (command == command_paste_text) {
            on_paste_text();
        } else if (command == command_select_all) {
            _multi_line.select_all();
            set_status("Selected all multiline editor text.");
        } else if (command == command_copy_image) {
            copy_image();
        } else if (command == command_paste_image) {
            paste_image();
        } else if (command == _modeless_command) {
            on_show_modeless();
        } else if (command == command_modal) {
            on_show_modal();
        } else if (command == command_layout) {
            on_show_layout();
        } else if (command == command_collections) {
            on_show_collections();
        } else if (command == command_tables) {
            on_show_tables();
        } else if (command == command_code_editor) {
            on_show_code_editor();
        } else if (command == command_splitter) {
            on_show_splitter();
        } else if (command == command_input_chrome) {
            on_show_input_chrome();
        } else if (command == _reset_image_command) {
            reset_image();
        } else if (command == command_installed_font) {
            load_first_installed_font();
        } else {
            return false;
        }
        return true;
    }

    bool vision_window::on_action() {
        ++_activation_count;
        set_status("Native button activations: " +
                   std::to_string(_activation_count));
        return true;
    }

    bool vision_window::on_editing_enabled(bool enabled) {
        _single_line.set_read_only(!enabled);
        _multi_line.set_read_only(!enabled);
        set_status(enabled ? "Text editing enabled."
                           : "Text editors are read-only.");
        return true;
    }

    bool vision_window::on_mode_changed(bool selected) {
        if (selected) {
            set_status(
                _compact.get_selected()
                    ? "Compact mode selected."
                    : "Detailed mode selected.");
        }
        return true;
    }

    bool vision_window::on_feature_selected(int index) {
        const auto &items = _features.get_items();
        if (index >= 0 &&
            static_cast<std::size_t>(index) < items.size()) {
            set_status("Selected native list item: " +
                       items[static_cast<std::size_t>(index)]);
        }
        return true;
    }

    bool vision_window::on_text_changed(std::string text) {
        set_status("Validated text change: " +
                   std::to_string(text.size()) + " UTF-8 bytes.");
        return true;
    }

    bool vision_window::on_copy_text() {
        _single_line.select_all();
        set_status(_single_line.copy()
                       ? "Copied single-line text directly."
                       : "No text was available to copy.");
        return true;
    }

    bool vision_window::on_paste_text() {
        _multi_line.select_all();
        set_status(_multi_line.paste()
                       ? "Pasted text directly into multiline editor."
                       : "Clipboard text was rejected or unavailable.");
        return true;
    }

    bool vision_window::on_open_file() {
        _open_image.create();
        _open_image.show();
        return true;
    }

    bool vision_window::on_save_file() {
        _save_image.create();
        _save_image.show();
        return true;
    }

    bool vision_window::on_show_modeless() {
        show_inspector();
        return true;
    }

    bool vision_window::on_show_modal() {
        show_dialog();
        return true;
    }

    bool vision_window::on_show_layout() {
        show_layout();
        return true;
    }

    bool vision_window::on_show_collections() {
        show_collections();
        return true;
    }

    bool vision_window::on_show_tables() {
        show_tables();
        return true;
    }

    bool vision_window::on_show_code_editor() {
        show_code_editor();
        return true;
    }

    bool vision_window::on_show_splitter() {
        show_splitter();
        return true;
    }

    bool vision_window::on_show_input_chrome() {
        show_input_chrome();
        return true;
    }

    bool vision_window::on_dialog_closed(
        native::dialog_result result) {
        set_status(std::string("Modal dialog result: ") +
                   result_name(result));
        return true;
    }

} // namespace vision
