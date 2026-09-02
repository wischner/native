//
// Implements shared themed rendering and pointer routing for custom
// accordion and icon-view backends without imposing backend colors.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "collection_render.h"

#include <algorithm>
#include <limits>

#include <native/accordion.h>
#include <native/icon_view.h>
#include <native/tab_view.h>
#include <native/theme.h>
#include <native/tree_view.h>

namespace native::detail
{
    namespace
    {
        class offset_graphics final : public gpx
        {
        public:
            offset_graphics(gpx &target, point origin, size dimensions)
                : _target(target)
                , _origin(origin)
                , _clip(0, 0, dimensions.w, dimensions.h) {
                _ink = target.get_ink();
                _paper = target.get_paper();
                _thickness = target.get_pen();
                _font = &target.get_font();
            }

            gpx &set_clip(const rect &bounds) override {
                _clip = bounds;
                return *this;
            }

            rect get_clip() const override { return _clip; }

            gpx &clear(rgba color) override {
                apply();
                _target.set_ink(color).draw_rect(translated(_clip), true);
                return *this;
            }

            gpx &draw_line(point from, point to) override {
                apply();
                _target.draw_line(translated(from), translated(to));
                return *this;
            }

            gpx &draw_rect(rect bounds, bool filled) override {
                apply();
                _target.draw_rect(translated(bounds), filled);
                return *this;
            }

            gpx &draw_img(const img &source, point destination) override {
                apply();
                _target.draw_img(source, translated(destination));
                return *this;
            }

        protected:
            gpx &draw_native_text(const std::string &text,
                                  point position) override {
                apply();
                _target.draw_text(text, translated(position));
                return *this;
            }

        private:
            gpx &_target;
            point _origin;
            rect _clip;

            point translated(point value) const {
                return point(
                    static_cast<coord>(value.x + _origin.x),
                    static_cast<coord>(value.y + _origin.y));
            }

            rect translated(rect value) const {
                value.p = translated(value.p);
                return value;
            }

            void apply() {
                _target.set_ink(_ink)
                    .set_paper(_paper)
                    .set_pen(_thickness)
                    .set_font(get_font())
                    .set_clip(translated(_clip));
            }
        };

        rect fitted_bounds(const img &image, const rect &box) {
            if (!box.d.w || !box.d.h)
                return rect(box.p, size());
            const double scale = std::min(
                {1.0,
                 static_cast<double>(box.d.w) / image.w(),
                 static_cast<double>(box.d.h) / image.h()});
            const int width = std::max(
                1, static_cast<int>(image.w() * scale));
            const int height = std::max(
                1, static_cast<int>(image.h() * scale));
            return rect(
                static_cast<coord>(
                    box.p.x +
                    (static_cast<int>(box.d.w) - width) / 2),
                static_cast<coord>(
                    box.p.y +
                    (static_cast<int>(box.d.h) - height) / 2),
                static_cast<dim>(width),
                static_cast<dim>(height));
        }
    } // namespace

    void draw_accordion(accordion &control, gpx &graphics) {
        auto painter = theme::create(graphics);
        const theme::metrics values = painter->defaults();
        theme::state panel_state;
        control.draw_background(
            graphics,
            *painter,
            rect(0, 0,
                 control.get_dimensions().w,
                 control.get_dimensions().h),
            panel_state);
        for (std::size_t index = 0;
             index < control.get_item_count();
             ++index) {
            accordion_item &item = control.get_item(index);
            theme::state state;
            state.disabled = !item.get_enabled();
            state.focused = control.get_focused_index() ==
                            static_cast<int>(index);
            const rect header = control.get_header_bounds(index);
            control.draw_header_background(
                graphics, *painter, index, item, header, state);
            const int disclosure_size = std::min(
                values.disclosure_size,
                std::max(1, static_cast<int>(header.d.h) - 4));
            const rect disclosure(
                static_cast<coord>(header.p.x +
                                   values.header_padding_x),
                static_cast<coord>(
                    header.p.y +
                    (static_cast<int>(header.d.h) -
                     disclosure_size) /
                        2),
                static_cast<dim>(disclosure_size),
                static_cast<dim>(disclosure_size));
            control.draw_header_disclosure(
                graphics, *painter, index, item, disclosure, state);
            int text_x = disclosure.x2() + values.header_gap;
            if (const img *icon = item.get_icon()) {
                const int side = std::max(
                    1,
                    static_cast<int>(header.d.h) -
                        values.header_gap * 2);
                const rect icon_box(
                    static_cast<coord>(text_x),
                    static_cast<coord>(header.p.y +
                                       values.header_gap),
                    static_cast<dim>(side),
                    static_cast<dim>(side));
                control.draw_header_image(
                    graphics,
                    *painter,
                    index,
                    item,
                    fitted_bounds(*icon, icon_box),
                    state);
                text_x = icon_box.x2() + values.header_gap;
            }
            const int text_width = std::max(
                0, header.x2() - text_x - values.header_padding_x);
            control.draw_header_text(
                graphics,
                *painter,
                index,
                item,
                rect(static_cast<coord>(text_x),
                     header.p.y,
                     static_cast<dim>(text_width),
                     header.d.h),
                state);
            control.draw_header_border(
                graphics, *painter, index, item, header, state);
        }
    }

    void draw_accordion_at(accordion &control,
                           gpx &graphics,
                           point origin) {
        auto saved = graphics.save_state();
        offset_graphics translated(
            graphics, origin, control.get_dimensions());
        draw_accordion(control, translated);
    }

    bool handle_accordion_click(accordion &control,
                                point position) {
        for (std::size_t index = 0;
             index < control.get_item_count();
             ++index) {
            if (control.get_header_bounds(index).contains(position)) {
                control.on_native_toggle(index);
                return true;
            }
        }
        return false;
    }

    void draw_tab_view_at(tab_view &control,
                          gpx &graphics,
                          point origin) {
        auto saved = graphics.save_state();
        offset_graphics translated(
            graphics, origin, control.get_dimensions());
        const rect bounds(0, 0,
                          control.get_dimensions().w,
                          control.get_dimensions().h);
        control.on_native_paint(wnd_paint_event(bounds, translated));
    }

    void draw_icon_view(icon_view &control, gpx &graphics) {
        auto painter = theme::create(graphics);
        const theme::metrics values = painter->defaults();
        const size icon_size = control.get_icon_size();
        const rect viewport(0, 0,
                            control.get_dimensions().w,
                            control.get_dimensions().h);
        graphics.set_clip(graphics.get_clip().intersect(viewport));
        theme::state control_state;
        control_state.focused = control.get_focused();
        control.draw_background(
            graphics, *painter, viewport, control_state);
        for (std::size_t index = 0;
             index < control.get_items().size();
             ++index) {
            const rect item_bounds = control.get_item_bounds(index);
            if (item_bounds.y2() <= 0 ||
                item_bounds.p.y >= static_cast<int>(viewport.d.h)) {
                continue;
            }
            const icon_view_item &item = control.get_items()[index];
            const int padding = values.icon_view_padding_x;
            rect image_bounds;
            point label_position;
            if (control.get_label_mode() ==
                icon_view_label_mode::beside) {
                image_bounds = rect(
                    item_bounds.p.x + padding,
                    item_bounds.p.y + padding,
                    icon_size.w,
                    icon_size.h);
                label_position = point(
                    static_cast<coord>(image_bounds.x2() + padding),
                    static_cast<coord>(item_bounds.p.y + padding));
            } else {
                image_bounds = rect(
                    static_cast<coord>(
                        item_bounds.p.x +
                        std::max(0,
                                 (static_cast<int>(item_bounds.d.w) -
                                  static_cast<int>(icon_size.w)) /
                                     2)),
                    item_bounds.p.y + padding,
                    icon_size.w,
                    icon_size.h);
                label_position = point(
                    item_bounds.p.x + padding,
                    static_cast<coord>(image_bounds.y2() + padding));
            }
            theme::state state;
            state.selected = static_cast<int>(index) ==
                             control.get_selected_index();
            state.disabled = !item.enabled;
            state.focused = state.selected && control.get_focused();
            control.draw_item_background(
                graphics, *painter, index, item, item_bounds, state);
            if (item.image) {
                control.draw_item_image(
                    graphics,
                    *painter,
                    index,
                    item,
                    fitted_bounds(*item.image, image_bounds),
                    state);
            }
            if (control.get_label_mode() !=
                icon_view_label_mode::hidden) {
                const int label_width = std::max(
                    0, item_bounds.x2() - label_position.x - padding);
                const int label_height = std::max(
                    0, item_bounds.y2() - label_position.y - padding);
                control.draw_item_label(
                    graphics,
                    *painter,
                    index,
                    item,
                    rect(label_position,
                         size(static_cast<dim>(label_width),
                              static_cast<dim>(label_height))),
                    state);
            }
            control.draw_item_focus(
                graphics, *painter, index, item, item_bounds, state);
        }

        const size content = control.get_content_dimensions();
        if (content.h > viewport.d.h) {
            const int extent = std::max(1, values.scrollbar_extent);
            const rect track(
                static_cast<coord>(
                    std::max(0,
                             static_cast<int>(viewport.d.w) - extent)),
                0,
                static_cast<dim>(extent),
                viewport.d.h);
            const int thumb_height = std::max(
                values.scrollbar_min_thumb,
                static_cast<int>(viewport.d.h) * viewport.d.h /
                    std::max(1, static_cast<int>(content.h)));
            const int maximum_scroll =
                static_cast<int>(content.h) - viewport.d.h;
            const int thumb_y = maximum_scroll > 0
                                    ? control.get_scroll_offset() *
                                          (viewport.d.h - thumb_height) /
                                          maximum_scroll
                                    : 0;
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::vertical,
                track,
                rect(track.p.x,
                     static_cast<coord>(thumb_y),
                     track.d.w,
                     static_cast<dim>(thumb_height)),
                control_state);
        }
    }

    void draw_icon_view_at(icon_view &control,
                           gpx &graphics,
                           point origin) {
        auto saved = graphics.save_state();
        offset_graphics translated(
            graphics, origin, control.get_dimensions());
        draw_icon_view(control, translated);
    }

    bool handle_icon_view_click(icon_view &control,
                                point position) {
        const int index = control.item_at(position);
        if (index < 0)
            return false;
        control.on_native_selection(index);
        return true;
    }

    void draw_tree_view(tree_view &control, gpx &graphics) {
        auto painter = theme::create(graphics);
        const theme::metrics values = painter->defaults();
        // A child can be created before its modeless parent has a usable
        // graphics context.  Reapply an unoverridden native default here,
        // when the active toolkit is certain to be available.
        control.apply_theme_metrics(values);
        const rect viewport(0,
                            0,
                            control.get_dimensions().w,
                            control.get_dimensions().h);
        graphics.set_clip(graphics.get_clip().intersect(viewport));
        theme::state control_state;
        control_state.focused = control.get_focused();
        control.draw_background(
            graphics, *painter, viewport, control_state);

        for (std::size_t index = 0;
             index < control.get_visible_item_count();
             ++index) {
            const rect row = control.get_row_bounds(index);
            if (row.y2() <= 0 ||
                row.p.y >= static_cast<int>(viewport.d.h)) {
                continue;
            }
            const tree_view_visible_item visible =
                control.get_visible_item(index);
            const tree_view_item &item = control.get_item(visible.id);
            theme::state state;
            state.selected = visible.id ==
                             control.get_selected_item();
            state.disabled = !item.enabled;
            state.focused = state.selected && control.get_focused();
            control.draw_row_background(
                graphics, *painter, visible, item, row, state);

            const rect disclosure =
                control.get_disclosure_bounds(index);
            control.draw_connectors(
                graphics,
                *painter,
                visible,
                item,
                row,
                disclosure,
                state);
            control.draw_disclosure(
                graphics,
                *painter,
                visible,
                item,
                disclosure,
                state);

            int x = disclosure.x2() + control._item_gap;
            if (item.image) {
                const size icon_size = control.get_icon_size();
                const rect icon_box(
                    static_cast<coord>(x),
                    static_cast<coord>(
                        row.p.y +
                        (static_cast<int>(row.d.h) -
                         static_cast<int>(icon_size.h)) /
                            2),
                    icon_size.w,
                    icon_size.h);
                control.draw_item_image(
                    graphics,
                    *painter,
                    visible,
                    item,
                    fitted_bounds(*item.image, icon_box),
                    state);
                x = icon_box.x2() + control._item_gap;
            }
            const int width = std::max(
                0,
                static_cast<int>(row.d.w) - x -
                    control._horizontal_padding);
            control.draw_item_text(
                graphics,
                *painter,
                visible,
                item,
                rect(static_cast<coord>(x),
                     row.p.y,
                     static_cast<dim>(width),
                     row.d.h),
                state);
            control.draw_row_focus(
                graphics, *painter, visible, item, row, state);
        }

        const int row_height = control.get_visible_item_count() > 0
                                   ? control.get_row_bounds(0).d.h
                                   : values.list_item_height;
        const std::size_t visible_count =
            control.get_visible_item_count();
        const std::size_t maximum = static_cast<std::size_t>(
            std::numeric_limits<int>::max());
        const int content_height =
            visible_count > maximum /
                                static_cast<std::size_t>(
                                    std::max(1, row_height))
                ? std::numeric_limits<int>::max()
                : static_cast<int>(visible_count) * row_height;
        if (content_height > static_cast<int>(viewport.d.h)) {
            const int extent = std::max(1, values.scrollbar_extent);
            const rect track(
                static_cast<coord>(
                    std::max(0,
                             static_cast<int>(viewport.d.w) - extent)),
                0,
                static_cast<dim>(extent),
                viewport.d.h);
            const int thumb_height = std::max(
                values.scrollbar_min_thumb,
                static_cast<int>(viewport.d.h) * viewport.d.h /
                    std::max(1, content_height));
            const int maximum_scroll =
                content_height - viewport.d.h;
            const int thumb_y = maximum_scroll > 0
                                    ? control.get_scroll_offset() *
                                          (viewport.d.h - thumb_height) /
                                          maximum_scroll
                                    : 0;
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::vertical,
                track,
                rect(track.p.x,
                     static_cast<coord>(thumb_y),
                     track.d.w,
                     static_cast<dim>(thumb_height)),
                control_state);
        }
    }

    void draw_tree_view_at(tree_view &control,
                           gpx &graphics,
                           point origin) {
        auto saved = graphics.save_state();
        offset_graphics translated(
            graphics, origin, control.get_dimensions());
        draw_tree_view(control, translated);
    }

    bool handle_tree_view_click(tree_view &control,
                                point position) {
        const tree_view_hit hit = control.hit_test(position);
        if (hit.id == invalid_tree_item_id)
            return false;
        if (hit.part == tree_view_hit_part::disclosure) {
            control.on_native_expansion(
                hit.id, !control.get_expanded(hit.id));
        } else {
            control.on_native_selection(hit.id);
        }
        return true;
    }
} // namespace native::detail
