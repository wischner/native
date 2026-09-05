//
// Implements the OPEN LOOK theme using the same OLGX primitives,
// control color map, and XView font resources as native Panel items.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>
#include <optional>

#include <native.h>
#include <native/theme.h>

#include <X11/Xlib.h>
#include <xview/cms.h>
#include <xview/font.h>
#include <xview/panel.h>
#include <xview/scrollbar.h>
#include <xview/window.h>
#include <xview/xview.h>

#include "../../gpx_wnd.h"
#include "../emulated_theme.h"
#include "globals.h"

struct graphics_info;

extern "C"
{
    graphics_info *xv_init_olgx(Xv_Window window,
                                int *three_d,
                                Xv_Font text_font);
    void olgx_draw_button(graphics_info *information,
                          Window drawable,
                          int x,
                          int y,
                          int width,
                          int height,
                          void *label,
                          int state);
    void olgx_draw_box(graphics_info *information,
                       Window drawable,
                       int x,
                       int y,
                       int width,
                       int height,
                       int state,
                       int fill);
    void olgx_draw_check_box(graphics_info *information,
                             Window drawable,
                             int x,
                             int y,
                             int state);
    void olgx_draw_menu_mark(graphics_info *information,
                             Window drawable,
                             int x,
                             int y,
                             int state,
                             int fill);
    void olgx_draw_pushpin(graphics_info *information,
                           Window drawable,
                           int x,
                           int y,
                           int state);
}

namespace
{
    constexpr int olgx_normal = 0x0000;
    constexpr int olgx_invoked = 0x0001;
    constexpr int olgx_menu_item = 0x0002;
    constexpr int olgx_erase = 0x0004;
    constexpr int olgx_inactive = 0x0020;
    constexpr int olgx_vertical_menu_mark = 0x0040;
    constexpr int olgx_horizontal_menu_mark = 0x0080;
    constexpr int olgx_vertical_back_menu_mark = 0x2000;
    constexpr int olgx_checked = 0x0002;
    constexpr int olgx_pushpin_out = 0x2000;
    constexpr int olgx_pushpin_in = 0x4000;

    struct openlook_target
    {
        Panel panel = XV_NULL;
        linux::openlook::openlook_gpx *cache = nullptr;
        graphics_info *information = nullptr;
    };

    bool contains(const native::rect &outer,
                  const native::rect &inner) {
        return outer.p.x <= inner.p.x &&
               outer.p.y <= inner.p.y &&
               outer.x2() >= inner.x2() &&
               outer.y2() >= inner.y2();
    }

    openlook_target target_from(native::gpx &graphics) {
        auto *window_graphics =
            dynamic_cast<native::gpx_wnd *>(&graphics);
        if (!window_graphics)
            return {};
        native::wnd *window = window_graphics->window();
        auto *cache = linux::openlook::wnd_gpx_bindings
                          .object_from_handle(window);
        if (!cache || !cache->backbuffer)
            return {};

        Panel panel = XV_NULL;
        if (auto *top = dynamic_cast<native::app_wnd *>(window)) {
            auto *state = linux::openlook::window_state(top);
            panel = state ? state->content : XV_NULL;
        } else if (auto *accordion =
                       dynamic_cast<native::accordion *>(window)) {
            auto *state = linux::openlook::accordion_bindings
                              .object_from_handle(accordion);
            panel = state ? state->panel : XV_NULL;
        } else if (auto *icons =
                       dynamic_cast<native::icon_view *>(window)) {
            auto *state = linux::openlook::icon_view_bindings
                              .object_from_handle(icons);
            panel = state ? state->panel : XV_NULL;
        } else {
            Xv_opaque item = linux::openlook::wnd_bindings
                                 .handle_from_object(window);
            panel = item
                        ? static_cast<Panel>(xv_get(item, XV_OWNER))
                        : XV_NULL;
        }
        if (!panel)
            return {};

        int three_d = TRUE;
        Xv_Font font = static_cast<Xv_Font>(xv_get(panel, WIN_FONT));
        graphics_info *information = xv_init_olgx(
            panel, &three_d, font);
        return {panel, cache, information};
    }

    native::rgba pixel_color(Panel panel, unsigned long pixel) {
        XColor color = {};
        color.pixel = pixel;
        Display *display = linux::openlook::cached_display;
        Cms cms = panel
                      ? static_cast<Cms>(xv_get(panel, WIN_CMS))
                      : XV_NULL;
        Colormap colormap = cms
                                ? static_cast<Colormap>(xv_get(
                                      cms, CMS_CMAP_ID))
                                : DefaultColormap(
                                      display,
                                      DefaultScreen(display));
        XQueryColor(display, colormap, &color);
        return native::rgba(
            static_cast<std::uint8_t>(color.red >> 8),
            static_cast<std::uint8_t>(color.green >> 8),
            static_cast<std::uint8_t>(color.blue >> 8),
            255);
    }

    native::theme::palette fallback_palette() {
        native::theme::palette result;
        result.button_bg = native::rgba(204, 204, 204, 255);
        result.button_border = native::rgba(0, 0, 0, 255);
        result.button_highlight = native::rgba(255, 255, 255, 255);
        result.button_shadow = native::rgba(128, 128, 128, 255);
        result.button_text = native::rgba(0, 0, 0, 255);
        result.button_disabled_text = result.button_shadow;
        result.button_hot_bg = result.button_bg;
        result.button_hot_text = result.button_text;
        result.button_pressed_bg = result.button_shadow;
        result.button_pressed_text = result.button_text;
        result.menu_bar_bg = result.button_bg;
        result.menu_bar_line_top = result.button_highlight;
        result.menu_bar_line_bottom = result.button_shadow;
        result.menu_text = result.button_text;
        result.menu_disabled_text = result.button_disabled_text;
        result.menu_hot_bg = result.button_bg;
        result.menu_hot_text = result.button_text;
        result.menu_popup_bg = result.button_bg;
        result.menu_popup_border = result.button_border;
        result.content_bg = native::rgba(255, 255, 255, 255);
        result.content_alt_bg = result.button_bg;
        result.content_text = result.button_text;
        result.selection_bg = result.button_text;
        result.selection_text = result.button_bg;
        result.selection_inactive_bg = result.button_shadow;
        result.selection_inactive_text = result.button_text;
        result.separator = result.button_shadow;
        result.focus = result.button_text;
        return result;
    }

    int native_state(const native::theme::state &state) {
        int result = state.pressed || state.selected
                         ? olgx_invoked
                         : olgx_normal;
        if (state.disabled)
            result |= olgx_inactive;
        return result;
    }

    class openlook_theme final : public linux::emulated_theme
    {
    public:
        explicit openlook_theme(native::gpx &graphics)
            : emulated_theme(graphics) {}

        metrics defaults() const override {
            metrics result;
            result.menu_bar_height = 30;
            result.menu_item_height = text_height() + 8;
            result.popup_width = 180;
            result.text_padding_x = 8;
            result.check_height = 24;
            result.radio_height = 24;
            result.list_item_height = text_height() + 5;
            result.table_row_height = result.list_item_height;
            result.header_height = text_height() + 6;
            result.tab_height = result.header_height;
            result.scrollbar_extent = std::max(
                1, scrollbar_width_for_scale(WIN_SCALE_MEDIUM));
            result.scrollbar_min_thumb = result.scrollbar_extent;
            return result;
        }

        palette native_palette() const override {
            palette result = fallback_palette();
            openlook_target target = target_from(_g);
            if (!target.panel || !linux::openlook::cached_display)
                return result;

            Cms cms = static_cast<Cms>(xv_get(
                target.panel, WIN_CMS));
            auto *pixels =
                cms
                    ? reinterpret_cast<unsigned long *>(xv_get(
                          cms, CMS_INDEX_TABLE))
                    : nullptr;
            const int size = cms
                                 ? static_cast<int>(xv_get(
                                       cms, CMS_SIZE))
                                 : 0;
            if (!pixels || size < 2)
                return result;

            const bool control = static_cast<bool>(xv_get(
                cms, CMS_CONTROL_CMS));
            const unsigned long black = pixels[size - 1];
            const unsigned long background = pixels[0];
            result.button_text = pixel_color(target.panel, black);
            result.button_bg = pixel_color(target.panel, background);
            result.button_border = result.button_text;
            if (control && size >= CMS_CONTROL_COLORS + 1) {
                result.button_bg = pixel_color(
                    target.panel, pixels[CMS_CONTROL_BG1]);
                result.button_shadow = pixel_color(
                    target.panel, pixels[CMS_CONTROL_BG2]);
                result.button_border = pixel_color(
                    target.panel, pixels[CMS_CONTROL_BG3]);
                result.button_highlight = pixel_color(
                    target.panel, pixels[CMS_CONTROL_HIGHLIGHT]);
            }
            result.button_disabled_text = result.button_shadow;
            result.button_hot_bg = result.button_bg;
            result.button_hot_text = result.button_text;
            result.button_pressed_bg = result.button_shadow;
            result.button_pressed_text = result.button_text;
            result.menu_bar_bg = result.button_bg;
            result.menu_bar_line_top = result.button_highlight;
            result.menu_bar_line_bottom = result.button_shadow;
            result.menu_text = result.button_text;
            result.menu_disabled_text = result.button_disabled_text;
            result.menu_hot_bg = result.button_bg;
            result.menu_hot_text = result.button_text;
            result.menu_popup_bg = result.button_bg;
            result.menu_popup_border = result.button_border;
            result.content_bg = result.button_bg;
            result.content_alt_bg = result.button_highlight;
            result.content_text = result.button_text;
            result.selection_bg = result.button_text;
            result.selection_text = result.button_bg;
            result.selection_inactive_bg = result.button_shadow;
            result.selection_inactive_text = result.button_text;
            result.separator = result.button_shadow;
            result.focus = result.button_text;
            return result;
        }

        theme &draw_surface(
            const native::rect &bounds,
            native::surface_kind kind,
            const state &element_state) override {
            if (kind != native::surface_kind::ruler &&
                kind != native::surface_kind::header &&
                kind != native::surface_kind::table_header) {
                return emulated_theme::draw_surface(
                    bounds, kind, element_state);
            }
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_surface(
                    bounds, kind, element_state);
            }
            const palette colors = native_palette();
            const native::rgba fill =
                element_state.pressed || element_state.selected
                    ? colors.button_pressed_bg
                    : element_state.hot
                          ? colors.button_hot_bg
                          : colors.button_bg;
            const int left = bounds.p.x;
            const int top = bounds.p.y;
            const int width = bounds.d.w;
            const int height = bounds.d.h;
            if (width < 6 || height < 6) {
                _g.set_pen(1)
                    .set_ink(fill)
                    .draw_rect(bounds, true)
                    .set_ink(colors.button_border)
                    .draw_rect(bounds, false);
                return *this;
            }

            const int right = left + width - 1;
            const int bottom = top + height - 1;
            _g.set_pen(1)
                .set_ink(colors.button_bg)
                .draw_rect(bounds, true)
                .set_ink(fill)
                .draw_rect(native::rect(
                               static_cast<native::coord>(left + 2),
                               static_cast<native::coord>(top),
                               static_cast<native::dim>(width - 4),
                               static_cast<native::dim>(height)),
                           true)
                .draw_rect(native::rect(
                               static_cast<native::coord>(left),
                               static_cast<native::coord>(top + 2),
                               static_cast<native::dim>(width),
                               static_cast<native::dim>(height - 4)),
                           true)
                .set_ink(colors.button_border)
                .draw_polyline({
                    native::point(
                        static_cast<native::coord>(left + 2),
                        static_cast<native::coord>(top)),
                    native::point(
                        static_cast<native::coord>(right - 2),
                        static_cast<native::coord>(top)),
                    native::point(
                        static_cast<native::coord>(right - 1),
                        static_cast<native::coord>(top + 1)),
                    native::point(
                        static_cast<native::coord>(right),
                        static_cast<native::coord>(top + 2)),
                    native::point(
                        static_cast<native::coord>(right),
                        static_cast<native::coord>(bottom - 2)),
                    native::point(
                        static_cast<native::coord>(right - 1),
                        static_cast<native::coord>(bottom - 1)),
                    native::point(
                        static_cast<native::coord>(right - 2),
                        static_cast<native::coord>(bottom)),
                    native::point(
                        static_cast<native::coord>(left + 2),
                        static_cast<native::coord>(bottom)),
                    native::point(
                        static_cast<native::coord>(left + 1),
                        static_cast<native::coord>(bottom - 1)),
                    native::point(
                        static_cast<native::coord>(left),
                        static_cast<native::coord>(bottom - 2)),
                    native::point(
                        static_cast<native::coord>(left),
                        static_cast<native::coord>(top + 2)),
                    native::point(
                        static_cast<native::coord>(left + 1),
                        static_cast<native::coord>(top + 1)),
                    native::point(
                        static_cast<native::coord>(left + 2),
                        static_cast<native::coord>(top))});
            return *this;
        }

        theme &draw_button(const native::rect &bounds,
                           const std::string &text,
                           const state &element_state) override {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_button(
                    bounds, text, element_state);
            }
            olgx_draw_button(
                target.information,
                target.cache->backbuffer,
                bounds.p.x,
                bounds.p.y,
                bounds.d.w,
                bounds.d.h,
                const_cast<char *>(text.c_str()),
                native_state(element_state));
            return *this;
        }

        theme &draw_menu_bar(const native::rect &bounds) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            _g.set_pen(1)
                .set_ink(colors.menu_bar_bg)
                .draw_rect(bounds, true);
            return *this;
        }

        theme &draw_menu_title(const native::rect &bounds,
                               const std::string &text,
                               const state &element_state) override {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_menu_title(
                    bounds, text, element_state);
            }
            olgx_draw_button(
                target.information,
                target.cache->backbuffer,
                bounds.p.x,
                bounds.p.y,
                bounds.d.w,
                bounds.d.h,
                const_cast<char *>(text.c_str()),
                native_state(element_state) |
                    olgx_vertical_menu_mark);
            return *this;
        }

        theme &draw_menu_item(const native::rect &bounds,
                              const std::string &text,
                              const state &element_state) override {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_menu_item(
                    bounds, text, element_state);
            }
            olgx_draw_button(
                target.information,
                target.cache->backbuffer,
                bounds.p.x,
                bounds.p.y,
                bounds.d.w,
                bounds.d.h,
                const_cast<char *>(text.c_str()),
                native_state(element_state) | olgx_menu_item |
                    olgx_erase);
            return *this;
        }

        theme &draw_popup_frame(
            const native::rect &bounds) override {
            return draw_native_frame(bounds, false);
        }

        theme &draw_list_item(const native::rect &bounds,
                              const std::string &text,
                              const state &element_state) override {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (target.information && element_state.selected) {
                olgx_draw_box(target.information,
                              target.cache->backbuffer,
                              bounds.p.x,
                              bounds.p.y,
                              bounds.d.w,
                              bounds.d.h,
                              olgx_invoked,
                              TRUE);
            } else {
                _g.set_ink(native_palette().menu_popup_bg)
                    .draw_rect(bounds, true);
            }
            const palette colors = native_palette();
            draw_control_label(bounds,
                               bounds.p.x + 6,
                               text,
                               element_state,
                               colors);
            return *this;
        }

        theme &draw_check(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override {
            return draw_indicator(bounds, text, element_state);
        }

        theme &draw_radio(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override {
            return draw_button(bounds, text, element_state);
        }

        theme &draw_list(const native::rect &bounds,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &element_state) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            _g.set_pen(1)
                .set_ink(colors.menu_popup_bg)
                .draw_rect(bounds, true);
            draw_native_frame(bounds, true);
            if (bounds.d.w <= 8 || bounds.d.h <= 8)
                return *this;
            const native::rect content(
                bounds.p.x + 4,
                bounds.p.y + 4,
                bounds.d.w - 8,
                bounds.d.h - 8);
            _g.set_clip(_g.get_clip().intersect(content));
            const int height = defaults().list_item_height;
            for (std::size_t index = 0;
                 index < items.size();
                 ++index) {
                const int y = content.p.y +
                              static_cast<int>(index) * height;
                if (y >= content.y2())
                    break;
                state row_state = element_state;
                row_state.selected =
                    static_cast<int>(index) == selected_index;
                draw_list_item(
                    native::rect(content.p.x,
                                 y,
                                 content.d.w,
                                 std::min(height,
                                          content.y2() - y)),
                    items[index],
                    row_state);
            }
            return *this;
        }

        theme &draw_text_edit_frame(
            const native::rect &bounds,
            const state &) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            _g.set_ink(colors.menu_popup_bg).draw_rect(bounds, true);
            return draw_native_frame(bounds, true);
        }

        theme &draw_disclosure(
            const native::rect &bounds,
            native::disclosure_state disclosure,
            const state &element_state) override {
            return draw_native_mark(
                bounds,
                disclosure == native::disclosure_state::expanded
                    ? olgx_vertical_menu_mark
                    : olgx_horizontal_menu_mark,
                element_state,
                disclosure,
                std::nullopt);
        }

        theme &draw_sort_indicator(
            const native::rect &bounds,
            native::sort_indicator_state direction,
            const state &element_state) override {
            return draw_native_mark(
                bounds,
                direction == native::sort_indicator_state::ascending
                    ? olgx_vertical_back_menu_mark
                    : olgx_vertical_menu_mark,
                element_state,
                std::nullopt,
                direction);
        }

        theme &draw_caption_button(
            const native::rect &bounds,
            native::caption_button_kind kind,
            const state &element_state) override {
            if (kind == native::caption_button_kind::close) {
                saved_state saved(_g);
                const palette colors = native_palette();
                const int inset = std::max(
                    2, std::min<int>(bounds.d.w, bounds.d.h) / 4);
                _g.set_pen(element_state.pressed ? 2 : 1)
                    .set_ink(element_state.disabled
                                 ? colors.button_disabled_text
                                 : colors.button_text)
                    .draw_line(
                        native::point(bounds.p.x + inset,
                                      bounds.p.y + inset),
                        native::point(bounds.x2() - inset - 1,
                                      bounds.y2() - inset - 1))
                    .draw_line(
                        native::point(bounds.x2() - inset - 1,
                                      bounds.p.y + inset),
                        native::point(bounds.p.x + inset,
                                      bounds.y2() - inset - 1));
                return *this;
            }

            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_caption_button(
                    bounds, kind, element_state);
            }

            // OPEN LOOK captions use the same pushpin glyph as pinned
            // menus. A docked pane is "in"; an auto-hidden pane is
            // "out". Show the prospective state while the pin is held.
            bool pushed_in =
                kind == native::caption_button_kind::pin;
            if (element_state.pressed)
                pushed_in = !pushed_in;
            // The caller has already painted its background.
            // Do not pass OLGX_ERASE here: that implementation uses
            // XClearArea (Window-only), while Native themes paint into a
            // backing Pixmap before copying to the XView window.
            int pin_state = pushed_in ? olgx_pushpin_in
                                      : olgx_pushpin_out;
            if (element_state.disabled)
                pin_state |= olgx_inactive;

            // The stock control glyph is fourteen pixels high for the
            // XView fonts used here. OLGX owns its exact glyph metrics;
            // these offsets only center its origin in our compact slot.
            constexpr int pin_extent = 14;
            const int x = bounds.p.x + std::max(
                0, (static_cast<int>(bounds.d.w) - pin_extent) / 2);
            const int y = bounds.p.y + std::max(
                0, (static_cast<int>(bounds.d.h) - pin_extent) / 2);
            olgx_draw_pushpin(target.information,
                              target.cache->backbuffer,
                              x,
                              y,
                              pin_state);
            return *this;
        }

        theme &draw_scrollbar_part(
            const native::rect &bounds,
            native::scrollbar_orientation orientation,
            native::scrollbar_part part,
            const state &element_state) override {
            auto *window_graphics =
                dynamic_cast<native::gpx_wnd *>(&_g);
            native::wnd *window = window_graphics
                                      ? window_graphics->window()
                                      : nullptr;
            if (dynamic_cast<native::icon_view *>(window) ||
                dynamic_cast<native::tree_view *>(window) ||
                dynamic_cast<native::table_view *>(window)) {
                // These Canvas-backed controls attach real XView
                // Scrollbar objects. Their portable geometry still
                // reserves the native extent, but the duplicate
                // backbuffer track must remain unpainted.
                return *this;
            }
            return emulated_theme::draw_scrollbar_part(
                bounds, orientation, part, element_state);
        }

    protected:
        int text_width(const std::string &text) const override {
            return native::font_t::stock(native::font_role::control)
                .measure_text(text)
                .width;
        }

        int text_height() const override {
            return std::max(
                1,
                native::font_t::stock(native::font_role::control)
                    .get_metrics()
                    .height);
        }

    private:
        theme &draw_native_mark(
            const native::rect &bounds,
            int mark,
            const state &element_state,
            std::optional<native::disclosure_state> disclosure,
            std::optional<native::sort_indicator_state> sort) {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return disclosure
                           ? draw_disclosure_fallback(
                                 bounds, *disclosure, element_state)
                           : draw_sort_indicator_fallback(
                                 bounds, *sort, element_state);
            }
            int olgx_state = mark;
            if (element_state.disabled)
                olgx_state |= olgx_inactive;
            const native::size mark_size =
                linux::openlook::menu_mark_dimensions(
                    target.information);
            const int x = bounds.p.x + std::max(
                0,
                (static_cast<int>(bounds.d.w) -
                 static_cast<int>(mark_size.w)) /
                    2);
            const int y = bounds.p.y + std::max(
                0,
                (static_cast<int>(bounds.d.h) -
                 static_cast<int>(mark_size.h)) /
                    2);
            olgx_draw_menu_mark(target.information,
                                target.cache->backbuffer,
                                x,
                                y,
                                olgx_state,
                                TRUE);
            return *this;
        }

        openlook_target native_target(
            const native::rect &bounds) const {
            if (!contains(_g.get_clip(), bounds))
                return {};
            return target_from(_g);
        }

        theme &draw_native_frame(const native::rect &bounds,
                                 bool inset) {
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return inset
                           ? emulated_theme::draw_text_edit_frame(
                                 bounds, {})
                           : emulated_theme::draw_popup_frame(bounds);
            }
            olgx_draw_box(target.information,
                          target.cache->backbuffer,
                          bounds.p.x,
                          bounds.p.y,
                          bounds.d.w,
                          bounds.d.h,
                          inset ? olgx_invoked : olgx_normal,
                          FALSE);
            if (inset && bounds.d.w > 2 && bounds.d.h > 2) {
                olgx_draw_box(target.information,
                              target.cache->backbuffer,
                              bounds.p.x + 1,
                              bounds.p.y + 1,
                              bounds.d.w - 2,
                              bounds.d.h - 2,
                              olgx_normal,
                              FALSE);
            }
            return *this;
        }

        theme &draw_indicator(const native::rect &bounds,
                              const std::string &text,
                              const state &element_state) {
            saved_state saved(_g);
            openlook_target target = native_target(bounds);
            if (!target.information) {
                return emulated_theme::draw_check(
                    bounds, text, element_state);
            }
            const int side = std::min(
                16, std::max(10, static_cast<int>(bounds.d.h) - 4));
            const int y = bounds.p.y +
                          std::max(0,
                                   (static_cast<int>(bounds.d.h) -
                                    side) /
                                       2);
            int indicator_state = element_state.selected
                                      ? olgx_checked
                                      : olgx_normal;
            if (element_state.disabled)
                indicator_state |= olgx_inactive;
            olgx_draw_check_box(target.information,
                                target.cache->backbuffer,
                                bounds.p.x + 2,
                                y,
                                indicator_state);
            draw_control_label(bounds,
                               bounds.p.x + side + 8,
                               text,
                               element_state,
                               native_palette());
            return *this;
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<openlook_theme>(painter);
    }
} // namespace native
