//
// Implements the X11/Athena theme from the active Xaw resources.
// Xaw has no painter for arbitrary drawables, so this backend mirrors
// the native widgets' resources and documented flat/reverse-video
// model.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/List.h>

#include <native.h>
#include <native/theme.h>

#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    struct xaw_resources
    {
        native::rgba background = native::rgba(255, 255, 255, 255);
        native::rgba foreground = native::rgba(0, 0, 0, 255);
        native::rgba border = native::rgba(0, 0, 0, 255);
    };

    Widget query_parent() {
        native::app_wnd *main_window = native::app::main_wnd();
        return main_window
            ? linux::x11::wnd_bindings.handle_from_object(main_window)
            : nullptr;
    }

    Widget query_command() {
        Widget parent = query_parent();
        if (!parent)
            return nullptr;

        WidgetList children = nullptr;
        Cardinal child_count = 0;
        XtVaGetValues(parent,
                      XtNchildren,
                      &children,
                      XtNnumChildren,
                      &child_count,
                      nullptr);
        for (Cardinal index = 0; index < child_count; ++index) {
            if (XtIsSubclass(children[index], commandWidgetClass))
                return children[index];
        }
        return nullptr;
    }

    native::rgba color_from_pixel(Widget widget, Pixel pixel) {
        XColor color = {};
        color.pixel = pixel;
        XQueryColor(XtDisplay(widget),
                    DefaultColormapOfScreen(XtScreen(widget)),
                    &color);
        return native::rgba(static_cast<std::uint8_t>(color.red >> 8),
                            static_cast<std::uint8_t>(color.green >> 8),
                            static_cast<std::uint8_t>(color.blue >> 8),
                            255);
    }

    xaw_resources query_resources() {
        xaw_resources result;
        Widget parent = query_parent();
        if (!parent)
            return result;

        Widget probe = query_command();
        const bool owns_probe = !probe;
        if (!probe) {
            probe = XtVaCreateWidget("theme_probe",
                                     commandWidgetClass,
                                     parent,
                                     nullptr);
        }
        if (!probe)
            return result;

        Pixel background = 0;
        Pixel foreground = 0;
        Pixel border = 0;
        XtVaGetValues(probe,
                      XtNbackground,
                      &background,
                      XtNforeground,
                      &foreground,
                      XtNborderColor,
                      &border,
                      nullptr);
        result.background = color_from_pixel(probe, background);
        result.foreground = color_from_pixel(probe, foreground);
        result.border = color_from_pixel(probe, border);
        if (owns_probe)
            XtDestroyWidget(probe);
        return result;
    }

    XFontStruct *query_control_font() {
        const auto &font =
            native::font_t::stock(native::font_role::control);
        auto *binding =
            linux::x11::font_bindings.object_from_handle(font.id());
        if (!binding || !binding->xfont || !linux::x11::cached_display)
            return nullptr;
        return XQueryFont(linux::x11::cached_display, binding->xfont);
    }

    class xaw_theme final : public linux::emulated_theme
    {
    public:
        explicit xaw_theme(native::gpx &graphics)
            : emulated_theme(graphics) {}

        metrics defaults() const override {
            const int row_height = text_height() + 2;
            metrics result;
            result.menu_bar_height = row_height + 4;
            result.menu_item_height = row_height + 2;
            result.popup_width = 180;
            result.text_padding_x = 4;
            result.check_height = row_height + 4;
            result.radio_height = row_height + 4;
            result.list_item_height = row_height;
            result.header_height = row_height + 4;
            return result;
        }

        palette native_palette() const override {
            const xaw_resources resources = query_resources();
            palette result;
            result.button_bg = resources.background;
            result.button_border = resources.border;
            result.button_highlight = resources.background;
            result.button_shadow = resources.border;
            result.button_text = resources.foreground;
            result.button_disabled_text = native::rgba(128, 128, 128,
                                                        255);
            result.button_hot_bg = resources.background;
            result.button_hot_text = resources.foreground;
            result.button_pressed_bg = resources.foreground;
            result.button_pressed_text = resources.background;
            result.menu_bar_bg = resources.background;
            result.menu_bar_line_top = resources.background;
            result.menu_bar_line_bottom = resources.background;
            result.menu_text = resources.foreground;
            result.menu_disabled_text = result.button_disabled_text;
            result.menu_hot_bg = resources.foreground;
            result.menu_hot_text = resources.background;
            result.menu_popup_bg = resources.background;
            result.menu_popup_border = resources.border;
            result.content_bg = resources.background;
            result.content_text = resources.foreground;
            result.selection_bg = resources.foreground;
            result.selection_text = resources.background;
            result.selection_inactive_bg = resources.border;
            result.selection_inactive_text = resources.background;
            result.separator = resources.border;
            result.focus = resources.foreground;
            return result;
        }

        native::theme &draw_button(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_command(bounds, text, element_state, true);
        }

        native::theme &draw_menu_bar(
            const native::rect &bounds) override {
            saved_state saved(_g);
            _g.set_pen(1)
                .set_ink(native_palette().menu_bar_bg)
                .draw_rect(bounds, true);
            return *this;
        }

        native::theme &draw_menu_title(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            state command_state = element_state;
            command_state.pressed = element_state.selected ||
                                    element_state.hot;
            return draw_command(bounds, text, command_state, true);
        }

        native::theme &draw_menu_item(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_entry(bounds, text, element_state);
        }

        native::theme &draw_popup_frame(
            const native::rect &bounds) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            _g.set_pen(1)
                .set_ink(colors.menu_popup_bg)
                .draw_rect(bounds, true)
                .set_ink(colors.menu_popup_border)
                .draw_rect(bounds, false);
            return *this;
        }

        native::theme &draw_list_item(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_entry(bounds, text, element_state);
        }

        native::theme &draw_list(
            const native::rect &bounds,
            const std::vector<std::string> &items,
            int selected_index,
            const state &element_state) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            Dimension horizontal_margin = 2;
            Dimension vertical_margin = 2;
            Widget parent = query_parent();
            Widget probe = parent
                               ? XtVaCreateWidget(
                                     "theme_list_probe",
                                     listWidgetClass,
                                     parent,
                                     nullptr)
                               : nullptr;
            if (probe) {
                XtVaGetValues(probe,
                              XtNinternalWidth,
                              &horizontal_margin,
                              XtNinternalHeight,
                              &vertical_margin,
                              nullptr);
                XtDestroyWidget(probe);
            }

            _g.set_pen(1)
                .set_ink(colors.menu_popup_bg)
                .draw_rect(bounds, true)
                .set_ink(colors.menu_popup_border)
                .draw_rect(bounds, false);
            const int inset_x = 1 + horizontal_margin;
            const int inset_y = 1 + vertical_margin;
            if (static_cast<int>(bounds.d.w) <= inset_x * 2 ||
                static_cast<int>(bounds.d.h) <= inset_y * 2) {
                return *this;
            }
            const native::rect content(
                bounds.p.x + inset_x,
                bounds.p.y + inset_y,
                bounds.d.w - inset_x * 2,
                bounds.d.h - inset_y * 2);
            _g.set_clip(_g.get_clip().intersect(content));
            const int item_height =
                std::max(1, defaults().list_item_height);
            for (std::size_t index = 0; index < items.size(); ++index) {
                const int y = content.p.y +
                              static_cast<int>(index) * item_height;
                if (y >= content.y2())
                    break;
                state item_state = element_state;
                item_state.selected =
                    static_cast<int>(index) == selected_index;
                draw_list_item(
                    native::rect(
                        content.p.x,
                        y,
                        content.d.w,
                        std::min(item_height, content.y2() - y)),
                    items[index],
                    item_state);
            }
            return *this;
        }

        native::theme &draw_check(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_toggle(bounds, text, element_state);
        }

        native::theme &draw_radio(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_toggle(bounds, text, element_state);
        }

        native::theme &draw_text_edit_frame(
            const native::rect &bounds,
            const state &) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            _g.set_pen(1)
                .set_ink(colors.menu_popup_bg)
                .draw_rect(bounds, true)
                .set_ink(colors.menu_popup_border)
                .draw_rect(bounds, false);
            return *this;
        }

    protected:
        int text_width(const std::string &text) const override {
            XFontStruct *font = query_control_font();
            if (!font)
                return static_cast<int>(text.size()) * 7;
            const int width = XTextWidth(
                font, text.c_str(), static_cast<int>(text.size()));
            XFreeFontInfo(nullptr, font, 1);
            return width;
        }

        int text_height() const override {
            XFontStruct *font = query_control_font();
            if (!font)
                return 12;
            const int height =
                std::max(1, font->ascent + font->descent);
            XFreeFontInfo(nullptr, font, 1);
            return height;
        }

    private:
        native::theme &draw_command(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state,
            bool centered) {
            saved_state saved(_g);
            const palette colors = native_palette();
            const bool reversed = element_state.pressed ||
                                  element_state.selected;
            const native::rgba background =
                reversed ? colors.button_pressed_bg
                         : colors.button_bg;
            const native::rgba foreground =
                element_state.disabled
                    ? colors.button_disabled_text
                    : (reversed ? colors.button_pressed_text
                                : colors.button_text);

            _g.set_pen(1)
                .set_ink(background)
                .draw_rect(bounds, true)
                .set_ink(colors.button_border)
                .draw_rect(bounds, false);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_clip(_g.get_clip().intersect(bounds));
            const int x = centered
                ? bounds.p.x +
                      std::max(0,
                               (static_cast<int>(bounds.d.w) -
                                text_width(text)) /
                                   2)
                : bounds.p.x + defaults().text_padding_x;
            _g.set_ink(foreground)
                .draw_text(text, native::point(x, text_y(bounds)));
            return *this;
        }

        native::theme &draw_toggle(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) {
            state command_state = element_state;
            command_state.pressed = element_state.pressed ||
                                    element_state.selected;
            return draw_command(bounds, text, command_state, true);
        }

        native::theme &draw_entry(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) {
            saved_state saved(_g);
            const palette colors = native_palette();
            const bool selected = element_state.selected ||
                                  element_state.hot;
            const native::rgba background =
                selected ? colors.menu_hot_bg
                         : colors.menu_popup_bg;
            const native::rgba foreground =
                element_state.disabled
                    ? colors.menu_disabled_text
                    : (selected ? colors.menu_hot_text
                                : colors.menu_text);
            _g.set_pen(1).set_ink(background).draw_rect(bounds, true);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_clip(_g.get_clip().intersect(bounds));
            _g.set_ink(foreground)
                .draw_text(
                    text,
                    native::point(bounds.p.x +
                                      defaults().text_padding_x,
                                  text_y(bounds)));
            return *this;
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<xaw_theme>(painter);
    }
} // namespace native
