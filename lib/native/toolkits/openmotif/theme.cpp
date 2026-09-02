//
// Implements the OpenMotif theme with Motif resource colors and Xme
// drawing primitives. Image targets fall back to a Motif-specific
// emulation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>
#include <optional>

#include <Xm/DrawP.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>

#include <native.h>
#include <native/theme.h>

#include "../emulated_theme.h"
#include "globals.h"
#include "theme_support.h"

namespace
{
    constexpr int infolib_disclosure_side = 13;
    constexpr int infolib_disclosure_inset = 3;

    native::rgba x_resource_color(const char *application,
                                  const char *resource,
                                  native::rgba fallback) {
        Display *display = linux::openmotif::cached_display;
        if (!display)
            return fallback;
        const char *value = XGetDefault(display, application, resource);
        XColor color{};
        if (!value || !XParseColor(
                          display,
                          DefaultColormap(display, DefaultScreen(display)),
                          value,
                          &color)) {
            return fallback;
        }
        return native::rgba(
            static_cast<std::uint8_t>(color.red >> 8),
            static_cast<std::uint8_t>(color.green >> 8),
            static_cast<std::uint8_t>(color.blue >> 8),
            255);
    }

    struct motif_indicator_target
    {
        linux::openmotif::theme_target drawing;
        Pixel background = 0;
        Pixel foreground = 0;
        Pixel top = 0;
        Pixel bottom = 0;
        Pixel select = 0;
        Dimension size = 16;
        Dimension shadow = 1;
        XtEnum indicator = XmINDICATOR_FILL;
    };

    class motif_theme final : public linux::emulated_theme
    {
    public:
        explicit motif_theme(native::gpx &g)
            : emulated_theme(g)
            , _button_probe(nullptr)
            , _check_probe(nullptr)
            , _radio_probe(nullptr)
            , _list_probe(nullptr) {}

        ~motif_theme() override {
            if (_list_probe)
                XtDestroyWidget(_list_probe);
            if (_radio_probe)
                XtDestroyWidget(_radio_probe);
            if (_check_probe)
                XtDestroyWidget(_check_probe);
            if (_button_probe)
                XtDestroyWidget(_button_probe);
        }

        metrics defaults() const override {
            metrics m;
            m.menu_bar_height = 24;
            m.menu_item_height = 20;
            m.popup_width = 180;
            m.text_padding_x = 3;
            m.list_item_height = text_height() + 2;
            m.table_row_height = m.list_item_height;
            m.header_height = text_height() + 8;
            if (native_infolib_tree()) {
                // Dtinfo's OutlineList is a compact, indentation-only tree.
                m.disclosure_size = infolib_disclosure_side;
                m.tree_lines_visible = false;
                m.tree_row_height = text_height() + 4;
                m.tree_horizontal_padding = 16;
                m.tree_indent_width = 17;
                m.tree_item_gap = 3;
                m.tree_icon_vertical_padding = 2;
            }
            return m;
        }

        palette native_palette() const override {
            Widget reference =
                linux::openmotif::theme_reference_widget(_g);
            Widget button = button_probe(reference);
            Widget list = list_probe(reference);
            palette result = linux::openmotif::theme_palette(
                reference, button, list);
            if (native_infolib_tree()) {
                result.content_bg = x_resource_color(
                    "OpenWindows",
                    "DataBackground",
                    native::rgba(104, 111, 130, 255));
                result.content_text = x_resource_color(
                    "OpenWindows",
                    "DataForeground",
                    native::rgba(255, 255, 255, 255));
                result.selection_bg = result.content_text;
                result.selection_text = result.content_bg;
                result.selection_inactive_bg = result.selection_bg;
                result.selection_inactive_text = result.selection_text;
            }
            return result;
        }

        theme &draw_button(const native::rect &r,
                           const std::string &text,
                           const state &s) override {
            saved_state saved(_g);
            const palette colors = native_palette();
            const native::rgba background =
                s.pressed ? colors.button_pressed_bg
                          : colors.button_bg;
            const native::rgba foreground =
                s.disabled ? colors.button_disabled_text
                           : colors.button_text;
            Widget probe = button_probe(
                linux::openmotif::theme_reference_widget(_g));
            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            draw_shadow(r,
                        s.pressed ? XmSHADOW_IN : XmSHADOW_OUT,
                        shadow_thickness(probe, 2),
                        probe);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_clip(_g.get_clip().intersect(r));
            _g.set_ink(foreground)
                .draw_text(
                    text,
                    native::point(
                        r.p.x +
                            std::max(0,
                                     (static_cast<int>(r.d.w) -
                                      text_width(text)) /
                                         2),
                        text_y(r)));
            return *this;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            emulated_theme::draw_menu_bar(r);
            draw_shadow(r, XmSHADOW_OUT, 1);
            return *this;
        }

        theme &draw_popup_frame(const native::rect &r) override {
            emulated_theme::draw_popup_frame(r);
            draw_shadow(r, XmSHADOW_OUT, 2);
            return *this;
        }

        theme &draw_check(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            motif_indicator_target target;
            if (!get_indicator_target(false, target))
                return emulated_theme::draw_check(r, text, s);

            saved_state saved(_g);
            palette colors = native_palette();
            colors.button_bg = linux::openmotif::theme_pixel_color(
                target.drawing.widget, target.background);
            colors.button_text = linux::openmotif::theme_pixel_color(
                target.drawing.widget, target.foreground);
            colors.button_disabled_text =
                linux::openmotif::theme_pixel_color(
                    target.drawing.widget, target.bottom);
            const native::rect area = indicator_bounds(
                r, static_cast<int>(target.size), 5);
            const native::rect indicator = check_bounds(area);
            _g.set_pen(1)
                .set_ink(colors.button_bg)
                .draw_rect(r, true);
            draw_check_indicator(target, indicator, s);
            draw_control_label(
                r, area.x2() + 5, text, s, colors);
            return *this;
        }

        theme &draw_radio(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            motif_indicator_target target;
            if (!get_indicator_target(true, target))
                return emulated_theme::draw_radio(r, text, s);

            saved_state saved(_g);
            palette colors = native_palette();
            colors.button_bg = linux::openmotif::theme_pixel_color(
                target.drawing.widget, target.background);
            colors.button_text = linux::openmotif::theme_pixel_color(
                target.drawing.widget, target.foreground);
            colors.button_disabled_text =
                linux::openmotif::theme_pixel_color(
                    target.drawing.widget, target.bottom);
            const native::rect indicator = indicator_bounds(
                r, static_cast<int>(target.size), 7);
            _g.set_pen(1)
                .set_ink(colors.button_bg)
                .draw_rect(r, true);
            draw_radio_indicator(target, indicator, s);
            draw_control_label(
                r, indicator.x2() + 5, text, s, colors);
            return *this;
        }

        theme &draw_list(const native::rect &r,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &s) override {
            saved_state saved(_g);
            Widget probe = list_probe(
                linux::openmotif::theme_reference_widget(_g));
            Dimension margin_width = 0;
            Dimension margin_height = 0;
            Dimension highlight = 0;
            Dimension spacing = 0;
            if (probe) {
                XtVaGetValues(probe,
                              XmNmarginWidth,
                              &margin_width,
                              XmNmarginHeight,
                              &margin_height,
                              XmNhighlightThickness,
                              &highlight,
                              XmNlistSpacing,
                              &spacing,
                              nullptr);
            }
            const int shadow = shadow_thickness(probe, 2);
            const palette colors = native_palette();
            _g.set_pen(1)
                .set_ink(colors.menu_popup_bg)
                .draw_rect(r, true);
            draw_shadow(
                r,
                XmSHADOW_IN,
                shadow,
                probe);
            const int inset_x = shadow + highlight + margin_width;
            const int inset_y = shadow + highlight + margin_height;
            if (static_cast<int>(r.d.w) <= inset_x * 2 ||
                static_cast<int>(r.d.h) <= inset_y * 2) {
                return *this;
            }
            const native::rect content(
                r.p.x + inset_x,
                r.p.y + inset_y,
                r.d.w - inset_x * 2,
                r.d.h - inset_y * 2);
            _g.set_clip(_g.get_clip().intersect(content));
            const int item_height = std::max(
                1,
                defaults().list_item_height +
                    static_cast<int>(spacing));
            for (std::size_t index = 0; index < items.size(); ++index) {
                const int y = content.p.y +
                              static_cast<int>(index) * item_height;
                if (y >= content.y2())
                    break;
                state item_state = s;
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

        theme &draw_text_edit_frame(
            const native::rect &r,
            const state &s) override {
            emulated_theme::draw_text_edit_frame(r, s);
            draw_shadow(r, XmSHADOW_IN, 2);
            return *this;
        }

        theme &draw_disclosure(
            const native::rect &r,
            native::disclosure_state disclosure,
            const state &s) override {
            if (native_infolib_tree()) {
                saved_state saved(_g);
                const palette colors = native_palette();
                const int left = r.p.x;
                const int top = r.p.y;
                const int right = r.x2() - 1;
                const int bottom = r.y2() - 1;
                const int horizontal_middle = (left + right) / 2;
                const int vertical_middle = (top + bottom) / 2;
                std::vector<native::point> triangle;
                if (disclosure == native::disclosure_state::expanded) {
                    triangle = {
                        {static_cast<native::coord>(left),
                         static_cast<native::coord>(
                             top + infolib_disclosure_inset)},
                        {static_cast<native::coord>(right),
                         static_cast<native::coord>(
                             top + infolib_disclosure_inset)},
                        {static_cast<native::coord>(horizontal_middle),
                         static_cast<native::coord>(
                             bottom - infolib_disclosure_inset)}};
                } else {
                    triangle = {
                        {static_cast<native::coord>(
                             left + infolib_disclosure_inset),
                         static_cast<native::coord>(top)},
                        {static_cast<native::coord>(
                             right - infolib_disclosure_inset),
                         static_cast<native::coord>(vertical_middle)},
                        {static_cast<native::coord>(
                             left + infolib_disclosure_inset),
                         static_cast<native::coord>(bottom)}};
                }
                _g.set_pen(1)
                    .set_ink(s.disabled
                                 ? colors.selection_inactive_text
                                 : (s.selected ? colors.selection_text
                                               : colors.content_text))
                    .draw_polygon(triangle, true);
                return *this;
            }
            return draw_native_arrow(
                r,
                disclosure == native::disclosure_state::expanded
                    ? XmARROW_DOWN
                    : XmARROW_RIGHT,
                s,
                disclosure,
                std::nullopt);
        }

        theme &draw_focus(const native::rect &r,
                          const state &s) override {
            if (native_infolib_tree())
                return *this;
            return emulated_theme::draw_focus(r, s);
        }

        theme &draw_sort_indicator(
            const native::rect &r,
            native::sort_indicator_state direction,
            const state &s) override {
            return draw_native_arrow(
                r,
                direction == native::sort_indicator_state::ascending
                    ? XmARROW_UP
                    : XmARROW_DOWN,
                s,
                std::nullopt,
                direction);
        }

    protected:
        int text_width(const std::string &text) const override {
            XFontStruct *font =
                linux::openmotif::theme_control_font();
            if (!font)
                return static_cast<int>(text.size()) * 7;
            const int width = XTextWidth(
                font, text.c_str(), static_cast<int>(text.size()));
            XFreeFontInfo(nullptr, font, 1);
            return width;
        }

        int text_height() const override {
            XFontStruct *font =
                linux::openmotif::theme_control_font();
            if (!font)
                return 8;
            const int height =
                std::max(1, font->ascent + font->descent);
            XFreeFontInfo(nullptr, font, 1);
            return height;
        }

    private:
        bool native_infolib_tree() const {
            const linux::openmotif::theme_target target =
                linux::openmotif::theme_target_from(
                    const_cast<native::gpx &>(_g));
            const auto *tree =
                dynamic_cast<const native::tree_view *>(target.owner);
            return tree &&
                   tree->get_presentation() ==
                       native::tree_view_presentation::native;
        }

        theme &draw_native_arrow(
            const native::rect &r,
            unsigned char direction,
            const state &s,
            std::optional<native::disclosure_state> disclosure,
            std::optional<native::sort_indicator_state> sort) {
            linux::openmotif::theme_target target =
                linux::openmotif::theme_target_from(_g);
            if (!target.widget || !target.cache ||
                !target.cache->backbuffer ||
                !linux::openmotif::cached_display) {
                return disclosure
                           ? draw_disclosure_fallback(r, *disclosure, s)
                           : draw_sort_indicator_fallback(r, *sort, s);
            }
            Pixel top = 0;
            Pixel bottom = 0;
            Pixel center = 0;
            XtVaGetValues(target.widget,
                          XmNtopShadowColor,
                          &top,
                          XmNbottomShadowColor,
                          &bottom,
                          XmNforeground,
                          &center,
                          nullptr);
            if (s.disabled)
                center = bottom;
            GC top_gc = linux::openmotif::theme_gc(
                target, top, _g.get_clip());
            GC bottom_gc = linux::openmotif::theme_gc(
                target, bottom, _g.get_clip());
            GC center_gc = linux::openmotif::theme_gc(
                target, center, _g.get_clip());
            XmeDrawArrow(linux::openmotif::cached_display,
                         target.cache->backbuffer,
                         top_gc,
                         bottom_gc,
                         center_gc,
                         r.p.x,
                         r.p.y,
                         r.d.w,
                         r.d.h,
                         1,
                         direction);
            XFreeGC(linux::openmotif::cached_display, center_gc);
            XFreeGC(linux::openmotif::cached_display, bottom_gc);
            XFreeGC(linux::openmotif::cached_display, top_gc);
            return *this;
        }

        unsigned int shadow_thickness(
            Widget widget, unsigned int fallback) const {
            if (!widget)
                return fallback;
            Dimension result = fallback;
            XtVaGetValues(widget,
                          XmNshadowThickness,
                          &result,
                          nullptr);
            return std::max(1U, static_cast<unsigned int>(result));
        }

        native::rect indicator_bounds(const native::rect &r,
                                      int preferred,
                                      int minimum) const {
            const int side = std::max(
                minimum,
                std::min(preferred, static_cast<int>(r.d.h) - 4));
            return native::rect(
                r.p.x + 3,
                r.p.y +
                    std::max(0, (static_cast<int>(r.d.h) - side) / 2),
                side,
                side);
        }

        native::rect check_bounds(const native::rect &area) const {
            const int side = static_cast<int>(area.d.w);
            const int edge = std::max(
                1, side - 3 - std::max(0, (side - 10) / 10));
            const int delta = (side - edge) / 2;
            return native::rect(area.p.x + delta,
                                area.p.y + delta,
                                edge,
                                edge);
        }

        bool get_indicator_target(
            bool radio, motif_indicator_target &target) const {
            target.drawing =
                linux::openmotif::theme_target_from(_g);
            if (!target.drawing.widget || !target.drawing.cache ||
                !target.drawing.cache->backbuffer ||
                !linux::openmotif::cached_display) {
                return false;
            }
            Widget probe = toggle_probe(target.drawing.widget, radio);
            if (!probe)
                return false;
            XtVaGetValues(probe,
                          XmNbackground,
                          &target.background,
                          XmNforeground,
                          &target.foreground,
                          XmNtopShadowColor,
                          &target.top,
                          XmNbottomShadowColor,
                          &target.bottom,
                          XmNselectColor,
                          &target.select,
                          XmNindicatorSize,
                          &target.size,
                          XmNdetailShadowThickness,
                          &target.shadow,
                          XmNindicatorOn,
                          &target.indicator,
                          nullptr);
            return true;
        }

        Widget toggle_probe(Widget parent, bool radio) const {
            Widget &probe = radio ? _radio_probe : _check_probe;
            if (!probe) {
                probe = XtVaCreateWidget(
                    const_cast<char *>(radio ? "radio" : "check"),
                    xmToggleButtonWidgetClass,
                    parent,
                    XmNindicatorType,
                    radio ? XmONE_OF_MANY : XmN_OF_MANY,
                    nullptr);
            }
            return probe;
        }

        Widget button_probe(Widget parent) const {
            if (!_button_probe && parent) {
                _button_probe = XtVaCreateWidget(
                    const_cast<char *>("button"),
                    xmPushButtonWidgetClass,
                    parent,
                    nullptr);
            }
            return _button_probe;
        }

        Widget list_probe(Widget parent) const {
            if (!_list_probe && parent) {
                _list_probe = XtVaCreateWidget(
                    const_cast<char *>("list"),
                    xmListWidgetClass,
                    parent,
                    nullptr);
            }
            return _list_probe;
        }

        void draw_check_indicator(
            const motif_indicator_target &target,
            const native::rect &indicator,
            const state &s) {
            const Pixel top = s.selected ? target.bottom : target.top;
            const Pixel bottom =
                s.selected ? target.top : target.bottom;
            const Pixel fill = s.selected ? target.select
                                          : target.background;
            const unsigned int shadow = std::max(
                1U, static_cast<unsigned int>(target.shadow));
            GC top_gc = linux::openmotif::theme_gc(
                target.drawing, top, _g.get_clip());
            GC bottom_gc = linux::openmotif::theme_gc(
                target.drawing, bottom, _g.get_clip());
            GC fill_gc = linux::openmotif::theme_gc(
                target.drawing, fill, _g.get_clip());
            XmeDrawShadows(linux::openmotif::cached_display,
                           target.drawing.cache->backbuffer,
                           top_gc,
                           bottom_gc,
                           indicator.p.x,
                           indicator.p.y,
                           indicator.d.w,
                           indicator.d.h,
                           shadow,
                           XmSHADOW_OUT);
            if (indicator.d.w > shadow * 2 &&
                indicator.d.h > shadow * 2) {
                XFillRectangle(
                    linux::openmotif::cached_display,
                    target.drawing.cache->backbuffer,
                    fill_gc,
                    indicator.p.x + shadow,
                    indicator.p.y + shadow,
                    indicator.d.w - shadow * 2,
                    indicator.d.h - shadow * 2);
            }
            if (s.selected &&
                (target.indicator &
                 (XmINDICATOR_CHECK_GLYPH |
                  XmINDICATOR_CROSS_GLYPH))) {
                const Pixel glyph =
                    s.disabled ? target.bottom : target.foreground;
                GC glyph_gc = linux::openmotif::theme_gc(
                    target.drawing, glyph, _g.get_clip());
                XmeDrawIndicator(
                    linux::openmotif::cached_display,
                    target.drawing.cache->backbuffer,
                    glyph_gc,
                    indicator.p.x,
                    indicator.p.y,
                    indicator.d.w,
                    indicator.d.h,
                    shadow,
                    target.indicator);
                XFreeGC(linux::openmotif::cached_display, glyph_gc);
            }
            XFreeGC(linux::openmotif::cached_display, fill_gc);
            XFreeGC(linux::openmotif::cached_display, bottom_gc);
            XFreeGC(linux::openmotif::cached_display, top_gc);
        }

        void draw_radio_indicator(
            const motif_indicator_target &target,
            const native::rect &indicator,
            const state &s) {
            const Pixel top = s.selected ? target.bottom : target.top;
            const Pixel bottom =
                s.selected ? target.top : target.bottom;
            GC top_gc = linux::openmotif::theme_gc(
                target.drawing, top, _g.get_clip());
            GC bottom_gc = linux::openmotif::theme_gc(
                target.drawing, bottom, _g.get_clip());
            GC center_gc = linux::openmotif::theme_gc(
                target.drawing,
                s.selected
                    ? (s.disabled ? target.bottom : target.select)
                    : target.background,
                _g.get_clip());
            XmeDrawDiamond(
                linux::openmotif::cached_display,
                target.drawing.cache->backbuffer,
                top_gc,
                bottom_gc,
                center_gc,
                indicator.p.x,
                indicator.p.y,
                indicator.d.w,
                indicator.d.h,
                std::max(
                    1U, static_cast<unsigned int>(target.shadow)),
                1);
            XFreeGC(linux::openmotif::cached_display, center_gc);
            XFreeGC(linux::openmotif::cached_display, bottom_gc);
            XFreeGC(linux::openmotif::cached_display, top_gc);
        }

        void draw_shadow(const native::rect &r,
                         unsigned int shadow_type,
                         unsigned int thickness,
                         Widget resource_widget = nullptr) {
            linux::openmotif::theme_target target =
                linux::openmotif::theme_target_from(_g);
            if (!target.widget || !target.cache ||
                !target.cache->backbuffer ||
                !linux::openmotif::cached_display || !r.d.w || !r.d.h)
                return;

            Pixel top = 0;
            Pixel bottom = 0;
            XtVaGetValues(resource_widget ? resource_widget
                                          : target.widget,
                          XmNtopShadowColor,
                          &top,
                          XmNbottomShadowColor,
                          &bottom,
                          nullptr);
            GC top_gc = linux::openmotif::theme_gc(
                target, top, _g.get_clip());
            GC bottom_gc = linux::openmotif::theme_gc(
                target, bottom, _g.get_clip());
            XmeDrawShadows(linux::openmotif::cached_display,
                           target.cache->backbuffer,
                           top_gc,
                           bottom_gc,
                           r.p.x,
                           r.p.y,
                           r.d.w,
                           r.d.h,
                           thickness,
                           shadow_type);
            XFreeGC(linux::openmotif::cached_display, top_gc);
            XFreeGC(linux::openmotif::cached_display, bottom_gc);
        }

        mutable Widget _button_probe;
        mutable Widget _check_probe;
        mutable Widget _radio_probe;
        mutable Widget _list_probe;
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<motif_theme>(painter);
    }
} // namespace native
