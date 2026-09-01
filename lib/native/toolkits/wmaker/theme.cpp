//
// Implements Window Maker custom primitives with the same WINGs relief,
// colors, indicator pixmaps, and fonts used by native WINGs controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <X11/Xlib.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/theme.h>

#include "../../gpx_wnd.h"
#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    struct theme_target
    {
        W_Screen *screen = nullptr;
        linux::wmaker::window_graphics *graphics = nullptr;
    };

    theme_target target_from(native::gpx &graphics) {
        auto *window_graphics =
            dynamic_cast<native::gpx_wnd *>(&graphics);
        if (!window_graphics || !linux::wmaker::screen)
            return {};
        auto *target = linux::wmaker::graphics_bindings
                           .object_from_handle(
                               window_graphics->window());
        return {reinterpret_cast<W_Screen *>(linux::wmaker::screen),
                target};
    }

    native::rgba color_of(WMColor *color) {
        if (!color)
            return native::rgba(0, 0, 0, 255);
        return native::rgba(
            static_cast<std::uint8_t>(
                WMRedComponentOfColor(color) >> 8),
            static_cast<std::uint8_t>(
                WMGreenComponentOfColor(color) >> 8),
            static_cast<std::uint8_t>(
                WMBlueComponentOfColor(color) >> 8),
            255);
    }

    void fill(theme_target target,
              const native::rect &bounds,
              WMColor *color) {
        if (!target.screen || !target.graphics ||
            target.graphics->backbuffer == None || !color) {
            return;
        }
        XFillRectangle(target.screen->display,
                       target.graphics->backbuffer,
                       WMColorGC(color),
                       bounds.p.x,
                       bounds.p.y,
                       bounds.d.w,
                       bounds.d.h);
    }

    void relief(theme_target target,
                const native::rect &bounds,
                WMReliefType value) {
        if (!target.screen || !target.graphics ||
            target.graphics->backbuffer == None) {
            return;
        }
        W_DrawRelief(target.screen,
                     target.graphics->backbuffer,
                     bounds.p.x,
                     bounds.p.y,
                     bounds.d.w,
                     bounds.d.h,
                     value);
    }

    class wmaker_theme final : public linux::emulated_theme
    {
    public:
        explicit wmaker_theme(native::gpx &graphics)
            : emulated_theme(graphics) {}

        metrics defaults() const override {
            metrics result;
            const theme_target target = target_from(
                const_cast<native::gpx &>(_g));
            const int height = target.screen
                                   ? WMFontHeight(
                                         target.screen->normalFont)
                                   : 12;
            result.menu_bar_height = 24;
            result.menu_item_height = 24;
            result.popup_width = 180;
            result.text_padding_x = 6;
            result.check_height = 20;
            result.radio_height = 20;
            result.list_item_height = height + 1;
            result.header_height = height + 10;
            return result;
        }

        palette native_palette() const override {
            const theme_target target = target_from(
                const_cast<native::gpx &>(_g));
            palette result;
            const native::rgba black = target.screen
                                           ? color_of(
                                                 target.screen->black)
                                           : native::rgba(0, 0, 0, 255);
            const native::rgba white = target.screen
                                           ? color_of(
                                                 target.screen->white)
                                           : native::rgba(
                                                 255, 255, 255, 255);
            const native::rgba gray = target.screen
                                          ? color_of(
                                                target.screen->gray)
                                          : native::rgba(
                                                174, 174, 174, 255);
            const native::rgba dark = target.screen
                                          ? color_of(
                                                target.screen->darkGray)
                                          : native::rgba(
                                                85, 85, 85, 255);
            result.button_bg = gray;
            result.button_border = black;
            result.button_highlight = white;
            result.button_shadow = dark;
            result.button_text = black;
            result.button_disabled_text = dark;
            result.button_hot_bg = white;
            result.button_hot_text = black;
            result.button_pressed_bg = white;
            result.button_pressed_text = black;
            result.menu_bar_bg = gray;
            result.menu_bar_line_top = white;
            result.menu_bar_line_bottom = dark;
            result.menu_text = black;
            result.menu_disabled_text = dark;
            result.menu_hot_bg = white;
            result.menu_hot_text = black;
            result.menu_popup_bg = gray;
            result.menu_popup_border = black;
            result.content_bg = white;
            result.content_text = black;
            result.selection_bg = black;
            result.selection_text = white;
            result.selection_inactive_bg = dark;
            result.selection_inactive_text = white;
            result.separator = dark;
            result.focus = black;
            return result;
        }

        theme &draw_button(const native::rect &bounds,
                           const std::string &text,
                           const state &element_state) override {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return emulated_theme::draw_button(
                    bounds, text, element_state);
            }
            saved_state saved(_g);
            fill(target,
                 bounds,
                 element_state.pressed
                     ? target.screen->white
                     : target.screen->gray);
            relief(target,
                   bounds,
                   element_state.pressed ? WRPushed : WRRaised);
            const palette colors = native_palette();
            _g.set_font(native::font_t::stock(
                            native::font_role::control))
                .set_ink(element_state.disabled
                             ? colors.button_disabled_text
                             : colors.button_text)
                .set_clip(_g.get_clip().intersect(bounds));
            const int offset = element_state.pressed ? 1 : 0;
            _g.draw_text(
                text,
                native::point(
                    bounds.p.x +
                        std::max(0,
                                 (static_cast<int>(bounds.d.w) -
                                  text_width(text)) /
                                     2) +
                        offset,
                    text_y(bounds) + offset));
            return *this;
        }

        theme &draw_menu_bar(
            const native::rect &bounds) override {
            const theme_target target = target_from(_g);
            if (!target.graphics)
                return emulated_theme::draw_menu_bar(bounds);
            saved_state saved(_g);
            fill(target, bounds, target.screen->gray);
            return *this;
        }

        theme &draw_menu_title(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            return draw_button(bounds, text, element_state);
        }

        theme &draw_menu_item(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return emulated_theme::draw_menu_item(
                    bounds, text, element_state);
            }
            saved_state saved(_g);
            fill(target,
                 bounds,
                 element_state.hot || element_state.selected
                     ? target.screen->white
                     : target.screen->gray);
            relief(target, bounds, WRRaised);
            const palette colors = native_palette();
            _g.set_font(native::font_t::stock(
                            native::font_role::control))
                .set_ink(element_state.disabled
                             ? colors.menu_disabled_text
                             : colors.menu_text)
                .set_clip(_g.get_clip().intersect(bounds))
                .draw_text(text,
                           native::point(bounds.p.x + 6,
                                         text_y(bounds)));
            return *this;
        }

        theme &draw_popup_frame(
            const native::rect &bounds) override {
            const theme_target target = target_from(_g);
            if (!target.graphics)
                return emulated_theme::draw_popup_frame(bounds);
            saved_state saved(_g);
            fill(target, bounds, target.screen->gray);
            relief(target, bounds, WRRaised);
            return *this;
        }

        theme &draw_list_item(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return emulated_theme::draw_list_item(
                    bounds, text, element_state);
            }
            saved_state saved(_g);
            fill(target,
                 bounds,
                 element_state.selected
                     ? target.screen->white
                     : target.screen->gray);
            const palette colors = native_palette();
            _g.set_font(native::font_t::stock(
                            native::font_role::control))
                .set_ink(element_state.disabled
                             ? colors.button_disabled_text
                             : colors.button_text)
                .set_clip(_g.get_clip().intersect(bounds))
                .draw_text(text,
                           native::point(bounds.p.x + 4,
                                         text_y(bounds)));
            return *this;
        }

        theme &draw_check(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override {
            return draw_indicator(
                bounds, text, element_state, false);
        }

        theme &draw_radio(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override {
            return draw_indicator(
                bounds, text, element_state, true);
        }

        theme &draw_list(
            const native::rect &bounds,
            const std::vector<std::string> &items,
            int selected_index,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return emulated_theme::draw_list(
                    bounds, items, selected_index, element_state);
            }
            saved_state saved(_g);
            fill(target, bounds, target.screen->gray);
            relief(target, bounds, WRSunken);
            if (bounds.d.w <= 4 || bounds.d.h <= 4)
                return *this;
            const int item_height = defaults().list_item_height;
            const native::rect content(
                bounds.p.x + 2,
                bounds.p.y + 2,
                bounds.d.w - 4,
                bounds.d.h - 4);
            _g.set_clip(_g.get_clip().intersect(content));
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
                        static_cast<native::dim>(std::min(
                            item_height, content.y2() - y))),
                    items[index],
                    item_state);
            }
            return *this;
        }

        theme &draw_text_edit_frame(
            const native::rect &bounds,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return emulated_theme::draw_text_edit_frame(
                    bounds, element_state);
            }
            saved_state saved(_g);
            fill(target,
                 bounds,
                 element_state.disabled
                     ? target.screen->gray
                     : target.screen->white);
            relief(target, bounds, WRSunken);
            return *this;
        }

    protected:
        int text_width(const std::string &text) const override {
            return native::font_t::stock(
                       native::font_role::control)
                .measure_text(text)
                .width;
        }

        int text_height() const override {
            return native::font_t::stock(
                       native::font_role::control)
                .get_metrics()
                .height;
        }

    private:
        theme &draw_indicator(
            const native::rect &bounds,
            const std::string &text,
            const state &element_state,
            bool radio) {
            const theme_target target = target_from(_g);
            if (!target.graphics) {
                return radio
                           ? emulated_theme::draw_radio(
                                 bounds, text, element_state)
                           : emulated_theme::draw_check(
                                 bounds, text, element_state);
            }
            saved_state saved(_g);
            W_Pixmap *image = radio
                                  ? (element_state.selected
                                         ? target.screen
                                               ->radioButtonImageOn
                                         : target.screen
                                               ->radioButtonImageOff)
                                  : (element_state.selected
                                         ? target.screen
                                               ->checkButtonImageOn
                                         : target.screen
                                               ->checkButtonImageOff);
            if (!image) {
                return radio
                           ? emulated_theme::draw_radio(
                                 bounds, text, element_state)
                           : emulated_theme::draw_check(
                                 bounds, text, element_state);
            }
            fill(target, bounds, target.screen->white);
            const int x = bounds.p.x;
            const int y = bounds.p.y + std::max(
                0,
                (static_cast<int>(bounds.d.h) - image->height) / 2);
            GC gc = target.screen->clipGC;
            if (image->mask != None) {
                XSetClipOrigin(target.screen->display, gc, x, y);
                XSetClipMask(target.screen->display,
                             gc,
                             image->mask);
            }
            if (image->depth == 1) {
                XCopyPlane(target.screen->display,
                           image->pixmap,
                           target.graphics->backbuffer,
                           gc,
                           0,
                           0,
                           image->width,
                           image->height,
                           x,
                           y,
                           1);
            } else {
                XCopyArea(target.screen->display,
                          image->pixmap,
                          target.graphics->backbuffer,
                          gc,
                          0,
                          0,
                          image->width,
                          image->height,
                          x,
                          y);
            }
            XSetClipMask(target.screen->display, gc, None);
            const palette colors = native_palette();
            _g.set_font(native::font_t::stock(
                            native::font_role::control))
                .set_ink(element_state.disabled
                             ? colors.button_disabled_text
                             : colors.button_text)
                .set_clip(_g.get_clip().intersect(bounds))
                .draw_text(text,
                           native::point(
                               x + image->width + 9,
                               text_y(bounds)));
            return *this;
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<wmaker_theme>(painter);
    }
} // namespace native
