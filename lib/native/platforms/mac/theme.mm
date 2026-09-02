//
// Implements the macOS theme with AppKit cells, colors, fonts, and
// drawing. Image targets use an equivalent backend-local AppKit-color
// emulation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include <native.h>
#include <native/theme.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace
{
    // Convert a dynamic AppKit color to portable straight RGBA.
    native::rgba from_native(NSColor *source) {
        NSColor *color = [source
            colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
        if (!color)
            return native::rgba(0, 0, 0, 255);
        return native::rgba(
            static_cast<std::uint8_t>([color redComponent] * 255.0),
            static_cast<std::uint8_t>([color greenComponent] * 255.0),
            static_cast<std::uint8_t>([color blueComponent] * 255.0),
            static_cast<std::uint8_t>([color alphaComponent] * 255.0));
    }

    // Recover the AppKit view for a window-backed graphics context.
    NSView *view_from(native::gpx &g) {
        auto *window_gpx = dynamic_cast<native::gpx_wnd *>(&g);
        if (!window_gpx)
            return nil;
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(
            window_gpx->window());
        return cache ? cache->view : nil;
    }

    // Paint in the active drawRect context with the portable clip.
    template <typename function_type>
    bool with_view(native::gpx &g, function_type &&function) {
        NSView *view = view_from(g);
        if (!view || ![NSGraphicsContext currentContext])
            return false;

        [NSGraphicsContext saveGraphicsState];
        const native::rect clip = g.get_clip();
        NSRectClip(NSMakeRect(clip.p.x, clip.p.y, clip.d.w, clip.d.h));
        function(view);
        [NSGraphicsContext restoreGraphicsState];
        return true;
    }

    // Restore every portable graphics property after theme drawing.
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

    class mac_theme final : public native::theme
    {
    public:
        explicit mac_theme(native::gpx &g)
            : theme(g) {}

        metrics defaults() const override {
            metrics m;
            const CGFloat height = [NSFont menuFontOfSize:0]
                                       .boundingRectForFont.size.height;
            m.menu_bar_height = 22;
            m.menu_item_height =
                std::max(20, static_cast<int>(height) + 6);
            m.header_height =
                std::max(20, static_cast<int>(std::ceil(height)) + 8);
            m.popup_width = 200;
            m.text_padding_x = 8;
            NSTableView *table =
                [[NSTableView alloc] initWithFrame:NSZeroRect];
            m.list_item_height = std::max(
                1, static_cast<int>(std::ceil([table rowHeight])));
            m.table_row_height = m.list_item_height;
            [table release];
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = from_native([NSColor controlColor]);
            p.button_border = from_native([NSColor gridColor]);
            p.button_highlight = from_native([NSColor separatorColor]);
            p.button_shadow = from_native([NSColor separatorColor]);
            p.button_text = from_native([NSColor controlTextColor]);
            p.button_disabled_text =
                from_native([NSColor disabledControlTextColor]);
            p.button_hot_bg = from_native(
                [NSColor unemphasizedSelectedContentBackgroundColor]);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg =
                from_native([NSColor selectedContentBackgroundColor]);
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg =
                from_native([NSColor windowBackgroundColor]);
            p.menu_bar_line_top = from_native([NSColor separatorColor]);
            p.menu_bar_line_bottom =
                from_native([NSColor separatorColor]);
            p.menu_text = from_native([NSColor controlTextColor]);
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg =
                from_native([NSColor selectedContentBackgroundColor]);
            p.menu_hot_text =
                from_native([NSColor selectedControlTextColor]);
            p.menu_popup_bg =
                from_native([NSColor windowBackgroundColor]);
            p.menu_popup_border = from_native([NSColor gridColor]);
            p.content_bg = from_native([NSColor controlBackgroundColor]);
            p.content_text = from_native([NSColor controlTextColor]);
            p.selection_bg =
                from_native([NSColor selectedContentBackgroundColor]);
            p.selection_text =
                from_native([NSColor selectedControlTextColor]);
            p.selection_inactive_bg = from_native(
                [NSColor unemphasizedSelectedContentBackgroundColor]);
            p.selection_inactive_text = from_native(
                [NSColor unemphasizedSelectedTextColor]);
            p.separator = from_native([NSColor separatorColor]);
            p.focus = from_native([NSColor keyboardFocusIndicatorColor]);
            return p;
        }

        theme &draw_button(const native::rect &r,
                           const std::string &text,
                           const state &s) override {
            const bool painted = with_view(_g, [&](NSView *view) {
                NSString *label =
                    [NSString stringWithUTF8String:text.c_str()];
                NSButtonCell *cell =
                    [[NSButtonCell alloc] initTextCell:label];
                [cell setBezelStyle:NSBezelStyleRounded];
                [cell setButtonType:NSButtonTypeMomentaryPushIn];
                [cell setControlSize:NSControlSizeRegular];
                [cell setFont:[NSFont systemFontOfSize:
                                      [NSFont
                                          systemFontSizeForControlSize:
                                              NSControlSizeRegular]]];
                [cell setEnabled:s.disabled ? NO : YES];
                [cell setHighlighted:s.pressed ? YES : NO];
                [cell
                    drawWithFrame:NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h)
                           inView:view];
                [cell release];
            });
            return painted ? *this : draw_button_fallback(r, text, s);
        }

        theme &draw_menu_bar(const native::rect &r) override {
            const bool painted = with_view(_g, [&](NSView *) {
                [[NSColor windowBackgroundColor] setFill];
                NSRectFill(NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h));
                [[NSColor separatorColor] setStroke];
                NSBezierPath *line = [NSBezierPath bezierPath];
                [line moveToPoint:NSMakePoint(r.p.x, r.y2() - 1)];
                [line lineToPoint:NSMakePoint(r.x2(), r.y2() - 1)];
                [line stroke];
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
            const bool painted = with_view(_g, [&](NSView *) {
                NSBezierPath *path = [NSBezierPath
                    bezierPathWithRoundedRect:NSMakeRect(r.p.x,
                                                         r.p.y,
                                                         r.d.w,
                                                         r.d.h)
                                      xRadius:4
                                      yRadius:4];
                [[NSColor windowBackgroundColor] setFill];
                [[NSColor gridColor] setStroke];
                [path fill];
                [path stroke];
            });
            return painted ? *this : draw_popup_fallback(r);
        }

        theme &draw_list_item(const native::rect &r,
                              const std::string &text,
                              const state &s) override {
            const bool painted = with_view(_g, [&](NSView *) {
                paint_list_entry(r, text, s);
            });
            return painted ? *this
                           : draw_list_entry_fallback(r, text, s);
        }

        theme &draw_check(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            return draw_selection(r, text, s, false);
        }

        theme &draw_radio(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            return draw_selection(r, text, s, true);
        }

        theme &draw_list(const native::rect &r,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &s) override {
            const bool painted = with_view(_g, [&](NSView *) {
                [[NSColor textBackgroundColor] setFill];
                NSRectFill(NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h));
                [[NSColor gridColor] setStroke];
                const int frame_height =
                    std::max(0, static_cast<int>(r.d.h) - 1);
                NSBezierPath *frame = [NSBezierPath bezierPathWithRect:
                    NSMakeRect(r.p.x + 0.5,
                               r.p.y + 0.5,
                               std::max(0, static_cast<int>(r.d.w) - 1),
                               frame_height)];
                [frame setLineWidth:1.0];
                [frame stroke];

                const native::rect content(r.p.x + 1,
                                           r.p.y + 1,
                                           r.d.w > 2 ? r.d.w - 2 : 0,
                                           r.d.h > 2 ? r.d.h - 2 : 0);
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
            const bool painted = with_view(_g, [&](NSView *view) {
                NSTextFieldCell *cell =
                    [[NSTextFieldCell alloc] initTextCell:@""];
                [cell setBezeled:YES];
                [cell setEditable:s.disabled ? NO : YES];
                [cell drawWithFrame:NSMakeRect(
                                        r.p.x, r.p.y, r.d.w, r.d.h)
                              inView:view];
                [cell release];
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
        int text_width(const std::string &text) const {
            NSString *label =
                [NSString stringWithUTF8String:text.c_str()];
            NSDictionary *attributes =
                @{NSFontAttributeName : [NSFont menuFontOfSize:0]};
            return static_cast<int>(
                [label sizeWithAttributes:attributes].width);
        }

        int text_height() const {
            return std::max(
                1,
                static_cast<int>([NSFont menuFontOfSize:0]
                                     .boundingRectForFont.size.height));
        }

        native::rect indicator_bounds(const native::rect &r) const {
            const int side =
                std::max(7, std::min(14, static_cast<int>(r.d.h) - 2));
            return native::rect(
                r.p.x + 2,
                r.p.y +
                    std::max(0, (static_cast<int>(r.d.h) - side) / 2),
                side,
                side);
        }

        theme &draw_selection(const native::rect &r,
                              const std::string &text,
                              const state &s,
                              bool radio) {
            const bool painted = with_view(_g, [&](NSView *view) {
                NSString *label =
                    [NSString stringWithUTF8String:text.c_str()];
                NSButtonCell *cell =
                    [[NSButtonCell alloc] initTextCell:label];
                [cell setButtonType:radio ? NSButtonTypeRadio
                                          : NSButtonTypeSwitch];
                [cell setState:s.selected ? NSControlStateValueOn
                                          : NSControlStateValueOff];
                [cell setEnabled:s.disabled ? NO : YES];
                [cell setHighlighted:s.pressed ? YES : NO];
                [cell
                    drawWithFrame:NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h)
                           inView:view];
                [cell release];
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
                    native::point(
                        indicator.x2() + 5,
                        r.p.y + std::max(0,
                                         (static_cast<int>(r.d.h) -
                                          text_height()) /
                                             2)));
            return *this;
        }

        void paint_list_entry(const native::rect &r,
                              const std::string &text,
                              const state &s) const {
            NSColor *inactive_selection = [NSColor
                unemphasizedSelectedContentBackgroundColor];
            NSColor *background =
                s.selected
                    ? (s.hot
                           ? [NSColor selectedContentBackgroundColor]
                           : inactive_selection)
                    : [NSColor textBackgroundColor];
            NSColor *foreground =
                s.disabled
                    ? [NSColor disabledControlTextColor]
                    : (s.selected && s.hot
                           ? [NSColor alternateSelectedControlTextColor]
                           : [NSColor controlTextColor]);
            [background setFill];
            NSRectFill(NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h));
            NSFont *font = [NSFont controlContentFontOfSize:0];
            NSDictionary *attributes = @{
                NSForegroundColorAttributeName : foreground,
                NSFontAttributeName : font
            };
            NSString *label =
                [NSString stringWithUTF8String:text.c_str()];
            const CGFloat height =
                [label sizeWithAttributes:attributes].height;
            [label drawAtPoint:NSMakePoint(
                                   r.p.x + 2,
                                   r.p.y +
                                       std::max(
                                           0.0,
                                           (static_cast<double>(r.d.h) -
                                            height) /
                                               2.0))
                withAttributes:attributes];
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
                    native::point(
                        r.p.x + defaults().text_padding_x,
                        r.p.y + std::max(0,
                                         (static_cast<int>(r.d.h) -
                                          text_height()) /
                                             2)));
            return *this;
        }

        theme &draw_list_fallback(const native::rect &r,
                                  const std::vector<std::string> &items,
                                  int selected_index,
                                  const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            _g.set_ink(p.menu_popup_border).draw_rect(r, false);
            if (r.d.w <= 2 || r.d.h <= 2)
                return *this;

            const native::rect content(
                r.p.x + 1, r.p.y + 1, r.d.w - 2, r.d.h - 2);
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
            _g.set_pen(1)
                .set_ink(s.pressed
                             ? p.button_pressed_bg
                             : (s.hot ? p.button_hot_bg : p.button_bg))
                .draw_rect(r, true);
            _g.set_ink(p.button_border).draw_rect(r, false);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text
                                  : p.button_text)
                .draw_text(
                    text,
                    native::point(
                        r.p.x + std::max(0,
                                         (static_cast<int>(r.d.w) -
                                          text_width(text)) /
                                             2),
                        r.p.y + std::max(0,
                                         (static_cast<int>(r.d.h) -
                                          text_height()) /
                                             2)));
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
            const bool painted = with_view(_g, [&](NSView *) {
                NSColor *background =
                    active ? [NSColor selectedContentBackgroundColor]
                           : (title ? [NSColor windowBackgroundColor]
                                    : [NSColor windowBackgroundColor]);
                [background setFill];
                NSRectFill(NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h));

                NSColor *foreground =
                    s.disabled
                        ? [NSColor disabledControlTextColor]
                        : (active ? [NSColor selectedControlTextColor]
                                  : [NSColor controlTextColor]);
                NSDictionary *attributes = @{
                    NSForegroundColorAttributeName : foreground,
                    NSFontAttributeName : [NSFont menuFontOfSize:0]
                };
                NSString *label =
                    [NSString stringWithUTF8String:text.c_str()];
                const CGFloat height =
                    [label sizeWithAttributes:attributes].height;
                [label drawAtPoint:NSMakePoint(
                                       r.p.x +
                                           defaults().text_padding_x,
                                       r.p.y + std::max(
                                                   0.0,
                                                   (static_cast<double>(
                                                        r.d.h) -
                                                    height) /
                                                       2.0))
                    withAttributes:attributes];
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
                    native::point(
                        r.p.x + defaults().text_padding_x,
                        r.p.y + std::max(0,
                                         (static_cast<int>(r.d.h) -
                                          text_height()) /
                                             2)));
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
        return std::make_unique<mac_theme>(painter);
    }
} // namespace native
