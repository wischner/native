//
// Declares the GTK-native theme adapter shared by Wayland and
// elementary.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "theme_snapshot.h"
#include <optional>
namespace linux::wayland
{
    class gtk_theme final : public native::theme
    {
    public:
        explicit gtk_theme(native::gpx &graphics);
        metrics defaults() const override;
        palette native_palette() const override;
        theme &draw_button(const native::rect &,
                           const std::string &,
                           const state &) override;
        theme &draw_toolbar_button(const native::rect &,
                                   const std::string &,
                                   const state &) override;
        theme &draw_menu_bar(const native::rect &) override;
        theme &draw_menu_title(const native::rect &,
                               const std::string &,
                               const state &) override;
        theme &draw_menu_item(const native::rect &,
                              const std::string &,
                              const state &) override;
        theme &draw_popup_frame(const native::rect &) override;
        theme &draw_list_item(const native::rect &,
                              const std::string &,
                              const state &) override;
        theme &draw_check(const native::rect &,
                          const std::string &,
                          const state &) override;
        theme &draw_radio(const native::rect &,
                          const std::string &,
                          const state &) override;
        theme &draw_list(const native::rect &,
                         const std::vector<std::string> &,
                         int,
                         const state &) override;
        theme &draw_text_edit_frame(const native::rect &,
                                    const state &) override;
        theme &draw_surface(const native::rect &,
                            native::surface_kind,
                            const state &) override;
        theme &draw_selection(const native::rect &,
                              native::selection_shape,
                              const state &) override;
        theme &draw_focus(const native::rect &, const state &) override;
        theme &draw_disclosure(const native::rect &,
                               native::disclosure_state,
                               const state &) override;
        theme &draw_sort_indicator(const native::rect &,
                                   native::sort_indicator_state,
                                   const state &) override;
        theme &draw_caption_button(const native::rect &,
                                   native::caption_button_kind,
                                   const state &) override;
        theme &draw_separator(const native::rect &,
                              native::separator_orientation) override;
        theme &draw_scrollbar_part(const native::rect &,
                                   native::scrollbar_orientation,
                                   native::scrollbar_part,
                                   const state &) override;

    private:
        // Compose GTK's pixels through the destination graphics'
        // clip/offset.
        theme &paint(GtkWidget *,
                     const native::rect &,
                     const state &,
                     theme_part = theme_part::widget,
                     GtkWidget *target = nullptr,
                     const char *target_css = nullptr);
        std::shared_ptr<theme_snapshot> _snapshot;
        mutable std::optional<metrics> _metrics;
        mutable std::optional<palette> _palette;
    };
    // Construct the same GTK list composition used by stock list
    // controls.
    GtkWidget *theme_list(const std::vector<std::string> &items,
                          int selected);
    // Construct a real native radio, including its native radio-group
    // marker.
    GtkWidget *theme_radio(const std::string &text, bool selected);
    // Construct a selected native grid item for tile-selection
    // rendering.
    GtkWidget *theme_grid(bool selected);
    // Materialize a stock column-view row for native table metrics.
    GtkWidget *theme_table();
    // Construct actual column header/sort parts with GTK's native
    // sorter.
    GtkWidget *
    theme_column_header(native::sort_indicator_state direction =
                            native::sort_indicator_state::ascending);
    // Resolve the enabled placeholder command used by menu probes.
    void theme_menu_actions(GtkWidget *menu);
    // Preserve raw theme-label underscores through GTK mnemonic
    // parsing.
    std::string theme_menu_label(const std::string &text);
    // Build a real native menu bar with one optional title.
    GtkWidget *theme_menu(const std::string &title);
} // namespace linux::wayland
