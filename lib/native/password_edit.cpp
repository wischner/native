//
// Implements the shared secret field. It offers no copy, no selection
// and no reveal: the value leaves only through get_text(), and the
// destructor overwrites it so the bytes do not linger on the heap.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>

#include <native/clipboard.h>
#include <native/font.h>
#include <native/graphics.h>
#include <native/password_edit.h>

namespace native
{
    namespace
    {
        constexpr int field_inset = 6;
        constexpr int bullet_size = 6;
        constexpr int bullet_advance = 9;

        // Report whether a byte starts a UTF-8 character.
        bool starts_character(char c) {
            return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
        }

        // Overwrite a string in a way the optimizer must keep.
        void wipe(std::string &text) {
            volatile char *bytes = text.data();
            for (std::size_t i = 0; i < text.size(); ++i)
                bytes[i] = '\0';
            text.clear();
        }

        // Report whether text holds a control character.
        bool has_control(const std::string &text) {
            return std::any_of(text.begin(), text.end(), [](char c) {
                const auto byte = static_cast<unsigned char>(c);
                return byte < 32 || byte == 127;
            });
        }
    }

    password_edit::password_edit(coord x, coord y, dim width, dim height)
        : canvas(x, y, width, height) {
        set_cursor(mouse_cursor::ibeam);
        set_horizontal_scrollbar_policy(scrollbar_policy::never);
        set_vertical_scrollbar_policy(scrollbar_policy::never);
        on_wnd_paint.connect([this](wnd_paint_event event) {
            paint(event);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            if (event.action == mouse_action::press)
                focus();
            return true;
        });
    }

    password_edit::~password_edit() {
        wipe(_text);
    }

    const std::string &password_edit::get_text() const {
        return _text;
    }

    password_edit &password_edit::set_text(const std::string &text) {
        wipe(_text);
        _text = text;
        invalidate();
        return *this;
    }

    std::size_t password_edit::get_length() const {
        return static_cast<std::size_t>(
            std::count_if(_text.begin(), _text.end(), starts_character));
    }

    bool password_edit::append(const std::string &utf8) {
        if (utf8.empty() || has_control(utf8))
            return false;
        _text += utf8;
        invalidate();
        on_change.emit(_text);
        return true;
    }

    bool password_edit::on_native_text_input(const std::string &utf8) {
        return append(utf8) || canvas::on_native_text_input(utf8);
    }

    bool password_edit::on_native_key(key_event event) {
        const bool control =
            (event.modifiers & key_modifiers::ctrl) != key_modifiers::none;
        if (event.action != key_action::press)
            return canvas::on_native_key(event);
        if (event.key == key_code::backspace) {
            if (_text.empty())
                return true;
            while (!_text.empty() && !starts_character(_text.back()))
                _text.pop_back();
            if (!_text.empty())
                _text.pop_back();
            invalidate();
            on_change.emit(_text);
            return true;
        }
        if (control && event.key == key_code::v) {
            const clipboard source = clipboard::open_read();
            std::string pasted = source.read_text();
            std::erase_if(pasted, [](char c) {
                return c == '\n' || c == '\r';
            });
            append(pasted);
            wipe(pasted);
            return true;
        }
        return canvas::on_native_key(event);
    }

    void password_edit::on_native_focus(bool focused) {
        canvas::on_native_focus(focused);
        _focused = focused;
        invalidate();
    }

    void password_edit::paint(wnd_paint_event event) {
        auto appearance = theme::create(event.g);
        const size extent = get_dimensions();
        theme::state state;
        state.focused = _focused;
        draw_control(event.g, *appearance,
                     rect(0, 0, extent.w, extent.h), state);
    }

    void password_edit::draw_control(gpx &graphics,
                                     theme &appearance,
                                     const rect &bounds,
                                     const theme::state &state) {
        appearance.draw_text_edit_frame(bounds, state);
        const int middle = bounds.p.y + static_cast<int>(bounds.d.h) / 2;
        const int room = static_cast<int>(bounds.d.w) - 2 * field_inset;
        const int fitting = std::max(0, room / bullet_advance);
        const std::size_t shown = std::min(
            get_length(), static_cast<std::size_t>(fitting));
        graphics.set_ink(appearance.get_content_foreground_color());
        int x = bounds.p.x + field_inset;
        for (std::size_t i = 0; i < shown; ++i, x += bullet_advance)
            graphics.draw_ellipse(
                rect(static_cast<coord>(x),
                     static_cast<coord>(middle - bullet_size / 2),
                     bullet_size, bullet_size),
                true);
        if (state.focused) {
            graphics.set_pen(1);
            graphics.draw_line(
                point(static_cast<coord>(x), static_cast<coord>(middle - 7)),
                point(static_cast<coord>(x), static_cast<coord>(middle + 7)));
        }
    }
}
