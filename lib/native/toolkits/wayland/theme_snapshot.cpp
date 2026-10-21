//
// Reuses GTK snapshot/CSS rendering for native theme primitives.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "theme_snapshot.h"
#include <algorithm>
#include <stdexcept>
namespace
{
    // Map the widget hierarchy, not GtkWindow's desktop surface. The
    // normal GtkWindow realization still supplies GTK's root, settings
    // and renderer.
    void initialize_root(gpointer klass, gpointer) {
        auto *widget = GTK_WIDGET_CLASS(klass);
        auto *base =
            GTK_WIDGET_CLASS(g_type_class_peek(GTK_TYPE_WIDGET));
        widget->show = base->show;
        widget->hide = base->hide;
        widget->map = base->map;
        widget->unmap = base->unmap;
    }
    GType root_type() {
        static const GType type = [] {
            GTypeInfo info{};
            info.class_size = sizeof(GtkWindowClass);
            info.class_init = initialize_root;
            info.instance_size = sizeof(GtkWindow);
            return g_type_register_static(GTK_TYPE_WINDOW,
                                          "NativeThemeSnapshotRoot",
                                          &info,
                                          GTypeFlags(0));
        }();
        return type;
    }
    // GTK 4.14's supported CSS-part API is deprecated since 4.10, with
    // no equivalent public replacement for rendering a native
    // style-context part.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    void append_part(GtkSnapshot *snapshot,
                     GtkWidget *widget,
                     linux::wayland::theme_part part,
                     int w,
                     int h) {
        auto *style = gtk_widget_get_style_context(widget);
        using linux::wayland::theme_part;
        if (part == theme_part::background)
            gtk_snapshot_render_background(snapshot, style, 0, 0, w, h);
        else if (part == theme_part::frame ||
                 part == theme_part::label) {
            gtk_snapshot_render_background(snapshot, style, 0, 0, w, h);
            gtk_snapshot_render_frame(snapshot, style, 0, 0, w, h);
            if (part == theme_part::label) {
                std::vector<GtkWidget *> pending{widget};
                while (!pending.empty()) {
                    auto *current = pending.back();
                    pending.pop_back();
                    if (GTK_IS_LABEL(current)) {
                        auto *layout =
                            gtk_label_get_layout(GTK_LABEL(current));
                        int height = 0;
                        pango_layout_get_pixel_size(
                            layout, nullptr, &height);
                        GtkBorder padding{}, border{};
                        gtk_style_context_get_padding(style, &padding);
                        gtk_style_context_get_border(style, &border);
                        gtk_snapshot_render_layout(
                            snapshot,
                            gtk_widget_get_style_context(current),
                            padding.left + border.left,
                            (h - height) / 2,
                            layout);
                        break;
                    }
                    for (auto *child =
                             gtk_widget_get_first_child(current);
                         child;
                         child = gtk_widget_get_next_sibling(child))
                        pending.push_back(child);
                }
            }
        } else if (part == theme_part::focus)
            gtk_snapshot_render_focus(snapshot, style, 0, 0, w, h);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
} // namespace
namespace linux::wayland
{
    theme_snapshot::theme_snapshot() {
        initialize();
        _root = GTK_WIDGET(g_object_new(root_type(), nullptr));
        gtk_window_set_decorated(GTK_WINDOW(_root), false);
        configure_desktop(GTK_WINDOW(_root));
    }
    theme_snapshot::~theme_snapshot() {
        gtk_window_destroy(GTK_WINDOW(_root));
    }
    native::size theme_snapshot::measure(GtkWidget *widget,
                                         GtkWidget *target) {
        gtk_widget_unmap(_root);
        apply_state(_root, {});
        gtk_window_set_child(GTK_WINDOW(_root), widget);
        if (!target)
            target = widget;
        int width = 0, height = 0;
        gtk_widget_measure(target,
                           GTK_ORIENTATION_HORIZONTAL,
                           -1,
                           nullptr,
                           &width,
                           nullptr,
                           nullptr);
        gtk_widget_measure(target,
                           GTK_ORIENTATION_VERTICAL,
                           width,
                           nullptr,
                           &height,
                           nullptr,
                           nullptr);
        return native::size(width, height);
    }
    native::size theme_snapshot::measure_part(GtkWidget *widget,
                                              const char *name) {
        gtk_widget_unmap(_root);
        apply_state(_root, {});
        gtk_window_set_child(GTK_WINDOW(_root), widget);
        gtk_widget_set_visible(_root, true);
        gtk_widget_map(_root);
        gtk_widget_allocate(_root, 400, 400, -1, nullptr);
        auto *part = find_theme_widget(widget, name);
        if (!part)
            throw std::runtime_error(
                std::string("GTK theme part not found: ") + name);
        int w = 0, h = 0;
        gtk_widget_measure(part,
                           GTK_ORIENTATION_HORIZONTAL,
                           -1,
                           nullptr,
                           &w,
                           nullptr,
                           nullptr);
        gtk_widget_measure(part,
                           GTK_ORIENTATION_VERTICAL,
                           w,
                           nullptr,
                           &h,
                           nullptr,
                           nullptr);
        return native::size(w, h);
    }
    void
    theme_snapshot::apply_state(GtkWidget *widget,
                                const native::theme::state &state) {
        constexpr unsigned interaction =
            GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE |
            GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_INSENSITIVE |
            GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_FOCUS_VISIBLE |
            GTK_STATE_FLAG_FOCUS_WITHIN | GTK_STATE_FLAG_BACKDROP;
        unsigned flags = 0;
        if (state.hot)
            flags |= GTK_STATE_FLAG_PRELIGHT;
        if (state.pressed)
            flags |= GTK_STATE_FLAG_ACTIVE;
        if (state.selected)
            flags |= GTK_STATE_FLAG_SELECTED;
        if (state.disabled)
            flags |= GTK_STATE_FLAG_INSENSITIVE;
        if (state.focused)
            flags |= GTK_STATE_FLAG_FOCUSED |
                     GTK_STATE_FLAG_FOCUS_VISIBLE |
                     GTK_STATE_FLAG_FOCUS_WITHIN;
        if (!state.active)
            flags |= GTK_STATE_FLAG_BACKDROP;
        gtk_widget_unset_state_flags(widget,
                                     GtkStateFlags(interaction));
        gtk_widget_set_state_flags(widget, GtkStateFlags(flags), false);
        for (auto *child = gtk_widget_get_first_child(widget); child;
             child = gtk_widget_get_next_sibling(child)) {
            auto child_state = state;
            child_state.selected = gtk_widget_get_state_flags(child) &
                                   GTK_STATE_FLAG_SELECTED;
            apply_state(child, child_state);
        }
    }
    GskRenderNode *
    theme_snapshot::create_node(GtkWidget *widget,
                                native::size dimensions,
                                const native::theme::state &state,
                                theme_part part,
                                GtkWidget *target,
                                const char *target_css) {
        const int w = std::max(1, int(dimensions.w));
        const int h = std::max(1, int(dimensions.h));
        gtk_widget_unmap(_root);
        gtk_window_set_child(GTK_WINDOW(_root), nullptr);
        apply_state(_root, state);
        apply_state(widget, state);
        gtk_widget_set_sensitive(widget, !state.disabled);
        gtk_window_set_child(GTK_WINDOW(_root), widget);
        if (!GTK_IS_NATIVE(widget)) {
            gtk_widget_set_visible(_root, true);
            gtk_widget_map(_root);
            int minimum_w = 0, minimum_h = 0;
            gtk_widget_measure(widget,
                               GTK_ORIENTATION_HORIZONTAL,
                               -1,
                               &minimum_w,
                               nullptr,
                               nullptr,
                               nullptr);
            gtk_widget_measure(widget,
                               GTK_ORIENTATION_VERTICAL,
                               std::max(w, minimum_w),
                               &minimum_h,
                               nullptr,
                               nullptr,
                               nullptr);
            gtk_widget_allocate(_root,
                                std::max(w, minimum_w),
                                std::max(h, minimum_h),
                                -1,
                                nullptr);
        }
        if (target_css) {
            target = find_theme_widget(widget, target_css);
            if (!target)
                throw std::runtime_error(
                    std::string("GTK theme part not found: ") +
                    target_css);
        }
        if (!target)
            target = widget;
        auto *snapshot = gtk_snapshot_new();
        if (part == theme_part::widget) {
            graphene_rect_t bounds{};
            if (!gtk_widget_compute_bounds(
                    target, gtk_widget_get_parent(target), &bounds))
                bounds = graphene_rect_t{};
            GtkBorder margin{};
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_style_context_get_margin(
                gtk_widget_get_style_context(target), &margin);
            G_GNUC_END_IGNORE_DEPRECATIONS
            // Widget origins are CSS content origins, not outer control
            // origins. Preserve the complete native border, padding and
            // margin.
            const graphene_point_t translation{
                -bounds.origin.x + margin.left,
                -bounds.origin.y + margin.top};
            gtk_snapshot_translate(snapshot, &translation);
            gtk_widget_snapshot_child(
                gtk_widget_get_parent(target), target, snapshot);
        } else
            append_part(snapshot, target, part, w, h);
        auto *node = gtk_snapshot_free_to_node(snapshot);
        gtk_window_set_child(GTK_WINDOW(_root), nullptr);
        return node;
    }
    std::unique_ptr<native::img>
    theme_snapshot::render(GtkWidget *widget,
                           native::size dimensions,
                           const native::theme::state &state,
                           theme_part part,
                           GtkWidget *target,
                           const char *target_css) {
        const int w = std::max(1, int(dimensions.w));
        const int h = std::max(1, int(dimensions.h));
        auto *node = create_node(
            widget, dimensions, state, part, target, target_css);
        auto *surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        auto *context = cairo_create(surface);
        if (node) {
            gsk_render_node_draw(node, context);
            gsk_render_node_unref(node);
        }
        cairo_surface_flush(surface);
        auto image = std::make_unique<native::img>(w, h);
        const auto *pixels = reinterpret_cast<const std::uint32_t *>(
            cairo_image_surface_get_data(surface));
        const int stride = cairo_image_surface_get_stride(surface) / 4;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto value = pixels[y * stride + x];
                const unsigned alpha = value >> 24;
                image->pixels()[y * w + x] =
                    alpha ? native::rgba(
                                ((value >> 16) & 255) * 255 / alpha,
                                ((value >> 8) & 255) * 255 / alpha,
                                (value & 255) * 255 / alpha,
                                alpha)
                          : native::rgba(0, 0, 0, 0);
            }
        cairo_destroy(context);
        cairo_surface_destroy(surface);
        return image;
    }
} // namespace linux::wayland
