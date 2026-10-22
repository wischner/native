//
// Owns shared GTK peers, Wayland initialization and widget hierarchy.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../gpx_wnd.h"
#include "../../collection_render.h"
#include "../../table_render.h"
#include "../../code_render.h"
#include <stdexcept>

namespace linux::wayland
{
    native::bindings<GtkWidget *, native::wnd *> wnd_bindings;
    bool exit_requested = false;
    GtkApplication *application = nullptr;
    widget_state::~widget_state() {
        if (position_source)
            g_source_remove(position_source);
        if (widget)
            g_object_unref(widget);
    }
    void initialize() {
        static bool initialized = false;
        if (initialized)
            return;
        gdk_set_allowed_backends("wayland");
        if (!gtk_init_check() ||
            !GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()))
            throw std::runtime_error(
                "Native requires a Wayland compositor.");
        application = gtk_application_new("com.wischner.native",
                                          G_APPLICATION_NON_UNIQUE);
        GError *error = nullptr;
        if (!g_application_register(
                G_APPLICATION(application), nullptr, &error)) {
            std::string message =
                error ? error->message
                      : "GTK application registration failed.";
            if (error)
                g_error_free(error);
            g_clear_object(&application);
            throw std::runtime_error(message);
        }
        g_application_set_default(G_APPLICATION(application));
        initialized = true;
    }
    widget_state &state(const native::wnd &owner) {
        auto *peer = native::detail::peer_state<widget_state>(owner);
        if (!peer)
            throw std::runtime_error("Missing Wayland widget peer.");
        return *peer;
    }
    void geometry(native::wnd &owner) {
        auto *resource =
            native::detail::peer_state<widget_state>(owner);
        if (!resource || !resource->widget)
            return;
        auto &peer = *resource;
        auto bounds = owner.get_bounds();
        if (GTK_IS_WINDOW(peer.widget)) {
            int height = bounds.h();
            for (auto *chrome : {gtk_window_get_titlebar(
                                    GTK_WINDOW(peer.widget)),
                                peer.menu}) {
                if (chrome && gtk_widget_get_visible(chrome)) {
                    int natural = 0;
                    gtk_widget_measure(chrome,
                                       GTK_ORIENTATION_VERTICAL,
                                       bounds.w(),
                                       nullptr,
                                       &natural,
                                       nullptr,
                                       nullptr);
                    height += natural;
                }
            }
            gtk_window_set_default_size(
                GTK_WINDOW(peer.widget), bounds.w(), height);
        } else {
            int minimum_width = bounds.w();
            int minimum_height = bounds.h();
            if (auto *split = dynamic_cast<native::split_view *>(
                    owner.get_parent())) {
                const int minimum = &split->get_first() == &owner
                                        ? split->get_first_minimum()
                                        : split->get_second_minimum();
                minimum_width =
                    split->get_orientation() ==
                            native::split_orientation::horizontal
                        ? minimum
                        : -1;
                minimum_height =
                    split->get_orientation() ==
                            native::split_orientation::vertical
                        ? minimum
                        : -1;
            } else if (dynamic_cast<native::tab_view *>(
                           owner.get_parent())) {
                minimum_width = -1;
                minimum_height = -1;
            }
            gtk_widget_set_size_request(
                peer.widget, minimum_width, minimum_height);
            auto *parent = gtk_widget_get_parent(peer.widget);
            if (GTK_IS_FIXED(parent))
                gtk_fixed_move(GTK_FIXED(parent),
                               peer.widget,
                               bounds.x1(),
                               bounds.y1());
            if (peer.layout_children) peer.layout_children();
        }
    }
    namespace
    {
        void place(native::wnd &owner, GtkWidget *widget) {
            auto *parent = owner.get_parent();
            auto *host = state(*parent).content;
            if (auto *split =
                    dynamic_cast<native::split_view *>(parent)) {
                if (&split->get_first() == &owner)
                    gtk_paned_set_start_child(GTK_PANED(host), widget);
                else
                    gtk_paned_set_end_child(GTK_PANED(host), widget);
            } else if (auto *tabs =
                           dynamic_cast<native::tab_view *>(parent)) {
                for (std::size_t i = 0; i < tabs->get_item_count(); ++i)
                    if (&tabs->get_item(i).get_content() == &owner)
                        gtk_overlay_set_child(
                            GTK_OVERLAY(state(*parent).pages.at(i)),
                            widget);
            } else if (auto *sections =
                           dynamic_cast<native::accordion *>(parent)) {
                auto *expander = gtk_widget_get_first_child(host);
                for (std::size_t i = 0; i < sections->get_item_count();
                     ++i) {
                    if (&sections->get_item(i).get_content() ==
                        &owner) {
                        gtk_expander_set_child(GTK_EXPANDER(expander),
                                               widget);
                        break;
                    }
                    expander = gtk_widget_get_next_sibling(expander);
                }
            } else
                gtk_fixed_put(GTK_FIXED(host),
                              widget,
                              owner.get_position().x,
                              owner.get_position().y);
        }
    } // namespace
    void
    attach(native::wnd &owner, GtkWidget *widget, GtkWidget *content) {
        auto *parent = owner.get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "Wayland controls require a created parent.");
        auto *host = state(*parent).content;
        if (!GTK_IS_FIXED(host) && !GTK_IS_PANED(host) &&
            !GTK_IS_NOTEBOOK(host) && !GTK_IS_BOX(host))
            throw std::runtime_error(
                "Wayland parent has no child host.");
        auto *peer = new widget_state;
        peer->widget = GTK_WIDGET(g_object_ref_sink(widget));
        peer->content = content;
        native::detail::assign_peer_state(owner, peer);
        native::detail::wnd_peer_access::assign_content(owner, content);
        wnd_bindings.register_pair(widget, &owner);
        gtk_widget_set_visible(widget, false);
        place(owner, widget);
        geometry(owner);
        install_keyboard(owner, widget);
    }
    namespace
    {
        void remove(GtkWidget *widget) {
            if (auto *parent = gtk_widget_get_parent(widget)) {
                // Unparenting can synchronously close the containing window.
                // Keep the native container alive until GTK finishes removal.
                g_object_ref(parent);
                if (GTK_IS_FIXED(parent))
                    gtk_fixed_remove(GTK_FIXED(parent), widget);
                else if (GTK_IS_PANED(parent)) {
                    if (gtk_paned_get_start_child(GTK_PANED(parent)) ==
                        widget)
                        gtk_paned_set_start_child(GTK_PANED(parent),
                                                  nullptr);
                    else
                        gtk_paned_set_end_child(GTK_PANED(parent),
                                                nullptr);
                } else if (GTK_IS_OVERLAY(parent)) {
                    if (gtk_overlay_get_child(GTK_OVERLAY(parent)) ==
                        widget)
                        gtk_overlay_set_child(GTK_OVERLAY(parent),
                                              nullptr);
                    else
                        gtk_overlay_remove_overlay(GTK_OVERLAY(parent),
                                                   widget);
                } else if (auto *expander = gtk_widget_get_ancestor(
                               widget, GTK_TYPE_EXPANDER)) {
                    gtk_expander_set_child(GTK_EXPANDER(expander),
                                           nullptr);
                } else if (GTK_IS_NOTEBOOK(parent))
                    gtk_notebook_remove_page(
                        GTK_NOTEBOOK(parent),
                        gtk_notebook_page_num(GTK_NOTEBOOK(parent),
                                              widget));
                else
                    gtk_widget_unparent(widget);
                g_object_unref(parent);
            }
        }
    } // namespace
    void reparent(native::wnd &owner) {
        auto *peer = native::detail::peer_state<widget_state>(owner);
        if (!peer || !peer->widget)
            return;
        auto *widget = peer->widget;
        g_object_ref(widget);
        const native::detail::window_guard guard(owner);
        remove(widget);
        if (guard.alive()) {
            place(owner, widget);
            geometry(owner);
        }
        g_object_unref(widget);
    }
    namespace
    {
        struct button_style
        {
            GdkDisplay *display;
            GtkCssProvider *provider;
            native::border_sides sides;
            ~button_style() {
                gtk_style_context_remove_provider_for_display(
                    display, GTK_STYLE_PROVIDER(provider));
                g_object_unref(provider);
                g_object_unref(display);
            }
        };
    } // namespace
    void style_button_frame(native::button &owner,
                            native::border_sides sides) {
        auto *widget = state(owner).editor;
        auto *previous = static_cast<button_style *>(
            g_object_get_data(G_OBJECT(widget), "native-button-style"));
        if (previous && previous->sides == sides)
            return;
        if (sides == native::border_sides::all) {
            g_object_set_data(
                G_OBJECT(widget), "native-button-style", nullptr);
            return;
        }
        const auto name =
            "native-button-" +
            std::to_string(reinterpret_cast<std::uintptr_t>(widget));
        gtk_widget_set_name(widget, name.c_str());
        GdkRGBA color{0.75, 0.75, 0.75, 1};
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_style_context_lookup_color(
            gtk_widget_get_style_context(widget), "borders", &color);
        G_GNUC_END_IGNORE_DEPRECATIONS
        char *text = gdk_rgba_to_string(&color);
        const auto edge = [sides](native::border_sides side) {
            return native::has_border(sides, side) ? "1px " : "0px ";
        };
        const auto css = "#" + name +
                         " { box-shadow: none; outline-style: none; "
                         "border-style: solid; border-color: " +
                         text + "; border-width: " +
                         edge(native::border_sides::top) +
                         edge(native::border_sides::right) +
                         edge(native::border_sides::bottom) +
                         edge(native::border_sides::left) + "; }";
        g_free(text);
        auto *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(provider, css.c_str());
        auto *display = gtk_widget_get_display(widget);
        auto *style = new button_style{
            GDK_DISPLAY(g_object_ref(display)), provider, sides};
        g_object_set_data_full(
            G_OBJECT(widget),
            "native-button-style",
            style,
            +[](gpointer data) {
                delete static_cast<button_style *>(data);
            });
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    void destroy(native::wnd &owner) {
        auto *resource =
            native::detail::peer_state<widget_state>(owner);
        if (!resource || !resource->widget)
            return;
        auto &peer = *resource;
        auto *widget = peer.widget;
        peer.widget = nullptr;
        wnd_bindings.unregister_by_handle(widget);
        if (GTK_IS_WINDOW(widget))
            gtk_window_destroy(GTK_WINDOW(widget));
        else
            remove(widget);
        // The peer's reference survives any earlier parent destruction and
        // remains local across callbacks which may release the portable peer.
        g_object_unref(widget);
    }
    namespace
    {
        void draw(GtkDrawingArea *,
                  cairo_t *cr,
                  int w,
                  int h,
                  gpointer data) {
            auto &owner = *static_cast<native::wnd *>(data);
            auto &peer = state(owner);
            peer.context = cr;
            const native::detail::window_guard guard(owner);
            auto &graphics = owner.get_gpx();
            native::rect bounds(0, 0, w, h);
            graphics.set_clip(bounds);
            if (auto *icons = dynamic_cast<native::icon_view *>(&owner))
                native::detail::draw_icon_view(*icons, graphics);
            else if (auto *table =
                         dynamic_cast<native::table_view *>(&owner))
                native::detail::draw_table_view(*table, graphics);
            else if (auto *editor =
                         dynamic_cast<native::code_edit *>(&owner))
                native::draw_code_edit(
                    *editor, graphics, native::point());
            else if (auto *accordion =
                         dynamic_cast<native::accordion *>(&owner))
                native::detail::draw_accordion(*accordion, graphics);
            else
                owner.on_native_paint(
                    native::wnd_paint_event(bounds, graphics));
            if (guard.alive())
                state(owner).context = nullptr;
        }
    } // namespace
    void configure_drawing(native::wnd &owner) {
        auto *drawing = state(owner).drawing;
        g_signal_connect(
            drawing,
            "resize",
            G_CALLBACK(
                +[](GtkDrawingArea *, int w, int h, gpointer data) {
                    static_cast<native::wnd *>(data)->on_native_resize(
                        native::size(w, h));
                }),
            &owner);
        gtk_drawing_area_set_draw_func(
            GTK_DRAWING_AREA(drawing), draw, &owner, nullptr);
        // The child host receives background input while native children
        // remain independently targetable above the painting surface.
        install_input(owner, state(owner).content
                                 ? state(owner).content
                                 : drawing);
    }
    void create_surface(native::wnd &owner) {
        auto *overlay = gtk_overlay_new();
        auto *drawing = gtk_drawing_area_new();
        auto *content = gtk_fixed_new();
        gtk_widget_set_hexpand(drawing, true);
        gtk_widget_set_vexpand(drawing, true);
        gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), content);
        // Absolute child bounds describe the current allocation, not a
        // growing minimum for the parent. Keep oversized children clipped.
        gtk_overlay_set_measure_overlay(
            GTK_OVERLAY(overlay), content, false);
        gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), content, true);
        attach(owner, overlay, content);
        state(owner).drawing = drawing;
        configure_drawing(owner);
    }
} // namespace linux::wayland
