//
// Implements Window Maker custom primitives with the same WINGs relief,
// colors, indicator pixmaps, and fonts used by native WINGs controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>
#include <optional>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/icon_view.h>
#include <native/code_edit.h>
#include <native/table_view.h>
#include <native/theme.h>
#include <native/tree_view.h>

#include "../../gpx_wnd.h"
#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    struct theme_target
    {
        W_Screen *screen = nullptr;
        linux::wmaker::window_graphics *graphics = nullptr;
        native::wnd *owner = nullptr;
    };

    theme_target target_from(native::gpx &graphics) {
        theme_target result;
        result.screen = reinterpret_cast<W_Screen *>(
            linux::wmaker::screen);
        auto *window_graphics =
            dynamic_cast<native::gpx_wnd *>(&graphics);
        if (!window_graphics || !result.screen)
            return result;
        auto *target = linux::wmaker::graphics_bindings
                           .object_from_handle(
                               window_graphics->window());
        result.graphics = target;
        result.owner = window_graphics->window();
        return result;
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
            // Match the GTK Task Manager's full-row selection and vertical
            // breathing room while keeping ordinary WINGs lists compact.
            result.table_row_height = height + 8;
            result.table_outer_border_extent = 1;
            result.disclosure_size = 9;
            result.tree_lines_visible = false;
            result.header_height = height + 10;
            result.tab_height = result.header_height;
            result.scrollbar_extent = SCROLLER_WIDTH;
            result.scrollbar_min_thumb = SCROLLER_WIDTH;
            result.table_fill_last_column = true;
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
            const bool table =
                dynamic_cast<native::table_view *>(target.owner) != nullptr;
            const bool editor =
                dynamic_cast<native::code_edit *>(target.owner) != nullptr;
            const bool row_selection = table || editor;
            result.content_bg = table
                                    ? native::rgba(215, 215, 215, 255)
                                    : (editor ? white : gray);
            result.content_alt_bg = table
                                        ? native::rgba(200, 200, 200, 255)
                                        : native::rgba(0, 0, 0, 0);
            result.content_text = black;
            result.selection_bg = row_selection
                                      ? native::rgba(85, 85, 85, 255)
                                      : dark;
            result.selection_text = row_selection
                                        ? native::rgba(215, 215, 215, 255)
                                        : white;
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
            if (bounds.d.w > 0 && bounds.d.h > 0) {
                _g.set_pen(1)
                    .set_ink(native_palette().menu_bar_line_bottom)
                    .draw_line(
                        native::point(bounds.p.x, bounds.y2() - 1),
                        native::point(bounds.x2() - 1,
                                      bounds.y2() - 1));
            }
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
            if (bounds.d.w <= 0 || bounds.d.h <= 0)
                return *this;
            const palette colors = native_palette();
            const native::coord right = bounds.x2() - 1;
            const native::coord bottom = bounds.y2() - 1;
            _g.set_pen(1)
                .set_ink(colors.menu_popup_border)
                .draw_rect(bounds, false);
            if (bounds.d.w > 3 && bounds.d.h > 3) {
                const native::coord inner_left = bounds.p.x + 1;
                const native::coord inner_top = bounds.p.y + 1;
                const native::coord inner_right = right - 1;
                const native::coord inner_bottom = bottom - 1;
                _g.set_ink(colors.button_highlight)
                    .draw_line(native::point(inner_left, inner_top),
                               native::point(inner_right, inner_top))
                    .draw_line(native::point(inner_left, inner_top),
                               native::point(inner_left, inner_bottom))
                    .set_ink(colors.button_shadow)
                    .draw_line(native::point(inner_left, inner_bottom),
                               native::point(inner_right, inner_bottom))
                    .draw_line(native::point(inner_right, inner_top),
                               native::point(inner_right, inner_bottom));
            }
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
                     ? (linux::wmaker::list_selection_background
                            ? linux::wmaker::list_selection_background
                            : target.screen->darkGray)
                     : target.screen->gray);
            const palette colors = native_palette();
            _g.set_font(native::font_t::stock(
                            native::font_role::control))
                .set_ink(element_state.selected
                             ? native::rgba(215, 215, 215, 255)
                             : (element_state.disabled
                                    ? colors.button_disabled_text
                                    : colors.button_text))
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

        theme &draw_surface(
            const native::rect &bounds,
            native::surface_kind kind,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            const bool native_header =
                kind == native::surface_kind::ruler ||
                kind == native::surface_kind::header ||
                kind == native::surface_kind::table_header;
            if (!target.graphics || !native_header) {
                return emulated_theme::draw_surface(
                    bounds, kind, element_state);
            }
            saved_state saved(_g);
            _g.set_ink(native::rgba(128, 128, 128, 255))
                .draw_rect(bounds, true);
            if (bounds.d.w <= 0 || bounds.d.h <= 0)
                return *this;
            const palette colors = native_palette();
            const native::coord right = bounds.x2() - 1;
            const native::coord bottom = bounds.y2() - 1;
            // Task Manager column headers are not full raised WINGs
            // buttons: their only light edge is the left divider. Use the
            // identical recipe for collection headers so compact Window
            // Maker headers belong to the same visual family.
            _g.set_pen(1)
                .set_ink(colors.button_border)
                .draw_line(bounds.p,
                           native::point(right, bounds.p.y))
                .set_ink(colors.button_highlight)
                .draw_line(bounds.p,
                           native::point(bounds.p.x, bottom))
                .set_ink(colors.button_shadow)
                .draw_line(native::point(bounds.p.x, bottom),
                           native::point(right, bottom))
                .draw_line(native::point(right, bounds.p.y),
                           native::point(right, bottom));
            return *this;
        }

        theme &draw_scrollbar_part(
            const native::rect &bounds,
            native::scrollbar_orientation orientation,
            native::scrollbar_part part,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            if (dynamic_cast<native::icon_view *>(target.owner) ||
                dynamic_cast<native::tree_view *>(target.owner) ||
                dynamic_cast<native::table_view *>(target.owner)) {
                // These hosts place real WINGs scrollers over the semantic
                // scrollbar reservation. Do not paint a duplicate beneath
                // the native child widget.
                return *this;
            }
            return emulated_theme::draw_scrollbar_part(
                bounds, orientation, part, element_state);
        }

        theme &draw_disclosure(
            const native::rect &bounds,
            native::disclosure_state disclosure,
            const state &element_state) override {
            // Window Maker's stock W_Pixmap arrows carry an opaque square
            // resource paper in their mask on common WINGs builds. Removing
            // that paper also removes the one-color disclosure glyph. The
            // native applications use the same compact filled down/right
            // triangles, so draw that shape through the semantic fallback
            // with the live Window Maker palette.
            return draw_disclosure_fallback(
                bounds, disclosure, element_state);
        }

        theme &draw_sort_indicator(
            const native::rect &bounds,
            native::sort_indicator_state direction,
            const state &element_state) override {
            const theme_target target = target_from(_g);
            W_Pixmap *image = target.screen
                ? (direction == native::sort_indicator_state::ascending
                       ? target.screen->upArrow
                       : target.screen->downArrow)
                : nullptr;
            return draw_native_pixmap(
                bounds, image, std::nullopt, direction, element_state);
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
        theme &draw_native_pixmap(
            const native::rect &bounds,
            W_Pixmap *image,
            std::optional<native::disclosure_state> disclosure,
            std::optional<native::sort_indicator_state> sort,
            const state &element_state) {
            const theme_target target = target_from(_g);
            if (!target.screen || !target.graphics ||
                target.graphics->backbuffer == None || !image) {
                return disclosure
                           ? draw_disclosure_fallback(
                                 bounds, *disclosure, element_state)
                           : draw_sort_indicator_fallback(
                                 bounds, *sort, element_state);
            }
            const int x = bounds.p.x + std::max(
                0,
                (static_cast<int>(bounds.d.w) - image->width) / 2);
            const int y = bounds.p.y + std::max(
                0,
                (static_cast<int>(bounds.d.h) - image->height) / 2);
            Display *display = target.screen->display;
            XImage *pixels = XGetImage(display,
                                       image->pixmap,
                                       0,
                                       0,
                                       image->width,
                                       image->height,
                                       AllPlanes,
                                       ZPixmap);
            XImage *mask = image->mask != None
                               ? XGetImage(display,
                                           image->mask,
                                           0,
                                           0,
                                           image->width,
                                           image->height,
                                           1,
                                           ZPixmap)
                               : nullptr;
            if (pixels) {
                // WINGs' arrow masks include their square resource paper.
                // Treat the corner pixel as that paper and composite only
                // differing native glyph pixels onto the already-painted
                // semantic surface.
                const unsigned long paper = XGetPixel(pixels, 0, 0);
                GC gc = target.screen->clipGC;
                for (unsigned int source_y = 0;
                     source_y < image->height;
                     ++source_y) {
                    for (unsigned int source_x = 0;
                         source_x < image->width;
                         ++source_x) {
                        if (mask &&
                            XGetPixel(mask, source_x, source_y) == 0)
                            continue;
                        const unsigned long pixel = XGetPixel(
                            pixels, source_x, source_y);
                        if (pixel == paper)
                            continue;
                        XSetForeground(display, gc, pixel);
                        XDrawPoint(display,
                                   target.graphics->backbuffer,
                                   gc,
                                   x + static_cast<int>(source_x),
                                   y + static_cast<int>(source_y));
                    }
                }
            } else {
                // Retain a visible native glyph on servers that cannot read
                // the small stock pixmap, even though color-key compositing
                // is unavailable there.
                GC gc = target.screen->clipGC;
                if (image->mask != None) {
                    XSetClipOrigin(display, gc, x, y);
                    XSetClipMask(display, gc, image->mask);
                }
                if (image->depth == 1) {
                    XCopyPlane(display,
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
                    XCopyArea(display,
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
                XSetClipMask(display, gc, None);
            }
            if (mask)
                XDestroyImage(mask);
            if (pixels)
                XDestroyImage(pixels);
            return *this;
        }

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
            fill(target, bounds, target.screen->gray);
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
