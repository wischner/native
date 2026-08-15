//
// Implements the Haiku theme with BControlLook. Non-window targets use a
// backend-local Be-style emulation based on the current system colors.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <Alignment.h>
#include <ControlLook.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <Looper.h>
#include <Region.h>
#include <View.h>

#include <native.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace
{
    native::rgba from_native(rgb_color color) {
        return native::rgba(color.red, color.green, color.blue, color.alpha);
    }

    BView *view_from(native::gpx &g) {
        auto *window_gpx = dynamic_cast<native::gpx_wnd *>(&g);
        if (!window_gpx)
            return nullptr;
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(
            window_gpx->window());
        return cache ? cache->view : nullptr;
    }

    uint32 flags_from(const native::theme::state &state) {
        uint32 flags = 0;
        if (state.hot)
            flags |= BPrivate::BControlLook::B_HOVER;
        if (state.pressed)
            flags |= BPrivate::BControlLook::B_CLICKED;
        if (state.selected)
            flags |= BPrivate::BControlLook::B_ACTIVATED;
        if (state.disabled)
            flags |= BPrivate::BControlLook::B_DISABLED;
        return flags;
    }

    template<typename function_type>
    bool with_view(native::gpx &g, function_type &&function) {
        BView *view = view_from(g);
        BLooper *looper = view ? view->Looper() : nullptr;
        if (!view || !looper || !be_control_look)
            return false;
        const bool was_locked = looper->IsLocked();
        if (!was_locked && !looper->Lock())
            return false;

        view->PushState();
        const native::rect clip = g.get_clip();
        BRegion region(BRect(clip.p.x, clip.p.y, clip.x2() - 1, clip.y2() - 1));
        view->ConstrainClippingRegion(&region);
        function(view);
        view->PopState();

        if (!was_locked)
            looper->Unlock();
        return true;
    }

    class saved_state
    {
    public:
        explicit saved_state(native::gpx &g)
            : _g(g), _ink(g.get_ink()), _paper(g.get_paper()),
              _pen(g.get_pen()), _font(g.get_font()), _clip(g.get_clip()) {}
        ~saved_state() {
            _g.set_ink(_ink).set_paper(_paper).set_pen(_pen).set_font(_font);
            _g.set_clip(_clip);
        }
    private:
        native::gpx &_g;
        native::rgba _ink;
        native::rgba _paper;
        std::uint8_t _pen;
        const native::font_t &_font;
        native::rect _clip;
    };

    class haiku_theme final : public native::theme
    {
    public:
        explicit haiku_theme(native::gpx &g) : theme(g) {}

        metrics defaults() const override {
            metrics m;
            font_height height{};
            be_plain_font->GetHeight(&height);
            const int text_height = std::max(
                1,
                static_cast<int>(height.ascent + height.descent + height.leading));
            m.menu_bar_height = std::max(20, text_height + 8);
            m.menu_item_height = std::max(18, text_height + 4);
            m.popup_width = static_cast<int>(be_plain_font->StringWidth("MMMMMMMMMMMMMMMMMMMM")) + 16;
            m.text_padding_x = 8;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = from_native(ui_color(B_CONTROL_BACKGROUND_COLOR));
            p.button_border = from_native(ui_color(B_CONTROL_BORDER_COLOR));
            p.button_highlight = from_native(ui_color(B_SHINE_COLOR));
            p.button_shadow = from_native(ui_color(B_SHADOW_COLOR));
            p.button_text = from_native(ui_color(B_CONTROL_TEXT_COLOR));
            p.button_disabled_text = from_native(
                tint_color(ui_color(B_CONTROL_TEXT_COLOR), B_DISABLED_LABEL_TINT));
            p.button_hot_bg = from_native(ui_color(B_CONTROL_HIGHLIGHT_COLOR));
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = from_native(
                tint_color(ui_color(B_CONTROL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg = from_native(ui_color(B_MENU_BACKGROUND_COLOR));
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = from_native(ui_color(B_MENU_ITEM_TEXT_COLOR));
            p.menu_disabled_text = from_native(
                tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR), B_DISABLED_LABEL_TINT));
            p.menu_hot_bg = from_native(ui_color(B_MENU_SELECTED_BACKGROUND_COLOR));
            p.menu_hot_text = from_native(ui_color(B_MENU_SELECTED_ITEM_TEXT_COLOR));
            p.menu_popup_bg = p.menu_bar_bg;
            p.menu_popup_border = from_native(ui_color(B_MENU_SELECTED_BORDER_COLOR));
            return p;
        }

        theme &draw_button(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base = ui_color(B_CONTROL_BACKGROUND_COLOR);
                const rgb_color background = ui_color(B_PANEL_BACKGROUND_COLOR);
                const uint32 flags = flags_from(s);
                const BRect update(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawButtonFrame(
                    view, content, update, base, background, flags);
                be_control_look->DrawButtonBackground(
                    view, content, update, base, flags);
                const rgb_color text_color = s.disabled
                    ? tint_color(ui_color(B_CONTROL_TEXT_COLOR), B_DISABLED_LABEL_TINT)
                    : ui_color(B_CONTROL_TEXT_COLOR);
                be_control_look->DrawLabel(
                    view,
                    text.c_str(),
                    content,
                    update,
                    base,
                    flags,
                    BAlignment(B_ALIGN_CENTER, B_ALIGN_MIDDLE),
                    &text_color);
            });
            return painted ? *this : draw_button_fallback(r, text, s);
        }

        theme &draw_menu_bar(const native::rect &r) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);
                const BRect update(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawMenuBarBackground(
                    view, content, update, base);
            });
            return painted ? *this : draw_menu_bar_fallback(r);
        }

        theme &draw_menu_title(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, true);
        }

        theme &draw_menu_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_popup_frame(const native::rect &r) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);
                const BRect update(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawMenuBackground(
                    view, content, update, base);
            });
            return painted ? *this : draw_popup_fallback(r);
        }

        theme &draw_list_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

    private:
        int baseline(const native::rect &r) const {
            font_height height{};
            be_plain_font->GetHeight(&height);
            return r.p.y + static_cast<int>(
                (r.d.h - height.ascent - height.descent) / 2 + height.ascent);
        }

        theme &draw_button_fallback(
            const native::rect &r,
            const std::string &text,
            const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            const native::rgba background = s.pressed
                ? p.button_pressed_bg
                : (s.hot ? p.button_hot_bg : p.button_bg);
            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            _g.set_ink(p.button_border).draw_rect(r, false);
            const int width = static_cast<int>(be_plain_font->StringWidth(text.c_str()));
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text : p.button_text)
                .draw_text(
                    text,
                    native::point(
                        r.p.x + std::max(0, (static_cast<int>(r.d.w) - width) / 2),
                        baseline(r)));
            return *this;
        }

        theme &draw_menu_bar_fallback(const native::rect &r) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_bar_bg).draw_rect(r, true);
            if (r.d.w && r.d.h) {
                _g.set_ink(p.menu_bar_line_bottom).draw_line(
                    native::point(r.p.x, r.y2() - 1),
                    native::point(r.x2() - 1, r.y2() - 1));
            }
            return *this;
        }

        theme &draw_menu_entry(
            const native::rect &r,
            const std::string &text,
            const state &s,
            bool title) {
            const bool active = s.hot || s.selected;
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);
                const uint32 flags = flags_from(s);
                const BRect update(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                if (!title || active) {
                    be_control_look->DrawMenuItemBackground(
                        view, content, update, base, flags);
                }
                const rgb_color text_color = s.disabled
                    ? tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR), B_DISABLED_LABEL_TINT)
                    : ui_color(active
                        ? B_MENU_SELECTED_ITEM_TEXT_COLOR
                        : B_MENU_ITEM_TEXT_COLOR);
                content.left += defaults().text_padding_x;
                be_control_look->DrawLabel(
                    view,
                    text.c_str(),
                    content,
                    update,
                    base,
                    flags,
                    BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE),
                    &text_color);
            });
            return painted
                ? *this
                : draw_menu_entry_fallback(r, text, s, title);
        }

        theme &draw_menu_entry_fallback(
            const native::rect &r,
            const std::string &text,
            const state &s,
            bool title) {
            saved_state saved(_g);
            const palette p = native_palette();
            const bool active = s.hot || s.selected;
            _g.set_pen(1)
                .set_ink(active ? p.menu_hot_bg
                                : (title ? p.menu_bar_bg : p.menu_popup_bg))
                .draw_rect(r, true);
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled
                           ? p.menu_disabled_text
                           : (active ? p.menu_hot_text : p.menu_text))
                .draw_text(
                    text,
                    native::point(r.p.x + defaults().text_padding_x, baseline(r)));
            return *this;
        }

        theme &draw_popup_fallback(const native::rect &r) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            _g.set_ink(p.menu_popup_border).draw_rect(r, false);
            return *this;
        }
    };
}

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<haiku_theme>(painter);
    }
}
