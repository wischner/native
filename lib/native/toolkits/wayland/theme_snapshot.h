//
// Snapshots native GTK widgets and CSS parts without showing a window.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "globals.h"
namespace linux::wayland
{
    // Locate a native part, including children materialized during
    // allocation.
    GtkWidget *find_theme_widget(GtkWidget *widget,
                                 const char *css_name);
    enum class theme_part
    {
        widget,
        background,
        frame,
        focus,
        label
    };

    class theme_snapshot
    {
    public:
        // Own a private GTK root whose native surface is never mapped.
        theme_snapshot();
        ~theme_snapshot();
        theme_snapshot(const theme_snapshot &) = delete;
        theme_snapshot &operator=(const theme_snapshot &) = delete;

        // Adopt a widget and measure its native preferred size.
        native::size measure(GtkWidget *widget,
                             GtkWidget *target = nullptr);
        // Materialize virtual/header children and measure their native
        // part.
        native::size measure_part(GtkWidget *widget,
                                  const char *css_name);
        // Produce native vector nodes for a window's device-scaled
        // Cairo target.
        GskRenderNode *create_node(GtkWidget *widget,
                                   native::size dimensions,
                                   const native::theme::state &state,
                                   theme_part part = theme_part::widget,
                                   GtkWidget *target = nullptr,
                                   const char *target_css = nullptr);
        // Adopt and render a widget, or one of its native CSS parts.
        std::unique_ptr<native::img>
        render(GtkWidget *widget,
               native::size dimensions,
               const native::theme::state &state,
               theme_part part = theme_part::widget,
               GtkWidget *target = nullptr,
               const char *target_css = nullptr);
        // Translate semantic state before the widget is rooted/mapped.
        static void apply_state(GtkWidget *widget,
                                const native::theme::state &state);

    private:
        GtkWidget *_root = nullptr;
    };
} // namespace linux::wayland
