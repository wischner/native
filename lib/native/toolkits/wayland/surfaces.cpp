//
// Hosts structural containers in GTK and paints portable canvas
// content.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"

namespace
{
    struct splitter_style
    {
        GdkDisplay *display;
        GtkCssProvider *provider;
        ~splitter_style() {
            gtk_style_context_remove_provider_for_display(
                display, GTK_STYLE_PROVIDER(provider));
            g_object_unref(provider);
            g_object_unref(display);
        }
    };

    // Some desktop themes give separators negative margins, leaving a
    // one-pixel allocation even when GTK's wide handle is enabled.
    void size_splitter(native::split_view &owner) {
        auto *widget = linux::wayland::state(owner).widget;
        const auto name =
            "native-split-" +
            std::to_string(reinterpret_cast<std::uintptr_t>(widget));
        gtk_widget_set_name(widget, name.c_str());
        auto *provider = gtk_css_provider_new();
        const auto size = std::to_string(owner.get_splitter_size());
        const auto css =
            "#" + name + " > separator { margin: 0; } #" + name +
            ".horizontal > separator { min-width: " + size +
            "px; min-height: 0; } #" + name +
            ".vertical > separator { min-height: " + size +
            "px; min-width: 0; }";
        gtk_css_provider_load_from_string(provider, css.c_str());
        auto *display = gtk_widget_get_display(widget);
        auto *style = new splitter_style{
            GDK_DISPLAY(g_object_ref(display)), provider};
        g_object_set_data_full(
            G_OBJECT(widget),
            "native-split-style",
            style,
            +[](gpointer value) {
                delete static_cast<splitter_style *>(value);
            });
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_paned_set_wide_handle(GTK_PANED(widget),
                                  owner.get_splitter_size() > 4);
    }

    struct splitter_position
    {
        native::split_view &owner;
        native::detail::window_guard guard;
    };
} // namespace
namespace native
{
    void panel::create_native() {
        linux::wayland::create_surface(*this);
    }
    void panel::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void panel::destroy_native() {
        destroy_children();
        linux::wayland::destroy(*this);
    }
    void canvas::create_native() {
        linux::wayland::create_surface(*this);
    }
    void canvas::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void canvas::destroy_native() {
        destroy_children();
        linux::wayland::destroy(*this);
    }
    void split_view::create_native() {
        auto *paned = gtk_paned_new(
            get_orientation() == split_orientation::horizontal
                ? GTK_ORIENTATION_HORIZONTAL
                : GTK_ORIENTATION_VERTICAL);
        linux::wayland::attach(*this, paned, paned);
        size_splitter(*this);
        gtk_paned_set_shrink_start_child(GTK_PANED(paned), true);
        gtk_paned_set_shrink_end_child(GTK_PANED(paned), true);
        _content_hosts_are_panes = true;
        refresh_contents();
        apply_ratio();
        g_signal_connect(
            paned,
            "notify::position",
            G_CALLBACK(+[](GObject *w, GParamSpec *, gpointer data) {
                auto &owner = *static_cast<split_view *>(data);
                if (linux::wayland::state(owner).updating)
                    return;
                const int extent =
                    owner.get_orientation() ==
                            split_orientation::horizontal
                        ? gtk_widget_get_width(GTK_WIDGET(w))
                        : gtk_widget_get_height(GTK_WIDGET(w));
                if (extent > owner.get_splitter_size()) {
                    // Position notifications also run inside GTK
                    // allocation. Coalesce portable child layout until
                    // that allocation ends.
                    auto &peer = linux::wayland::state(owner);
                    peer.position_ratio =
                        float(gtk_paned_get_position(GTK_PANED(w))) /
                        (extent - owner.get_splitter_size());
                    if (!peer.position_source)
                        peer.position_source = g_idle_add_full(
                            G_PRIORITY_DEFAULT_IDLE,
                            +[](gpointer data) -> gboolean {
                                auto &pending =
                                    *static_cast<splitter_position *>(
                                        data);
                                if (!pending.guard.alive())
                                    return G_SOURCE_REMOVE;
                                auto &peer = linux::wayland::state(
                                    pending.owner);
                                peer.position_source = 0;
                                peer.updating = true;
                                pending.owner.on_native_ratio(
                                    peer.position_ratio);
                                if (pending.guard.alive())
                                    linux::wayland::state(pending.owner)
                                        .updating = false;
                                return G_SOURCE_REMOVE;
                            },
                            new splitter_position{
                                owner, detail::window_guard(owner)},
                            +[](gpointer data) {
                                delete static_cast<splitter_position *>(
                                    data);
                            });
                }
            }),
            this);
    }
    void split_view::apply_orientation() {
        gtk_orientable_set_orientation(
            GTK_ORIENTABLE(linux::wayland::state(*this).widget),
            get_orientation() == split_orientation::horizontal
                ? GTK_ORIENTATION_HORIZONTAL
                : GTK_ORIENTATION_VERTICAL);
        apply_ratio();
    }
    void split_view::apply_ratio() {
        auto &peer = linux::wayland::state(*this);
        if (peer.updating) return;
        peer.updating = true;
        const int extent =
            get_orientation() == split_orientation::horizontal
                ? get_dimensions().w
                : get_dimensions().h;
        gtk_paned_set_position(GTK_PANED(peer.widget),
                               std::max(0, extent - int(get_splitter_size())) * get_ratio());
        peer.updating = false;
    }
    void split_view::apply_minimums() {
        apply_ratio();
    }
    void split_view::apply_splitter_size() {
        size_splitter(*this);
        apply_ratio();
    }
    void split_view::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void split_view::destroy_native() {
        auto &peer = linux::wayland::state(*this);
        peer.updating = true;
        g_signal_handlers_disconnect_by_data(peer.widget, this);
        destroy_children();
        linux::wayland::destroy(*this);
    }

    void tab_view::create_native() {
        auto *notebook = gtk_notebook_new();
        linux::wayland::attach(*this, notebook, notebook);
        configure_page_host(true, true);
        refresh();
        g_signal_connect(notebook,
                         "switch-page",
                         G_CALLBACK(+[](GtkNotebook *,
                                        GtkWidget *,
                                        guint page,
                                        gpointer data) {
                             auto &owner =
                                 *static_cast<tab_view *>(data);
                             if (!linux::wayland::state(owner).updating)
                                 owner.on_native_selection(page);
                         }),
                         this);
    }
    void tab_view::apply_items() {
        auto &peer = linux::wayland::state(*this);
        auto *notebook = GTK_NOTEBOOK(peer.widget);
        peer.updating = true;
        std::vector<GtkWidget *> contents(get_item_count(), nullptr);
        for (std::size_t i = 0; i < get_item_count(); ++i) {
            auto &content = get_item(i).get_content();
            if (!content.get_created())
                continue;
            contents[i] = linux::wayland::state(content).widget;
            g_object_ref(contents[i]);
            if (auto *parent = gtk_widget_get_parent(contents[i]))
                gtk_overlay_set_child(GTK_OVERLAY(parent), nullptr);
        }
        while (gtk_notebook_get_n_pages(notebook))
            gtk_notebook_remove_page(notebook, 0);
        peer.pages.clear();
        const GtkPositionType positions[] = {
            GTK_POS_TOP, GTK_POS_BOTTOM, GTK_POS_LEFT, GTK_POS_RIGHT};
        gtk_notebook_set_tab_pos(
            notebook, positions[static_cast<int>(get_tab_placement())]);
        gtk_notebook_set_show_border(notebook,
                                     get_page_frame_visible());
        for (std::size_t i = 0; i < get_item_count(); ++i) {
            auto *page = gtk_overlay_new();
            peer.pages.push_back(page);
            auto *label =
                gtk_label_new(get_item(i).get_title().c_str());
            gtk_widget_set_margin_start(label, 6);
            gtk_widget_set_margin_end(label, 6);
            gtk_widget_set_margin_top(label, 2);
            gtk_widget_set_margin_bottom(label, 2);
            gtk_widget_set_sensitive(label, get_item(i).get_enabled());
            gtk_notebook_append_page(notebook, page, label);
            if (contents[i]) {
                gtk_overlay_set_child(GTK_OVERLAY(page), contents[i]);
                g_object_unref(contents[i]);
            }
        }
        peer.updating = false;
    }
    void tab_view::apply_selected_index() {
        auto &peer = linux::wayland::state(*this);
        peer.updating = true;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(peer.widget),
                                      get_selected_index());
        peer.updating = false;
    }
    void tab_view::show_native() {
        apply_selected_index();
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void tab_view::destroy_native() {
        destroy_children();
        linux::wayland::destroy(*this);
    }

    void accordion::create_native() {
        auto *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        linux::wayland::attach(*this, box, box);
        synchronize_theme_metrics();
        apply_items();
        refresh();
    }
    void accordion::apply_items() {
        auto &peer = linux::wayland::state(*this);
        peer.updating = true;
        // Keep borrowed live pages alive while section positions
        // change.
        std::vector<GtkWidget *> pages(get_item_count(), nullptr);
        for (std::size_t i = 0; i < get_item_count(); ++i) {
            auto &content = get_item(i).get_content();
            if (!content.get_created())
                continue;
            pages[i] = linux::wayland::state(content).widget;
            g_object_ref(pages[i]);
            auto *expander =
                gtk_widget_get_ancestor(pages[i], GTK_TYPE_EXPANDER);
            if (expander)
                gtk_expander_set_child(GTK_EXPANDER(expander), nullptr);
        }
        while (auto *child = gtk_widget_get_first_child(peer.widget))
            gtk_box_remove(GTK_BOX(peer.widget), child);
        for (std::size_t i = 0; i < get_item_count(); ++i) {
            auto &item = get_item(i);
            auto *expander = gtk_expander_new(item.get_title().c_str());
            gtk_expander_set_expanded(GTK_EXPANDER(expander),
                                      item.get_expanded());
            gtk_widget_set_sensitive(expander, item.get_enabled());
            g_object_set_data(G_OBJECT(expander),
                              "native-index",
                              GUINT_TO_POINTER(i));
            if (pages[i]) {
                gtk_expander_set_child(GTK_EXPANDER(expander),
                                       pages[i]);
                g_object_unref(pages[i]);
            }
            gtk_box_append(GTK_BOX(peer.widget), expander);
            g_signal_connect(
                expander,
                "notify::expanded",
                G_CALLBACK(+[](GObject *expander,
                               GParamSpec *,
                               gpointer data) {
                    auto &owner = *static_cast<accordion *>(data);
                    if (linux::wayland::state(owner).updating)
                        return;
                    const auto index = GPOINTER_TO_UINT(
                        g_object_get_data(expander, "native-index"));
                    owner.on_native_toggle(index);
                }),
                this);
        }
        peer.updating = false;
    }
    void accordion::show_native() {
        gtk_widget_set_visible(linux::wayland::state(*this).widget,
                               true);
    }
    void accordion::destroy_native() {
        destroy_children();
        linux::wayland::destroy(*this);
    }
} // namespace native
