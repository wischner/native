//
// Implements the Haiku theme with BControlLook. Non-window targets use
// a backend-local Be-style emulation based on the current system
// colors.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <Alignment.h>
#include <Application.h>
#include <ControlLook.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <Looper.h>
#include <Region.h>
#include <View.h>

#include <native.h>
#include <native/theme.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace
{
    native::rgba from_native(rgb_color color) {
        return native::rgba(
            color.red, color.green, color.blue, color.alpha);
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

    template <typename function_type>
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
        BRegion region(
            BRect(clip.p.x, clip.p.y, clip.x2() - 1, clip.y2() - 1));
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
            : _g(g)
            , _ink(g.get_ink())
            , _paper(g.get_paper())
            , _pen(g.get_pen())
            , _font(g.get_font())
            , _clip(g.get_clip()) {}
        ~saved_state() {
            _g.set_ink(_ink).set_paper(_paper).set_pen(_pen).set_font(
                _font);
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
        explicit haiku_theme(native::gpx &g)
            : theme(g) {}

        metrics defaults() const override {
            metrics m;
            if (!be_app) {
                const native::font_t &font = native::font_t::stock(
                    native::font_role::control);
                const native::font_metrics font_metrics =
                    font.get_metrics();
                const int height = std::max(1, font_metrics.height);
                m.menu_bar_height = std::max(20, height + 8);
                m.menu_item_height = std::max(18, height + 4);
                m.header_height = std::max(20, height + 8);
                m.popup_width =
                    font.measure_text("MMMMMMMMMMMMMMMMMMMM").width +
                    16;
                m.text_padding_x = 8;
                m.list_item_height = height + 2;
                return m;
            }
            font_height height{};
            be_plain_font->GetHeight(&height);
            const int text_height = std::max(
                1,
                static_cast<int>(height.ascent + height.descent +
                                 height.leading));
            m.menu_bar_height = std::max(20, text_height + 8);
            m.menu_item_height = std::max(18, text_height + 4);
            m.header_height = std::max(20, text_height + 8);
            m.popup_width = static_cast<int>(be_plain_font->StringWidth(
                                "MMMMMMMMMMMMMMMMMMMM")) +
                            16;
            m.text_padding_x = 8;
            m.list_item_height = text_height + 2;
            return m;
        }

        palette native_palette() const override {
            palette p;
            if (!be_app) {
                p.button_bg = native::rgba(216, 216, 216, 255);
                p.button_border = native::rgba(80, 80, 80, 255);
                p.button_highlight = native::rgba(255, 255, 255, 255);
                p.button_shadow = native::rgba(150, 150, 150, 255);
                p.button_text = native::rgba(0, 0, 0, 255);
                p.button_disabled_text =
                    native::rgba(130, 130, 130, 255);
                p.button_hot_bg = native::rgba(225, 225, 225, 255);
                p.button_hot_text = p.button_text;
                p.button_pressed_bg =
                    native::rgba(190, 190, 190, 255);
                p.button_pressed_text = p.button_text;
                p.menu_bar_bg = p.button_bg;
                p.menu_bar_line_top = p.button_highlight;
                p.menu_bar_line_bottom = p.button_shadow;
                p.menu_text = p.button_text;
                p.menu_disabled_text = p.button_disabled_text;
                p.menu_hot_bg = native::rgba(60, 120, 210, 255);
                p.menu_hot_text = native::rgba(255, 255, 255, 255);
                p.menu_popup_bg = p.button_bg;
                p.menu_popup_border = p.button_border;
                p.content_bg = native::rgba(255, 255, 255, 255);
                p.content_text = p.button_text;
                p.selection_bg = p.menu_hot_bg;
                p.selection_text = p.menu_hot_text;
                p.selection_inactive_bg = p.button_shadow;
                p.selection_inactive_text = p.button_text;
                p.separator = p.button_shadow;
                p.focus = p.menu_hot_bg;
                return p;
            }
            p.button_bg =
                from_native(ui_color(B_CONTROL_BACKGROUND_COLOR));
            p.button_border =
                from_native(ui_color(B_CONTROL_BORDER_COLOR));
            p.button_highlight = from_native(ui_color(B_SHINE_COLOR));
            p.button_shadow = from_native(ui_color(B_SHADOW_COLOR));
            p.button_text = from_native(ui_color(B_CONTROL_TEXT_COLOR));
            p.button_disabled_text = from_native(tint_color(
                ui_color(B_CONTROL_TEXT_COLOR), B_DISABLED_LABEL_TINT));
            p.button_hot_bg =
                from_native(ui_color(B_CONTROL_HIGHLIGHT_COLOR));
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = from_native(tint_color(
                ui_color(B_CONTROL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg =
                from_native(ui_color(B_MENU_BACKGROUND_COLOR));
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = from_native(ui_color(B_MENU_ITEM_TEXT_COLOR));
            p.menu_disabled_text =
                from_native(tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
                                       B_DISABLED_LABEL_TINT));
            p.menu_hot_bg =
                from_native(ui_color(B_MENU_SELECTED_BACKGROUND_COLOR));
            p.menu_hot_text =
                from_native(ui_color(B_MENU_SELECTED_ITEM_TEXT_COLOR));
            p.menu_popup_bg = p.menu_bar_bg;
            p.menu_popup_border =
                from_native(ui_color(B_MENU_SELECTED_BORDER_COLOR));
            p.content_bg = from_native(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            p.content_text = from_native(ui_color(B_DOCUMENT_TEXT_COLOR));
            p.selection_bg = from_native(
                ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            p.selection_text = from_native(
                ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            p.selection_inactive_bg = from_native(tint_color(
                ui_color(B_LIST_SELECTED_BACKGROUND_COLOR),
                B_LIGHTEN_1_TINT));
            p.selection_inactive_text = p.content_text;
            p.separator = p.button_shadow;
            p.focus = from_native(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
            return p;
        }

        theme &draw_button(const native::rect &r,
                           const std::string &text,
                           const state &s) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base =
                    ui_color(B_CONTROL_BACKGROUND_COLOR);
                const rgb_color background =
                    ui_color(B_PANEL_BACKGROUND_COLOR);
                const uint32 flags = flags_from(s);
                const BRect update(
                    r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawButtonFrame(
                    view, content, update, base, background, flags);
                be_control_look->DrawButtonBackground(
                    view, content, update, base, flags);
                const rgb_color text_color =
                    s.disabled
                        ? tint_color(ui_color(B_CONTROL_TEXT_COLOR),
                                     B_DISABLED_LABEL_TINT)
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
                const rgb_color base =
                    ui_color(B_MENU_BACKGROUND_COLOR);
                const BRect update(
                    r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawMenuBarBackground(
                    view, content, update, base);
            });
            return painted ? *this : draw_menu_bar_fallback(r);
        }

        theme &draw_menu_title(const native::rect &r,
                               const std::string &text,
                               const state &s) override {
            return draw_menu_entry(r, text, s, true);
        }

        theme &draw_menu_item(const native::rect &r,
                              const std::string &text,
                              const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_popup_frame(const native::rect &r) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base =
                    ui_color(B_MENU_BACKGROUND_COLOR);
                const BRect update(
                    r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                be_control_look->DrawMenuBackground(
                    view, content, update, base);
            });
            return painted ? *this : draw_popup_fallback(r);
        }

        theme &draw_list_item(const native::rect &r,
                              const std::string &text,
                              const state &s) override {
            const bool painted = with_view(_g, [&](BView *view) {
                paint_list_entry(view, r, text, s);
            });
            return painted ? *this
                           : draw_list_entry_fallback(r, text, s);
        }

        theme &draw_check(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            return draw_native_selection(r, text, s, false);
        }

        theme &draw_radio(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            return draw_native_selection(r, text, s, true);
        }

        theme &draw_list(const native::rect &r,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &s) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const BRect frame(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                view->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
                view->FillRect(frame);
                const native::rect content = r;
                const int item_height =
                    std::max(1, defaults().list_item_height);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const int y =
                        content.p.y + static_cast<int>(i) * item_height;
                    if (y >= content.y2())
                        break;
                    state item_state = s;
                    item_state.selected =
                        static_cast<int>(i) == selected_index;
                    paint_list_entry(
                        view,
                        native::rect(
                            content.p.x,
                            y,
                            content.d.w,
                            std::min(item_height, content.y2() - y)),
                        items[i],
                        item_state);
                }
            });
            return painted ? *this
                           : draw_list_fallback(
                                 r, items, selected_index, s);
        }

        theme &draw_text_edit_frame(
            const native::rect &r,
            const state &s) override {
            const bool painted = with_view(_g, [&](BView *view) {
                const BRect update(
                    r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                view->SetHighColor(
                    s.disabled
                        ? ui_color(B_CONTROL_BACKGROUND_COLOR)
                        : ui_color(B_DOCUMENT_BACKGROUND_COLOR));
                view->FillRect(content);
                be_control_look->DrawTextControlBorder(
                    view,
                    content,
                    update,
                    ui_color(B_CONTROL_BACKGROUND_COLOR),
                    flags_from(s));
            });
            if (painted)
                return *this;
            const palette p = native_palette();
            _g.set_pen(1)
                .set_ink(s.disabled ? p.button_bg : p.menu_popup_bg)
                .draw_rect(r, true)
                .set_ink(p.button_border)
                .draw_rect(r, false);
            return *this;
        }

    private:
        int baseline(const native::rect &r) const {
            if (!be_app) {
                const native::font_metrics metrics =
                    native::font_t::stock(native::font_role::control)
                        .get_metrics();
                return r.p.y +
                       std::max(0,
                                (static_cast<int>(r.d.h) -
                                 metrics.height) /
                                    2) +
                       metrics.ascent;
            }
            font_height height{};
            be_plain_font->GetHeight(&height);
            return r.p.y +
                   static_cast<int>(
                       (r.d.h - height.ascent - height.descent) / 2 +
                       height.ascent);
        }

        int text_top(const native::rect &r) const {
            const native::font_metrics metrics =
                native::font_t::stock(native::font_role::control)
                    .get_metrics();
            return r.p.y +
                   std::max(0,
                            (static_cast<int>(r.d.h) -
                             metrics.height) /
                                2);
        }

        native::rect indicator_bounds(const native::rect &r) const {
            const int side =
                std::max(7, std::min(16, static_cast<int>(r.d.h) - 2));
            return native::rect(
                r.p.x + 2,
                r.p.y +
                    std::max(0, (static_cast<int>(r.d.h) - side) / 2),
                side,
                side);
        }

        theme &draw_native_selection(const native::rect &r,
                                     const std::string &text,
                                     const state &s,
                                     bool radio) {
            const native::rect indicator = indicator_bounds(r);
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base =
                    ui_color(B_CONTROL_BACKGROUND_COLOR);
                const BRect update(indicator.p.x,
                                   indicator.p.y,
                                   indicator.x2() - 1,
                                   indicator.y2() - 1);
                BRect mark(update);
                if (radio) {
                    be_control_look->DrawRadioButton(
                        view, mark, update, base, flags_from(s));
                } else {
                    be_control_look->DrawCheckBox(
                        view, mark, update, base, flags_from(s));
                }

                const rgb_color text_color =
                    s.disabled
                        ? tint_color(ui_color(B_CONTROL_TEXT_COLOR),
                                     B_DISABLED_LABEL_TINT)
                        : ui_color(B_CONTROL_TEXT_COLOR);
                const BRect label(
                    indicator.x2() + 5, r.p.y, r.x2() - 1, r.y2() - 1);
                be_control_look->DrawLabel(
                    view,
                    text.c_str(),
                    label,
                    label,
                    base,
                    flags_from(s),
                    BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE),
                    &text_color);
            });
            return painted ? *this
                           : draw_selection_fallback(r, text, s, radio);
        }

        theme &draw_selection_fallback(const native::rect &r,
                                       const std::string &text,
                                       const state &s,
                                       bool radio) {
            saved_state saved(_g);
            const palette p = native_palette();
            const native::rect indicator = indicator_bounds(r);
            _g.set_pen(1)
                .set_ink(p.button_bg)
                .draw_rect(indicator, true);
            _g.set_ink(p.button_border).draw_rect(indicator, false);
            if (s.selected) {
                const native::rgba color =
                    s.disabled ? p.button_disabled_text : p.button_text;
                if (radio) {
                    const int inset = std::max(
                        2, static_cast<int>(indicator.d.w) / 4);
                    _g.set_ink(color).draw_rect(
                        native::rect(indicator.p.x + inset,
                                     indicator.p.y + inset,
                                     indicator.d.w - inset * 2,
                                     indicator.d.h - inset * 2),
                        true);
                } else {
                    _g.set_pen(2)
                        .set_ink(color)
                        .draw_line(native::point(indicator.p.x + 3,
                                                 indicator.p.y +
                                                     indicator.d.h / 2),
                                   native::point(indicator.p.x +
                                                     indicator.d.w / 2 -
                                                     1,
                                                 indicator.y2() - 4))
                        .draw_line(native::point(indicator.p.x +
                                                     indicator.d.w / 2 -
                                                     1,
                                                 indicator.y2() - 4),
                                   native::point(indicator.x2() - 3,
                                                 indicator.p.y + 3));
                }
            }
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text
                                  : p.button_text)
                .draw_text(
                    text,
                    native::point(indicator.x2() + 5, text_top(r)));
            return *this;
        }

        void paint_list_entry(BView *view,
                              const native::rect &r,
                              const std::string &text,
                              const state &s) const {
            const rgb_color background =
                ui_color(s.selected
                             ? B_LIST_SELECTED_BACKGROUND_COLOR
                             : B_LIST_BACKGROUND_COLOR);
            const rgb_color foreground =
                s.disabled
                    ? tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
                                 B_DISABLED_LABEL_TINT)
                    : ui_color(s.selected
                                   ? B_LIST_SELECTED_ITEM_TEXT_COLOR
                                   : B_LIST_ITEM_TEXT_COLOR);
            view->SetHighColor(background);
            view->FillRect(BRect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1));
            view->SetFont(be_plain_font);
            view->SetHighColor(foreground);
            view->SetLowColor(background);
            view->DrawString(
                text.c_str(),
                BPoint(r.p.x + defaults().text_padding_x, baseline(r)));
        }

        theme &draw_list_entry_fallback(const native::rect &r,
                                        const std::string &text,
                                        const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1)
                .set_ink(s.selected ? p.menu_hot_bg : p.menu_popup_bg)
                .draw_rect(r, true);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.menu_disabled_text
                                  : (s.selected ? p.menu_hot_text
                                                : p.menu_text))
                .draw_text(
                    text,
                    native::point(r.p.x + defaults().text_padding_x,
                                  text_top(r)));
            return *this;
        }

        theme &draw_list_fallback(const native::rect &r,
                                  const std::vector<std::string> &items,
                                  int selected_index,
                                  const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            if (r.d.w == 0 || r.d.h == 0)
                return *this;

            const native::rect content = r;
            _g.set_clip(_g.get_clip().intersect(content));
            const int item_height =
                std::max(1, defaults().list_item_height);
            for (std::size_t i = 0; i < items.size(); ++i) {
                const int y =
                    content.p.y + static_cast<int>(i) * item_height;
                if (y >= content.y2())
                    break;
                state item_state = s;
                item_state.selected =
                    static_cast<int>(i) == selected_index;
                draw_list_entry_fallback(
                    native::rect(
                        content.p.x,
                        y,
                        content.d.w,
                        std::min(item_height, content.y2() - y)),
                    items[i],
                    item_state);
            }
            return *this;
        }

        theme &draw_button_fallback(const native::rect &r,
                                    const std::string &text,
                                    const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            const native::rgba background =
                s.pressed ? p.button_pressed_bg
                          : (s.hot ? p.button_hot_bg : p.button_bg);
            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            _g.set_ink(p.button_border).draw_rect(r, false);
            const native::font_t &font =
                native::font_t::stock(native::font_role::control);
            const int width = font.measure_text(text).width;
            _g.set_font(font);
            _g.set_ink(s.disabled ? p.button_disabled_text
                                  : p.button_text)
                .draw_text(
                    text,
                    native::point(
                        r.p.x +
                            std::max(0,
                                     (static_cast<int>(r.d.w) - width) /
                                         2),
                        text_top(r)));
            return *this;
        }

        theme &draw_menu_bar_fallback(const native::rect &r) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_bar_bg).draw_rect(r, true);
            if (r.d.w && r.d.h) {
                _g.set_ink(p.menu_bar_line_bottom)
                    .draw_line(native::point(r.p.x, r.y2() - 1),
                               native::point(r.x2() - 1, r.y2() - 1));
            }
            return *this;
        }

        theme &draw_menu_entry(const native::rect &r,
                               const std::string &text,
                               const state &s,
                               bool title) {
            const bool active = s.hot || s.selected;
            const bool painted = with_view(_g, [&](BView *view) {
                const rgb_color base =
                    ui_color(B_MENU_BACKGROUND_COLOR);
                const uint32 flags = flags_from(s);
                const BRect update(
                    r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
                BRect content(update);
                if (!title || active) {
                    be_control_look->DrawMenuItemBackground(
                        view, content, update, base, flags);
                }
                const rgb_color text_color =
                    s.disabled
                        ? tint_color(ui_color(B_MENU_ITEM_TEXT_COLOR),
                                     B_DISABLED_LABEL_TINT)
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

        theme &draw_menu_entry_fallback(const native::rect &r,
                                        const std::string &text,
                                        const state &s,
                                        bool title) {
            saved_state saved(_g);
            const palette p = native_palette();
            const bool active = s.hot || s.selected;
            _g.set_pen(1)
                .set_ink(
                    active ? p.menu_hot_bg
                           : (title ? p.menu_bar_bg : p.menu_popup_bg))
                .draw_rect(r, true);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled
                           ? p.menu_disabled_text
                           : (active ? p.menu_hot_text : p.menu_text))
                .draw_text(
                    text,
                    native::point(r.p.x + defaults().text_padding_x,
                                  text_top(r)));
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
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<haiku_theme>(painter);
    }
} // namespace native
