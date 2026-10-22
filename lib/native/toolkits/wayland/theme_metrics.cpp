//
// Measures actual GTK controls and reads the installed native CSS
// palette.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "gtk_theme.h"
#include <algorithm>
namespace
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    native::rgba
    color(GtkWidget *widget, const char *name, native::rgba fallback) {
        GdkRGBA value;
        if (!gtk_style_context_lookup_color(
                gtk_widget_get_style_context(widget), name, &value))
            return fallback;
        return native::rgba(value.red * 255,
                            value.green * 255,
                            value.blue * 255,
                            value.alpha * 255);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
} // namespace
namespace linux::wayland
{
    native::theme::metrics gtk_theme::defaults() const {
        if (_metrics) return *_metrics;
        metrics result;
        result.button_height =
            _snapshot->measure(gtk_button_new_with_label("M")).h;
        result.text_edit_height = _snapshot->measure(new_entry()).h;
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        result.text_edit_height = std::max({result.text_edit_height,
            result.button_height,
            int(_snapshot->measure(gtk_drop_down_new(nullptr, nullptr)).h),
            int(_snapshot->measure(gtk_combo_box_text_new_with_entry()).h),
            int(_snapshot->measure(gtk_spin_button_new_with_range(0, 59, 1)).h)});
        G_GNUC_END_IGNORE_DEPRECATIONS
        result.check_height =
            _snapshot->measure(gtk_check_button_new_with_label("M")).h;
        result.radio_height =
            _snapshot->measure(theme_radio("M", false)).h;
        result.menu_bar_height = _snapshot->measure(theme_menu("M")).h;
        auto *model = g_menu_new();
        g_menu_append(model, "M", "native.placeholder");
        auto *menu =
            gtk_popover_menu_new_from_model(G_MENU_MODEL(model));
        g_object_unref(model);
        theme_menu_actions(menu);
        result.menu_item_height =
            _snapshot
                ->measure(menu, find_theme_widget(menu, "modelbutton"))
                .h;
        result.list_item_height =
            _snapshot->measure(theme_list({"M"}, -1)).h;
        result.table_row_height =
            _snapshot->measure_part(theme_table(), "row").h;
        result.table_outer_border_extent = 0;
        auto *notebook = gtk_notebook_new();
        gtk_notebook_append_page(
            GTK_NOTEBOOK(notebook),
            gtk_box_new(GTK_ORIENTATION_VERTICAL, 0),
            gtk_label_new("M"));
        result.tab_height = _snapshot->measure(notebook).h;
        auto *header = theme_column_header();
        result.sort_indicator_size =
            _snapshot
                ->measure(header,
                          find_theme_widget(header, "sort-indicator"))
                .w;
        header = theme_column_header();
        result.header_height =
            _snapshot
                ->measure(header, find_theme_widget(header, "header"))
                .h;
        auto *expander = gtk_expander_new(nullptr);
        result.disclosure_size =
            _snapshot
                ->measure(expander,
                          find_theme_widget(expander, "expander"))
                .w;
        result.scrollbar_extent =
            std::max<int>(1,
                          _snapshot
                              ->measure(gtk_scrollbar_new(
                                  GTK_ORIENTATION_VERTICAL, nullptr))
                              .w);
        result.separator_extent = std::max<int>(
            1,
            _snapshot
                ->measure(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL))
                .h);
        auto *caption = gtk_header_bar_new();
        gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(caption),
                                             ":close");
        const auto button = _snapshot->measure_part(caption, "button");
        result.caption_button_size = std::max(button.w, button.h);
        auto *scroll =
            gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, nullptr);
        result.scrollbar_min_thumb =
            _snapshot->measure_part(scroll, "slider").h;
        result.status_bar_height = result.list_item_height;
        result.ruler_extent = result.header_height;
        _metrics = result;
        return result;
    }
    native::theme::palette gtk_theme::native_palette() const {
        if (_palette) return *_palette;
        using native::rgba;
        palette p{};
        auto *widget = gtk_label_new(nullptr);
        g_object_ref_sink(widget);
        const auto background =
            color(widget, "theme_bg_color", {245, 245, 245, 255});
        const auto base =
            color(widget, "theme_base_color", {255, 255, 255, 255});
        const auto border =
            color(widget, "borders", {190, 190, 190, 255});
        const auto selection =
            color(widget, "selected_bg_color", {52, 132, 228, 255});
        const auto selected_text =
            color(widget, "selected_fg_color", {255, 255, 255, 255});
        g_object_unref(widget);
        auto face = [&](const state &value) {
            auto pixels =
                _snapshot->render(gtk_button_new_with_label(""),
                                  native::size(80, 40),
                                  value);
            return pixels->pixels()[20 * 80 + 40];
        };
        auto text = [&](const state &value,
                        GtkWidget *widget = nullptr) {
            auto *button =
                widget ? widget : gtk_button_new_with_label("");
            theme_snapshot::apply_state(button, value);
            gtk_widget_set_sensitive(button, !value.disabled);
            _snapshot->measure(button);
            GdkRGBA value_color{};
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_style_context_get_color(
                gtk_widget_get_style_context(button), &value_color);
            G_GNUC_END_IGNORE_DEPRECATIONS
            return rgba(value_color.red * 255,
                        value_color.green * 255,
                        value_color.blue * 255,
                        value_color.alpha * 255);
        };
        state hot, pressed, disabled;
        hot.hot = true;
        pressed.pressed = true;
        disabled.disabled = true;
        p.button_bg = face({});
        p.button_border = border;
        p.button_highlight = base;
        p.button_shadow = border;
        p.button_text = text({});
        p.button_disabled_text = text(disabled);
        p.button_hot_bg = face(hot);
        p.button_hot_text = text(hot);
        p.button_pressed_bg = face(pressed);
        p.button_pressed_text = text(pressed);
        p.menu_bar_bg = background;
        p.menu_bar_line_top = background;
        p.menu_bar_line_bottom = border;
        p.menu_text = text({}, gtk_label_new(""));
        p.menu_disabled_text = p.button_disabled_text;
        p.menu_hot_bg = selection;
        p.menu_hot_text = selected_text;
        p.menu_popup_bg = base;
        p.menu_popup_border = border;
        p.content_bg = base;
        p.content_text = text({}, gtk_text_view_new());
        p.selection_bg = selection;
        p.selection_text = selected_text;
        p.selection_inactive_bg = background;
        p.selection_inactive_text = p.content_text;
        p.separator = border;
        p.focus = selection;
        _palette = p;
        return p;
    }
} // namespace linux::wayland
