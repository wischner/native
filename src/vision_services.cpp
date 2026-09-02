//
// Implements Vision demonstrations for image codecs, portable fonts,
// clipboard streams, file dialogs, and independently owned windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <array>
#include <exception>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t maximum_font_size =
        128U * 1024U * 1024U;

    // Read a reasonably bounded complete binary file into memory.
    bool read_file(const std::string &path,
                   std::vector<std::uint8_t> &bytes) {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            return false;

        const std::streamoff length = stream.tellg();
        if (length <= 0 ||
            static_cast<std::uintmax_t>(length) > maximum_font_size)
            return false;
        if (length > std::numeric_limits<std::streamsize>::max())
            return false;

        bytes.resize(static_cast<std::size_t>(length));
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char *>(bytes.data()),
                    static_cast<std::streamsize>(length));
        return static_cast<bool>(stream);
    }

    // Return the final component of a platform path for display.
    std::string file_name(const std::string &path) {
        const std::string::size_type separator =
            path.find_last_of("/\\");
        if (separator == std::string::npos)
            return path;
        return path.substr(separator + 1);
    }
} // namespace

namespace vision
{
    bool vision_window::on_image_opened(
        native::dialog_result result) {
        if (result != native::dialog_result::accepted) {
            set_status("Open-image dialog cancelled.");
            return true;
        }
        try {
            _image.reset(new native::img(
                native::img::load(_open_image.get_path())));
            _png_size =
                _image->encode(native::image_format::png).size();
            _jpeg_size =
                _image->encode(native::image_format::jpeg).size();
            set_status("Loaded image file: " +
                       file_name(_open_image.get_path()));
        } catch (const std::exception &error) {
            set_status(
                std::string("Image load failed: ") + error.what());
        }
        return true;
    }

    bool vision_window::on_image_saved(
        native::dialog_result result) {
        if (result != native::dialog_result::accepted) {
            set_status("Save-image dialog cancelled.");
            return true;
        }
        if (!_image)
            return false;
        try {
            _image->save(_save_image.get_path());
            set_status("Saved image file: " +
                       file_name(_save_image.get_path()));
        } catch (const std::exception &error) {
            set_status(
                std::string("Image save failed: ") + error.what());
        }
        return true;
    }

    bool vision_window::on_font_opened(
        native::dialog_result result) {
        if (result != native::dialog_result::accepted) {
            set_status("Open-font dialog cancelled.");
            return true;
        }
        load_font(_open_font.get_path());
        return true;
    }

    bool vision_window::reset_image() {
        try {
            native::img source(180, 120);
            native::gpx &g = source.get_gpx();
            std::unique_ptr<native::theme> appearance =
                native::theme::create(g);
            g.clear(appearance->native_palette().button_bg);
            g.set_ink(native::rgba(42, 94, 170, 255));
            g.draw_rect(native::rect(0, 0, 180, 32), true);
            g.set_ink(native::rgba(237, 135, 46, 255));
            g.draw_rect(native::rect(0, 32, 60, 48), true);
            g.set_ink(native::rgba(62, 160, 95, 255));
            g.draw_rect(native::rect(60, 32, 60, 48), true);
            g.set_ink(native::rgba(150, 86, 178, 255));
            g.draw_rect(native::rect(120, 32, 60, 48), true);
            g.set_font(
                native::font_t::stock(native::font_role::control));
            g.set_ink(native::rgba(255, 255, 255, 255));
            g.draw_text("gpx_img", native::point(58, 8));

            appearance->draw_button(native::rect(12, 86, 156, 26),
                                    "Image painter");

            const std::vector<std::uint8_t> png =
                source.encode(native::image_format::png);
            const std::vector<std::uint8_t> jpeg =
                source.encode(native::image_format::jpeg, 88);
            native::img jpeg_check =
                native::img::decode(jpeg.data(), jpeg.size());
            if (jpeg_check.w() != source.w() ||
                jpeg_check.h() != source.h()) {
                throw std::runtime_error(
                    "JPEG round trip changed image dimensions");
            }

            _image.reset(new native::img(
                native::img::decode(png.data(), png.size())));
            _png_size = png.size();
            _jpeg_size = jpeg.size();
            set_status(
                "gpx_img, PNG, and JPEG memory round trips passed.");
            return true;
        } catch (const std::exception &error) {
            set_status(
                std::string("Image demo failed: ") + error.what());
            return false;
        }
    }

    bool vision_window::load_font(const std::string &path,
                                  std::uint32_t face_index) {
        std::vector<std::uint8_t> bytes;
        if (!read_file(path, bytes)) {
            set_status("Could not read font file: " + file_name(path));
            return false;
        }

        native::font_t file_font =
            native::font_t::from_file(path, 20, face_index);
        native::font_t memory_font = native::font_t::from_memory(
            bytes.data(), bytes.size(), 20, face_index);
        if (!file_font.valid() || !memory_font.valid()) {
            set_status("Font file or memory creation failed: " +
                       file_name(path));
            return false;
        }

        _font_name = file_font.spec().family;
        if (_font_name.empty())
            _font_name = file_name(path);
        _file_font = std::move(file_font);
        _memory_font = std::move(memory_font);
        set_status("Loaded TTF/OTF from file and copied memory: " +
                   _font_name);
        return true;
    }

    bool vision_window::load_first_installed_font() {
        const std::vector<native::font_description> fonts =
            native::font_t::enumerate_installed();
        _installed_font_count = fonts.size();

        // macOS exposes private, purpose-built faces such as
        // ".ADT Slab Numeric" in its font directories.  They are valid
        // fonts, but most letters map to the missing-glyph box.  Prefer
        // a normal text family so this sample genuinely exercises both
        // file-backed and memory-backed TrueType rendering.
        constexpr std::array<const char *, 8> preferred_families = {
            "Arial",
            "Helvetica Neue",
            "Helvetica",
            "SF Pro Text",
            "Segoe UI",
            "DejaVu Sans",
            "Liberation Sans",
            "Noto Sans"};
        for (const char *family : preferred_families) {
            for (const native::font_description &font : fonts) {
                if (font.family == family && !font.path.empty() &&
                    load_font(font.path, font.face_index)) {
                    return true;
                }
            }
        }

        for (const native::font_description &font : fonts) {
            if (!font.family.empty() && font.family.front() != '.' &&
                !font.path.empty() &&
                load_font(font.path, font.face_index))
                return true;
        }
        set_status(
            "No installed TrueType/OpenType face could be loaded.");
        return false;
    }

    void vision_window::copy_image() {
        if (!_image) {
            set_status("There is no image to copy.");
            return;
        }
        try {
            native::clipboard output = native::clipboard::open_write();
            output.write_text("Vision image from the Native demo")
                .write_image(*_image)
                .commit();
            set_status(
                "Copied text and image clipboard formats atomically.");
        } catch (const std::exception &error) {
            set_status(std::string("Clipboard copy failed: ") +
                       error.what());
        }
    }

    void vision_window::paste_image() {
        try {
            native::clipboard input = native::clipboard::open_read();
            if (!input.has(native::clipboard_format::image)) {
                set_status("Clipboard has no portable image format.");
                return;
            }
            _image.reset(new native::img(input.read_image()));
            _png_size =
                _image->encode(native::image_format::png).size();
            _jpeg_size =
                _image->encode(native::image_format::jpeg).size();
            set_status("Pasted image from a clipboard snapshot.");
        } catch (const std::exception &error) {
            set_status(std::string("Clipboard paste failed: ") +
                       error.what());
        }
    }

    void vision_window::show_inspector() {
        if (!_inspector.get_created())
            _inspector.create();
        _inspector.show();
        set_status("Opened an independent modeless owned window.");
    }

    void vision_window::show_dialog() {
        if (!_dialog.get_created())
            _dialog.create();
        _dialog.show();
        set_status("Modal dialog is blocking its owner.");
    }

    void vision_window::show_layout() {
        if (!_layout.get_created())
            _layout.create();
        _layout.show();
        set_status("Layout window: resize it to watch the grid "
                   "arrange its children.");
    }

    void vision_window::show_collections() {
        if (!_collections.get_created())
            _collections.create();
        _collections.show();
        set_status("Opened the accordion, icon-view, and classic "
                   "tree-view gallery.");
    }

    void vision_window::show_tables() {
        if (!_tables.get_created())
            _tables.create();
        _tables.show();
        set_status("Opened materialized and million-row virtual "
                   "tables.");
    }

    void vision_window::show_code_editor() {
        if (!_code_editor.get_created())
            _code_editor.create();
        _code_editor.show();
        set_status("Opened the UTF-8 code editor with portable "
                   "gutter and overlays.");
    }

    void vision_window::show_splitter() {
        if (!_splitter.get_created())
            _splitter.create();
        _splitter.show();
        set_status("Opened the native two-pane split view.");
    }

    void vision_window::show_input_chrome() {
        if (!_input_chrome.get_created())
            _input_chrome.create();
        _input_chrome.show();
        set_status("Opened combo, list-box, ruler, status-bar, and "
                   "standard-dialog examples.");
    }

    void vision_window::set_status(const std::string &status) {
        _status = status;
        if (get_created())
            invalidate();
    }
} // namespace vision
