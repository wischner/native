//
// Renders custom control primitives using actual GTK/Granite widgets
// and CSS.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "gtk_theme.h"
#include "../../gpx_wnd.h"
#include <stdexcept>
namespace native::detail
{
    // Access only portable geometry; Cairo/GSK remain in this toolkit.
    class gpx_wnd_access
    {
    public:
        static point offset(const gpx_wnd &graphics) {
            return graphics._offset;
        }
    };
} // namespace native::detail
namespace linux::wayland
{
    gtk_theme::gtk_theme(native::gpx &graphics)
        : theme(graphics) {
        if (auto *window = dynamic_cast<native::gpx_wnd *>(&graphics)) {
            auto &peer = linux::wayland::state(*window->window());
            if (!peer.theme_cache)
                peer.theme_cache = std::make_shared<theme_snapshot>();
            _snapshot = peer.theme_cache;
        } else
            _snapshot = std::make_shared<theme_snapshot>();
    }
    native::theme &gtk_theme::paint(GtkWidget *widget,
                                    const native::rect &bounds,
                                    const state &value,
                                    theme_part part,
                                    GtkWidget *target,
                                    const char *target_css) {
        // Adopt even empty-bound probes, without leaking floating
        // widgets.
        if (!bounds.w() || !bounds.h()) {
            g_object_ref_sink(widget);
            g_object_unref(widget);
            return *this;
        }
        if (auto *window = dynamic_cast<native::gpx_wnd *>(&_g)) {
            auto *cr = linux::wayland::state(*window->window()).context;
            if (!cr) {
                g_object_ref_sink(widget);
                g_object_unref(widget);
                throw std::runtime_error(
                    "GTK theme drawing requires a paint callback.");
            }
            auto *node = _snapshot->create_node(
                widget, bounds.d, value, part, target, target_css);
            if (node) {
                const auto offset =
                    native::detail::gpx_wnd_access::offset(*window);
                const auto clip = _g.get_clip();
                cairo_save(cr);
                cairo_translate(cr, offset.x, offset.y);
                cairo_rectangle(
                    cr, clip.x1(), clip.y1(), clip.w(), clip.h());
                cairo_clip(cr);
                cairo_translate(cr, bounds.p.x, bounds.p.y);
                cairo_rectangle(cr, 0, 0, bounds.w(), bounds.h());
                cairo_clip(cr);
                gsk_render_node_draw(node, cr);
                cairo_restore(cr);
                gsk_render_node_unref(node);
            }
            return *this;
        }
        auto image = _snapshot->render(
            widget, bounds.d, value, part, target, target_css);
        _g.draw_img(*image, bounds.p);
        return *this;
    }
    native::theme &gtk_theme::draw_button(const native::rect &bounds,
                                          const std::string &text,
                                          const state &value) {
        return paint(
            gtk_button_new_with_label(text.c_str()), bounds, value);
    }
    native::theme &
    gtk_theme::draw_toolbar_button(const native::rect &bounds,
                                   const std::string &text,
                                   const state &value) {
        auto *button = gtk_button_new_with_label(text.c_str());
        gtk_widget_add_css_class(button, "flat");
        return paint(button, bounds, value);
    }
    native::theme &gtk_theme::draw_check(const native::rect &bounds,
                                         const std::string &text,
                                         const state &value) {
        auto *check = gtk_check_button_new_with_label(text.c_str());
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
                                    value.selected);
        auto control = value;
        control.selected = false;
        return paint(check, bounds, control);
    }
    native::theme &gtk_theme::draw_radio(const native::rect &bounds,
                                         const std::string &text,
                                         const state &value) {
        auto control = value;
        control.selected = false;
        return paint(
            theme_radio(text, value.selected), bounds, control);
    }
    native::theme &
    gtk_theme::draw_list(const native::rect &bounds,
                         const std::vector<std::string> &items,
                         int selected,
                         const state &value) {
        auto *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      theme_list(items, selected));
        auto control = value;
        control.selected = false;
        return paint(scroll, bounds, control);
    }
    native::theme &gtk_theme::draw_list_item(const native::rect &bounds,
                                             const std::string &text,
                                             const state &value) {
        auto *list = theme_list({text}, value.selected ? 0 : -1);
        auto control = value;
        control.selected = false;
        return paint(
            list,
            bounds,
            control,
            theme_part::widget,
            gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), 0)
                ? GTK_WIDGET(gtk_list_box_get_row_at_index(
                      GTK_LIST_BOX(list), 0))
                : list);
    }
    native::theme &
    gtk_theme::draw_text_edit_frame(const native::rect &bounds,
                                    const state &value) {
        auto focus = value;
        focus.focused |= value.selected;
        focus.selected = false;
        return paint(new_entry(), bounds, focus);
    }
    native::theme &
    gtk_theme::draw_menu_bar(const native::rect &bounds) {
        return paint(theme_menu(""), bounds, {});
    }
    native::theme &
    gtk_theme::draw_menu_title(const native::rect &bounds,
                               const std::string &text,
                               const state &value) {
        auto *menu = theme_menu(text);
        auto control = value;
        control.hot |= control.selected;
        return paint(menu,
                     bounds,
                     control,
                     theme_part::widget,
                     find_theme_widget(menu, "item"));
    }
    native::theme &gtk_theme::draw_menu_item(const native::rect &bounds,
                                             const std::string &text,
                                             const state &value) {
        auto *menu = g_menu_new();
        g_menu_append(
            menu, theme_menu_label(text).c_str(), "native.placeholder");
        auto *popover =
            gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
        g_object_unref(menu);
        theme_menu_actions(popover);
        auto *item = find_theme_widget(popover, "modelbutton");
        // The model button's CSS owns its face, font and label padding.
        auto control = value;
        control.hot |= control.selected;
        return paint(popover,
                     bounds,
                     control,
                     theme_part::label,
                     item ? item : popover);
    }
    native::theme &
    gtk_theme::draw_popup_frame(const native::rect &bounds) {
        auto *popover = gtk_popover_new();
        gtk_widget_add_css_class(popover, "menu");
        return paint(popover,
                     bounds,
                     {},
                     theme_part::frame,
                     find_theme_widget(popover, "contents"));
    }
    native::theme &gtk_theme::draw_focus(const native::rect &bounds,
                                         const state &value) {
        if (!value.focused || value.disabled)
            return *this;
        return paint(
            gtk_button_new(), bounds, value, theme_part::focus);
    }
    native::theme &gtk_theme::draw_surface(const native::rect &bounds,
                                           native::surface_kind kind,
                                           const state &value) {
        using native::surface_kind;
        GtkWidget *widget = nullptr;
        theme_part part = theme_part::frame;
        if (kind == surface_kind::content)
            widget = gtk_text_view_new();
        else if (kind == surface_kind::inset ||
                 kind == surface_kind::status_part)
            widget = gtk_frame_new(nullptr);
        else if (kind == surface_kind::popup) {
            auto *popover = gtk_popover_new();
            gtk_widget_add_css_class(popover, "menu");
            return paint(popover,
                         bounds,
                         value,
                         part,
                         find_theme_widget(popover, "contents"));
        } else if (kind == surface_kind::table_header) {
            auto *view = theme_column_header();
            return paint(view,
                         bounds,
                         value,
                         theme_part::widget,
                         find_theme_widget(view, "header"));
        } else if (kind == surface_kind::header ||
                   kind == surface_kind::ruler) {
            widget = gtk_header_bar_new();
            gtk_header_bar_set_show_title_buttons(
                GTK_HEADER_BAR(widget), false);
        } else {
            widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_add_css_class(widget, "background");
        }
        return paint(widget, bounds, value, part);
    }
    native::theme &
    gtk_theme::draw_selection(const native::rect &bounds,
                              native::selection_shape shape,
                              const state &value) {
        if (shape == native::selection_shape::tile) {
            return paint(theme_grid(value.selected),
                         bounds,
                         value,
                         theme_part::frame,
                         nullptr,
                         "child");
        }
        auto *list = theme_list({""}, value.selected ? 0 : -1);
        return paint(list,
                     bounds,
                     value,
                     theme_part::frame,
                     GTK_WIDGET(gtk_list_box_get_row_at_index(
                         GTK_LIST_BOX(list), 0)));
    }
    native::theme &
    gtk_theme::draw_disclosure(const native::rect &bounds,
                               native::disclosure_state expanded,
                               const state &value) {
        auto *expander = gtk_expander_new(nullptr);
        gtk_expander_set_expanded(
            GTK_EXPANDER(expander),
            expanded == native::disclosure_state::expanded);
        return paint(expander,
                     bounds,
                     value,
                     theme_part::widget,
                     find_theme_widget(expander, "expander"));
    }
    native::theme &gtk_theme::draw_sort_indicator(
        const native::rect &bounds,
        native::sort_indicator_state direction,
        const state &value) {
        auto *view = theme_column_header(direction);
        return paint(view,
                     bounds,
                     value,
                     theme_part::widget,
                     find_theme_widget(view, "sort-indicator"));
    }
    native::theme &
    gtk_theme::draw_caption_button(const native::rect &bounds,
                                   native::caption_button_kind kind,
                                   const state &value) {
        if (kind == native::caption_button_kind::close) {
            auto *bar = gtk_header_bar_new();
            gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(bar),
                                                 ":close");
            return paint(bar,
                         bounds,
                         value,
                         theme_part::widget,
                         nullptr,
                         "button");
        }
        auto *button = gtk_button_new_from_icon_name(
            kind == native::caption_button_kind::close
                ? "window-close-symbolic"
            : kind == native::caption_button_kind::pin
                ? "view-pin-symbolic"
                : "view-restore-symbolic");
        gtk_widget_add_css_class(button, "flat");
        gtk_widget_add_css_class(button, "titlebutton");
        if (kind == native::caption_button_kind::close)
            gtk_widget_add_css_class(button, "close");
        return paint(button, bounds, value);
    }
    native::theme &gtk_theme::draw_separator(
        const native::rect &bounds,
        native::separator_orientation orientation) {
        return paint(
            gtk_separator_new(
                orientation == native::separator_orientation::horizontal
                    ? GTK_ORIENTATION_HORIZONTAL
                    : GTK_ORIENTATION_VERTICAL),
            bounds,
            {});
    }
    native::theme &gtk_theme::draw_scrollbar_part(
        const native::rect &bounds,
        native::scrollbar_orientation orientation,
        native::scrollbar_part part,
        const state &value) {
        const auto axis =
            orientation == native::scrollbar_orientation::horizontal
                ? GTK_ORIENTATION_HORIZONTAL
                : GTK_ORIENTATION_VERTICAL;
        // GTK4 scrollbars have no stepper buttons. Preserve that native
        // absence.
        if (part == native::scrollbar_part::decrement ||
            part == native::scrollbar_part::increment)
            return *this;
        auto *scrollbar = gtk_scrollbar_new(axis, nullptr);
        auto *target = find_theme_widget(
            scrollbar,
            part == native::scrollbar_part::thumb ? "slider"
                                                  : "trough");
        return paint(
            scrollbar, bounds, value, theme_part::frame, target);
    }
} // namespace linux::wayland
namespace native
{
    std::unique_ptr<theme> theme::create(gpx &graphics) {
        return std::make_unique<linux::wayland::gtk_theme>(graphics);
    }
} // namespace native
