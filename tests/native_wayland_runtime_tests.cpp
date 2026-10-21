//
// Exercises real GTK Wayland widgets, silent setters, native edits,
// lazy collections, owned peers and destruction/recreation under
// Weston.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "native_gtk_theme_tests.h"
#include "../lib/native/toolkits/wayland/globals.h"
#include <gtksourceview/gtksource.h>
#ifdef NATIVE_TEST_ELEMENTARY
#include <granite-7.h>
#endif
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <array>
#include <stdexcept>
namespace
{
    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
    class lazy_rows final : public native::table_model
    {
    public:
        std::size_t row_count() const override {
            return 1000000;
        }
        native::table_row_id row_id(std::size_t row) const override {
            ++ids;
            return row + 1;
        }
        native::table_cell
        cell(std::size_t row, native::table_column_id) const override {
            ++cells;
            return {std::to_string(row), nullptr};
        }
        mutable int ids = 0;
        mutable int cells = 0;
    };
    class test_window final : public native::app_wnd
    {
    public:
        test_window()
            : app_wnd("Wayland native controls", 0, 0, 800, 600) {}
    };
} // namespace
int program(int, char **) {
    using namespace native;
    using linux::wayland::state;
    try {
        test_window window;
        window.menu << "&Actions_ && Tools"
                    << (menu_items("&Run_ && Go\tCtrl+R"));
        panel host(0, 0, 800, 600);
        button command("Run", 10, 10, 100, 40);
        check checked("Check", 120, 10, 100, 40);
        text_edit editor(
            "initial", text_edit_mode::single_line, 10, 60, 200, 40);
        code_edit source("int x;", 10, 110, 300, 100);
        icon_view icons(
            {{"First", {}, 1}, {"Second", {}, 2}}, 320, 60, 250, 150);
        lazy_rows rows;
        table_view table(10, 240, 600, 150);
        table.set_model(&rows).set_data_mode(
            table_data_mode::virtualized);
        table_column column{1, "Value", 180};
        column.sortable = true;
        table.set_columns({column});
        panel page_one, page_two, section_page, left, right;
        tab_view tabs(10, 420, 250, 130);
        tabs.add_item("One", page_one);
        tabs.add_item("Two", page_two);
        accordion sections(270, 420, 250, 130);
        sections.add_item("Section", section_page).set_expanded(true);
        split_view panes(left,
                         right,
                         split_orientation::horizontal,
                         530,
                         420,
                         250,
                         130);
        button bottom("Bottom action", 10, 560, 180, 40);
        int clicks = 0;
        bottom.on_click.connect([&] { ++clicks; return false; });
        bool failed = false;
        GdkPaintable *paintable = nullptr;
        window.on_wnd_create.connect([&] {
            host.set_parent(&window);
            host.create();
            host.show();
            for (wnd *control : std::array<wnd *, 10>{&bottom, &command,
                                                     &checked,
                                                     &editor,
                                                     &source,
                                                     &icons,
                                                     &table,
                                                     &tabs,
                                                     &sections,
                                                     &panes}) {
                control->set_parent(&host);
                control->create();
                control->show();
            }
            if (std::getenv("NATIVE_WAYLAND_SNAPSHOT"))
                paintable =
                    gtk_widget_paintable_new(state(window).widget);
            g_timeout_add_once(
                200,
                +[](gpointer data) {
                    std::unique_ptr<std::function<void()>> work(
                        static_cast<std::function<void()> *>(data));
                    (*work)();
                },
                new std::function<void()>([&] {
                    try {
                        {
                            modeless_wnd inspector(window, "Inspector");
                            inspector.create();
                            auto *peer = GTK_WINDOW(state(inspector).widget);
                            expect(!gtk_window_get_transient_for(peer),
                                   "modeless windows must stack independently");
                            expect(!gtk_window_get_modal(peer),
                                   "modeless windows must remain nonmodal");
                            expect(inspector.get_owner() == &window,
                                   "portable modeless ownership must remain");
                            modal_wnd dialog(window, "Modal dialog");
                            dialog.create();
                            auto *modal_peer = GTK_WINDOW(state(dialog).widget);
                            expect(gtk_window_get_transient_for(modal_peer) ==
                                       GTK_WINDOW(state(window).widget) &&
                                       gtk_window_get_modal(modal_peer),
                                   "modal windows must retain native ownership");
                        }
                        test_native_gtk_theme(window);
                        expect(GDK_IS_WAYLAND_DISPLAY(
                                   gdk_display_get_default()),
                               "display must be Wayland");
                        expect(gtk_widget_get_height(
                                   state(window).drawing) >= 600,
                               "window must preserve requested client "
                               "height below native chrome");
                        graphene_point_t point{20, 20}, location{};
                        expect(gtk_widget_compute_point(
                                   state(bottom).widget,
                                   state(window).widget,
                                   &point,
                                   &location),
                               "bottom button must have root coordinates");
                        expect(location.y < gtk_widget_get_height(
                                                state(window).widget),
                               "bottom button must fit inside window");
                        auto *picked = gtk_widget_pick(
                            state(window).widget,
                            location.x,
                            location.y,
                            GTK_PICK_DEFAULT);
                        while (picked && !GTK_IS_BUTTON(picked))
                            picked = gtk_widget_get_parent(picked);
                        expect(picked == state(bottom).widget,
                               "pointer picking must reach nested buttons");
                        g_signal_emit_by_name(picked, "clicked");
                        expect(clicks == 1,
                               "picked native button must deliver its action");
                        auto *menu = gtk_popover_menu_bar_get_menu_model(
                            GTK_POPOVER_MENU_BAR(state(window).menu));
                        gchar *label = nullptr;
                        g_menu_model_get_item_attribute(
                            menu, 0, G_MENU_ATTRIBUTE_LABEL, "s", &label);
                        const std::string title = label ? label : "";
                        g_free(label);
                        expect(title == "_Actions__ & Tools",
                               "GTK top menus must translate mnemonics "
                               "and escape literal underscores");
                        auto *submenu = g_menu_model_get_item_link(
                            menu, 0, G_MENU_LINK_SUBMENU);
                        auto *section = g_menu_model_get_item_link(
                            submenu, 0, G_MENU_LINK_SECTION);
                        label = nullptr;
                        g_menu_model_get_item_attribute(
                            section, 0, G_MENU_ATTRIBUTE_LABEL, "s", &label);
                        const std::string item = label ? label : "";
                        g_free(label);
                        g_object_unref(section);
                        g_object_unref(submenu);
                        expect(item == "_Run__ & Go",
                               "GTK menu items must preserve literal "
                               "ampersands and mnemonic markers");
                        expect(GTK_IS_BUTTON(state(command).widget),
                               "button must be native GTK");
                        expect(
                            GTK_IS_CHECK_BUTTON(state(checked).widget),
                            "check must be native GTK");
                        expect(GTK_IS_GRID_VIEW(state(icons).editor),
                               "icon view must use GTK grid");
                        expect(GTK_IS_COLUMN_VIEW(state(table).editor),
                               "table must use GTK column view");
                        expect(GTK_SOURCE_IS_VIEW(state(source).editor),
                               "source must use GtkSourceView");
#ifdef NATIVE_TEST_ELEMENTARY
                        expect(GRANITE_IS_VALIDATED_ENTRY(
                                   state(editor).editor),
                               "elementary entry must use Granite");
#endif
                        expect(GTK_IS_NOTEBOOK(state(tabs).widget),
                               "tabs must be native GTK");
                        expect(gtk_notebook_get_n_pages(GTK_NOTEBOOK(
                                   state(tabs).widget)) == 2,
                               "all native tabs must exist before "
                               "selection");
                        expect(GTK_IS_PANED(state(panes).widget) &&
                                   left.get_created() &&
                                   right.get_created(),
                               "native split panes must create both "
                               "borrowed children");
                        auto *expander = gtk_widget_get_first_child(
                            state(sections).widget);
                        expect(GTK_IS_EXPANDER(expander) &&
                                   section_page.get_created(),
                               "sections must use native expanders");
                        gtk_expander_set_expanded(
                            GTK_EXPANDER(expander), false);
                        expect(!section_page.get_created(),
                               "collapsing a section destroys its "
                               "borrowed resource");
                        expander = gtk_widget_get_first_child(
                            state(sections).widget);
                        gtk_expander_set_expanded(
                            GTK_EXPANDER(expander), true);
                        expect(section_page.get_created(),
                               "expanding recreates its borrowed "
                               "resource");
                        gtk_notebook_set_current_page(
                            GTK_NOTEBOOK(state(tabs).widget), 1);
                        expect(tabs.get_selected_index() == 1 &&
                                   page_two.get_created(),
                               "native tab selection must create its "
                               "portable page");
                        auto *columns =
                            GTK_COLUMN_VIEW(state(table).editor);
                        auto *native_column = GTK_COLUMN_VIEW_COLUMN(
                            g_list_model_get_item(
                                gtk_column_view_get_columns(columns),
                                0));
                        gtk_column_view_sort_by_column(
                            columns, native_column, GTK_SORT_ASCENDING);
                        expect(table.get_sort() &&
                                   table.get_sort()->column == 1,
                               "native sort requests must update "
                               "portable state");
                        g_object_unref(native_column);
                        native_column = GTK_COLUMN_VIEW_COLUMN(
                            g_list_model_get_item(
                                gtk_column_view_get_columns(columns),
                                0));
                        gtk_column_view_column_set_fixed_width(
                            native_column, 210);
                        expect(table.get_columns().front().width == 210,
                               "native resizing must update portable "
                               "width");
                        g_object_unref(native_column);
                        table.set_column_visibility_menu_enabled(true);
                        gtk_widget_activate_action(state(table).widget,
                                                   "columns.column1",
                                                   nullptr);
                        expect(!table.get_columns().front().visible,
                               "native column menu must hide a column");
                        gtk_widget_activate_action(state(table).widget,
                                                   "columns.column1",
                                                   nullptr);
                        expect(
                            table.get_columns().front().visible,
                            "native column menu must restore a column");
                        table.set_alternating_rows(true);
                        expect(state(table).portable_drawing,
                               "portable table drawing must retain "
                               "alternating rows");
                        table.set_alternating_rows(false);
                        expect(!state(table).portable_drawing,
                               "stock table must return to native "
                               "columns");
                        command.set_parent(&page_two);
                        command.set_parent(&host);
                        expect(command.get_created(),
                               "reparenting must preserve native "
                               "resources");
                        int changes = 0;
                        checked.on_change.connect([&](bool) {
                            ++changes;
                            return false;
                        });
                        checked.set_checked(true);
                        expect(changes == 0,
                               "check setters must be silent");
                        gtk_check_button_set_active(
                            GTK_CHECK_BUTTON(state(checked).widget),
                            false);
                        expect(
                            !checked.get_checked() && changes == 1,
                            "native toggles must update portable state "
                            "once");
                        editor.set_validator(
                            [](const std::string &text) {
                                return text.size() < 12;
                            });
                        gtk_editable_set_text(
                            GTK_EDITABLE(state(editor).editor),
                            "valid");
                        expect(editor.get_text() == "valid",
                               "native UTF-8 entry must update "
                               "portable text");
                        int insertion = 5;
                        gtk_editable_insert_text(
                            GTK_EDITABLE(state(editor).editor),
                            "invalid value which is too long",
                            -1,
                            &insertion);
                        expect(editor.get_text() == "valid",
                               "invalid native edits must be rejected");
                        expect(
                            std::string(gtk_editable_get_text(
                                GTK_EDITABLE(state(editor).editor))) ==
                                "valid",
                            "rejected entry must be restored");
                        gtk_text_buffer_set_text(
                            gtk_text_view_get_buffer(
                                GTK_TEXT_VIEW(state(source).editor)),
                            "int y;",
                            -1);
                        expect(source.get_text() == "int y;",
                               "source edits must update canonical "
                               "document");
                        expect(rows.ids < 10000 && rows.cells < 10000,
                               "virtual rows must remain lazy");
                        if (const char *path = std::getenv(
                                "NATIVE_WAYLAND_SNAPSHOT")) {
                            auto *widget = state(window).widget;

                            auto *snapshot = gtk_snapshot_new();
                            gdk_paintable_snapshot(
                                paintable,
                                GDK_SNAPSHOT(snapshot),
                                gtk_widget_get_width(widget),
                                gtk_widget_get_height(widget));
                            auto *node =
                                gtk_snapshot_free_to_node(snapshot);
                            expect(node != nullptr,
                                   "native widgets must produce a "
                                   "render node");
                            auto *texture = gsk_renderer_render_texture(
                                gtk_native_get_renderer(
                                    GTK_NATIVE(widget)),
                                node,
                                nullptr);
                            auto *bytes =
                                gdk_texture_save_to_png_bytes(texture);
                            gsize length = 0;
                            const void *pixels =
                                g_bytes_get_data(bytes, &length);
                            std::ofstream output(
                                std::filesystem::path(path),
                                std::ios::binary);
                            output.write(
                                static_cast<const char *>(pixels),
                                length);
                            g_bytes_unref(bytes);
                            g_object_unref(texture);
                            gsk_render_node_unref(node);
                        }
                        source.add_marker({0, marker_kind::breakpoint});
                        expect(gtk_widget_get_visible(
                                   state(source).drawing),
                               "portable source overlays must retain "
                               "their drawing path");
                        source.clear_markers(marker_kind::breakpoint);
                        expect(!gtk_widget_get_visible(
                                   state(source).drawing),
                               "stock source editing returns to "
                               "GtkSourceView");
                        const int selected = icons.get_selected_index();
                        icons.destroy();
                        icons.create();
                        icons.show();
                        expect(
                            icons.get_selected_index() == selected,
                            "recreation must preserve portable state");
                        command.destroy();
                        command.create();
                        command.show();
                        panel closing_parent(0, 0, 200, 80);
                        button closing_child("Close", 0, 0, 100, 40);
                        closing_parent.set_parent(&window);
                        closing_parent.create();
                        closing_child.set_parent(&closing_parent);
                        closing_child.create();
                        GWeakRef native_child;
                        g_weak_ref_init(&native_child,
                                        state(closing_child).widget);
                        closing_child.on_native_key(
                            {key_code::a, key_action::press});
                        closing_child.on_key_reset.connect([&] {
                            closing_parent.destroy();
                            return false;
                        });
                        closing_child.destroy();
                        auto *retained = g_weak_ref_get(&native_child);
                        const bool released = retained == nullptr;
                        if (retained)
                            g_object_unref(retained);
                        g_weak_ref_clear(&native_child);
                        expect(released,
                               "child teardown must release the peer's "
                               "GTK reference");
                        expect(!closing_parent.get_created() &&
                                   !closing_child.get_created(),
                               "parent teardown during child input reset "
                               "must keep pending native removal safe");
                        panel unmap_parent(0, 0, 200, 80);
                        button unmap_child("Unmap", 0, 0, 100, 40);
                        unmap_parent.set_parent(&window);
                        unmap_parent.create();
                        unmap_parent.show();
                        unmap_child.set_parent(&unmap_parent);
                        unmap_child.create();
                        unmap_child.show();
                        expect(gtk_widget_get_mapped(
                                   state(unmap_child).widget),
                               "unmap regression requires a mapped child");
                        g_signal_connect(
                            state(unmap_child).widget,
                            "unmap",
                            G_CALLBACK(+[](GtkWidget *, gpointer data) {
                                static_cast<panel *>(data)->destroy();
                            }),
                            &unmap_parent);
                        unmap_child.destroy();
                        expect(!unmap_parent.get_created() &&
                                   !unmap_child.get_created(),
                               "native unmap must tolerate synchronous "
                               "parent destruction during removal");
                    } catch (const std::exception &error) {
                        std::cerr << error.what() << '\n';
                        failed = true;
                    }
                    // Close a populated window through GTK while a native
                    // editor owns focus, rather than bypassing close-request.
                    gtk_widget_grab_focus(state(editor).editor);
                    gboolean handled = false;
                    g_signal_emit_by_name(state(window).widget,
                                          "close-request",
                                          &handled);
                }));
            return false;
        });
        app::run(window);
        if (paintable)
            g_object_unref(paintable);
        expect(!host.get_created() && !command.get_created(),
               "parent teardown must destroy children");
        return failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
