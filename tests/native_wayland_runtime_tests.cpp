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
#include "../lib/native/table_render.h"
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
    // Inspect the actual mapped native popup row, not just the combo
    // index.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkWidget *combo_popover(GtkWidget *widget) {
        if (GTK_IS_POPOVER(widget))
            return widget;
        for (auto *child = gtk_widget_get_first_child(widget); child;
             child = gtk_widget_get_next_sibling(child))
            if (auto *popup = combo_popover(child))
                return popup;
        return nullptr;
    }
    GtkWidget *popup_row(GtkWidget *widget, int index) {
        if (GTK_IS_CELL_VIEW(widget)) {
            auto *path =
                gtk_cell_view_get_displayed_row(GTK_CELL_VIEW(widget));
            const bool matches =
                path && gtk_tree_path_get_depth(path) == 1 &&
                gtk_tree_path_get_indices(path)[0] == index;
            if (path)
                gtk_tree_path_free(path);
            if (matches &&
                gtk_widget_get_focusable(gtk_widget_get_parent(widget)))
                return gtk_widget_get_parent(widget);
        }
        for (auto *child = gtk_widget_get_first_child(widget); child;
             child = gtk_widget_get_next_sibling(child))
            if (auto *row = popup_row(child, index))
                return row;
        return nullptr;
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
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
        std::size_t group_count() const override { return grouped ? 1 : 0; }
        native::table_group group(std::size_t) const override {
            return {17, "Rows", 0, row_count(), true, true};
        }
        bool grouped = false;
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
                        {
                            tree_view tree(
                                {tree_node("Folder",
                                           1,
                                           {tree_node("File", 2)},
                                           true),
                                 tree_node("Plain", 3)},
                                0,
                                0,
                                200,
                                120);
                            tree.set_parent(&host);
                            tree.create();
                            tree.show();
                            auto *folder = gtk_widget_get_first_child(
                                state(tree).editor);
                            auto *file =
                                gtk_widget_get_next_sibling(folder);
                            int folder_height = 0;
                            int file_height = 0;
                            gtk_widget_measure(folder,
                                               GTK_ORIENTATION_VERTICAL,
                                               200,
                                               nullptr,
                                               &folder_height,
                                               nullptr,
                                               nullptr);
                            gtk_widget_measure(file,
                                               GTK_ORIENTATION_VERTICAL,
                                               200,
                                               nullptr,
                                               &file_height,
                                               nullptr,
                                               nullptr);
                            expect(folder_height == file_height,
                                   "GTK folder disclosure does not add "
                                   "extra row padding");
                            auto *arrow = gtk_widget_get_first_child(
                                gtk_list_box_row_get_child(
                                    GTK_LIST_BOX_ROW(folder)));
                            expect(GTK_IS_EXPANDER(arrow),
                                   "tree disclosure uses a native "
                                   "expander");
                            gtk_expander_set_expanded(
                                GTK_EXPANDER(arrow), false);
                            expect(!tree.get_expanded(1),
                                   "native tree disclosure updates the "
                                   "portable model");
                        }
                        test_native_gtk_theme(window);
                        test_native_gtk_borders(window);
                        {
                            combo_box combo({"10 mm", "25 mm", "50 mm"},
                                            combo_box_style::editable,
                                            10,
                                            60,
                                            220,
                                            36);
                            combo.set_parent(&host); combo.create(); combo.show();
                            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                            expect(GTK_IS_COMBO_BOX_TEXT(state(combo).editor), "editable combo is one native GTK control");
                            gtk_editable_set_text(GTK_EDITABLE(state(combo).drawing), "25 mm");
                            expect(combo.get_selected_index() == 1 &&
                                gtk_combo_box_get_active(GTK_COMBO_BOX(state(combo).editor)) == 1,
                                "typed exact matches select the native popup item");
                            combo.set_text("10 mm");
                            combo.set_text("25 mm");
                            expect(
                                gtk_combo_box_get_active(GTK_COMBO_BOX(
                                    state(combo).editor)) == 1,
                                "programmatic exact matches "
                                "synchronize the popup index");
                            gtk_editable_set_text(GTK_EDITABLE(state(combo).drawing), "42 mm");
                            expect(combo.get_text() == "42 mm" && combo.get_selected_index() == -1,
                                "native editable combos retain custom text");
                            auto *entry = GTK_EDITABLE(state(combo).drawing);
                            gtk_editable_set_text(entry, "");
                            for (const char character : std::string("123 mm")) {
                                int cursor = gtk_editable_get_position(entry);
                                const char text[] = {character, '\0'};
                                gtk_editable_insert_text(entry, text, 1, &cursor);
                                gtk_editable_set_position(entry, cursor);
                            }
                            expect(combo.get_text() == "123 mm" && gtk_editable_get_position(entry) == 6,
                                   "individual editable-combo keystrokes retain left-to-right caret order");
                            gtk_editable_set_position(entry, 1);
                            int cursor = gtk_editable_get_position(entry);
                            gtk_editable_insert_text(entry, "X", 1, &cursor);
                            gtk_editable_set_position(entry, cursor);
                            expect(combo.get_text() == "1X23 mm" && gtk_editable_get_position(entry) == 2,
                                   "editable combos preserve insertion inside existing text");
                            combo.set_text("10 mm");
                            gtk_editable_set_text(entry, "50 mm");
                            auto *popup =
                                combo_popover(state(combo).editor);
                            expect(
                                popup,
                                "editable combo owns a native popup");
                            // Headless Weston has no input-grab serial.
                            // Map the same native rows without a modal
                            // grab.
                            gtk_popover_set_autohide(GTK_POPOVER(popup),
                                                     false);
                            for (int frame = 0;
                                 frame < 10 && !gtk_widget_get_mapped(
                                                   state(combo).editor);
                                 ++frame) {
                                while (g_main_context_iteration(
                                    nullptr, false)) {
                                }
                                g_usleep(10000);
                            }
                            gtk_combo_box_popup(
                                GTK_COMBO_BOX(state(combo).editor));
                            GtkWidget *selected_row = nullptr;
                            for (int frame = 0; frame < 50; ++frame) {
                                while (g_main_context_iteration(
                                    nullptr, false)) {
                                }
                                selected_row =
                                    popup_row(state(combo).editor, 2);
                                if (selected_row &&
                                    gtk_widget_get_mapped(selected_row))
                                    break;
                                g_usleep(10000);
                            }
                            expect(
                                selected_row &&
                                    gtk_widget_get_mapped(
                                        selected_row) &&
                                    gtk_widget_is_focus(selected_row) &&
                                    (gtk_widget_get_state_flags(
                                         selected_row) &
                                     GTK_STATE_FLAG_SELECTED),
                                "opening the native combo highlights "
                                "the typed 50 mm row");
                            expect(combo.get_text() == "50 mm" &&
                                       combo.get_selected_index() == 2,
                                   "native popup focus preserves typed "
                                   "text and logical selection");
                            gtk_combo_box_popdown(
                                GTK_COMBO_BOX(state(combo).editor));
                            G_GNUC_END_IGNORE_DEPRECATIONS
                            combo.set_style(combo_box_style::drop_down_list);
                            expect(GTK_IS_DROP_DOWN(state(combo).editor), "selection-only combo retains GtkDropDown");
                        }
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
                        expect(picked == state(bottom).editor,
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
                        expect(GTK_IS_BUTTON(state(command).editor),
                               "button must be native GTK");
                        auto *native_button = state(command).editor;
                        command.set_border_sides(border_sides::left | border_sides::right | border_sides::bottom);
                        expect(state(command).widget == native_button && state(command).editor == native_button,
                               "partial borders style the original native GTK button without reparenting it");
                        command.set_border_sides(border_sides::all);
                        expect(state(command).widget == native_button,
                               "restoring all edges restores the original GTK hierarchy");
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
                        table.set_selection_mode(table_selection_mode::multiple);
                        int releases = 0;
                        auto release_connection = table.on_mouse_click.connect([&](mouse_event event) {
                            if (event.action == mouse_action::release) ++releases;
                            return false;
                        });
                        const auto metrics = theme::create(table.get_gpx())->defaults();
                        const auto body = detail::table_body_bounds(table, metrics);
                        const int pitch = std::max(1, std::min(metrics.table_row_height,
                            int(body.h() / std::max<std::size_t>(1, table.get_visible_row_range().count))));
                        detail::handle_table_click(table, {20, native::coord(body.y1() + pitch / 2)});
                        detail::handle_table_click(table, {20, native::coord(body.y1() + pitch + pitch / 2)}, true);
                        expect(table.get_selected_rows().size() == 2,
                               "Ctrl click adds a portable table row");
                        auto *pointer_controllers = gtk_widget_observe_controllers(state(table).drawing);
                        for (guint index = 0; index < g_list_model_get_n_items(pointer_controllers); ++index) {
                            auto *controller = G_OBJECT(g_list_model_get_item(pointer_controllers, index));
                            if (GTK_IS_GESTURE_CLICK(controller))
                                g_signal_emit_by_name(controller, "released", 1, 20., double(body.y1() + pitch));
                            g_object_unref(controller);
                        }
                        g_object_unref(pointer_controllers);
                        expect(releases == 0 && table.get_selected_rows().size() == 2,
                               "GTK table release must not dispatch a second unmodified click");
                        table.on_mouse_click.disconnect(release_connection);
                        detail::handle_table_click(table, {20, native::coord(body.y1() + pitch / 2)}, true);
                        expect(table.get_selected_rows().size() == 1,
                               "Ctrl click removes a portable table row");
                        detail::handle_table_click(table, {20, native::coord(body.y1() + 3 * pitch + pitch / 2)}, false, true);
                        expect(table.get_selected_rows().size() == 3,
                               "Shift click selects the contiguous display range");
                        rows.grouped = true;
                        rows.on_change.emit({table_model_change_kind::reset, 0, rows.row_count()});
                        detail::handle_table_click(table, {20, native::coord(body.y1() + pitch / 2)});
                        expect(!table.get_group_expanded(17) && table.get_display_row_count() == 1,
                               "a grouped table collapses once on pointer press");
                        pointer_controllers = gtk_widget_observe_controllers(state(table).drawing);
                        for (guint index = 0; index < g_list_model_get_n_items(pointer_controllers); ++index) {
                            auto *controller = G_OBJECT(g_list_model_get_item(pointer_controllers, index));
                            if (GTK_IS_GESTURE_CLICK(controller))
                                g_signal_emit_by_name(controller, "released", 1, 20., double(body.y1() + pitch / 2));
                            g_object_unref(controller);
                        }
                        g_object_unref(pointer_controllers);
                        expect(!table.get_group_expanded(17),
                               "releasing a disclosure click must not immediately expand the group");
                        rows.grouped = false;
                        rows.on_change.emit({table_model_change_kind::reset, 0, rows.row_count()});
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
                        {
                            editor.set_text("copy field");
                            editor.select_all();
                            expect(editor.copy(), "copy field reaches the GTK clipboard");
                            text_edit destination("old", text_edit_mode::multi_line,
                                                  10, 60, 200, 40);
                            destination.set_parent(&host);
                            destination.create();
                            destination.select_all();
                            expect(destination.paste() && destination.get_text() == "copy field",
                                   "paste replaces a multiline GTK selection");
                        }
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
                        int completed = 0;
                        source.on_complete.connect([&](completion_item item) {
                            ++completed; source.insert(source.get_caret_offset(), item.insert); return true;
                        });
                        source.show_completion({{"return", "return", "statement"}});
                        expect(gtk_root_get_focus(gtk_widget_get_root(state(source).widget)) == state(source).drawing,
                               "showing completion focuses its keyboard host");
                        auto *controllers = gtk_widget_observe_controllers(state(source).drawing);
                        for (guint index = 0; index < g_list_model_get_n_items(controllers); ++index) {
                            auto *controller = G_OBJECT(g_list_model_get_item(controllers, index));
                            if (GTK_IS_EVENT_CONTROLLER_KEY(controller)) {
                                gboolean handled = false;
                                g_signal_emit_by_name(controller, "key-pressed", GDK_KEY_Tab, 0,
                                                      GdkModifierType(0), &handled);
                            }
                            g_object_unref(controller);
                        }
                        g_object_unref(controllers);
                        expect(completed == 1 && !source.get_completion_visible() &&
                                   source.get_text().find("return") != std::string::npos,
                               "Tab in GTK completion accepts and inserts through the app callback");
                        for (int index = 0; index < 50; ++index) {
                            panes.set_orientation(index % 2 ? split_orientation::horizontal : split_orientation::vertical);
                            gtk_paned_set_position(GTK_PANED(state(panes).widget), 50 + index);
                            gtk_widget_allocate(state(panes).widget, 250, 130, -1, nullptr);
                            for (int dispatch = 0; dispatch < 50 && g_main_context_pending(nullptr); ++dispatch)
                                g_main_context_iteration(nullptr, false);
                        }
                        expect(gtk_paned_get_wide_handle(GTK_PANED(state(panes).widget)),
                               "splitters start with a native wide handle");
                        auto *handle = gtk_widget_get_first_child(state(panes).widget);
                        while (handle && std::string_view(gtk_widget_get_css_name(handle)) != "separator")
                            handle = gtk_widget_get_next_sibling(handle);
                        expect(handle && gtk_widget_get_width(handle) >= panes.get_splitter_size(),
                               "native splitter allocation must retain the requested usable width");
                        {
                            panel first, second;
                            split_view transient(first, second, split_orientation::horizontal, 0, 0, 200, 100);
                            transient.set_parent(&host); transient.create(); transient.show();
                            gtk_widget_allocate(state(transient).widget, 200, 100, -1, nullptr);
                            gtk_paned_set_position(GTK_PANED(state(transient).widget), 60);
                            transient.destroy();
                            for (int dispatch = 0; dispatch < 50 && g_main_context_pending(nullptr); ++dispatch)
                                g_main_context_iteration(nullptr, false);
                            expect(!first.get_created() && !second.get_created(),
                                   "pending native splitter delivery must cancel on destruction");
                        }
                        {
                            modeless_wnd shell(window,
                                               "Closing moved splitter",
                                               0,
                                               0,
                                               240,
                                               160);
                            list first({"First"});
                            list second({"Second"});
                            split_view split(
                                first,
                                second,
                                split_orientation::horizontal,
                                0,
                                0,
                                240,
                                160);
                            shell.create();
                            shell.show();
                            split.set_parent(&shell);
                            split.create();
                            split.show();
                            gtk_widget_allocate(state(split).widget,
                                                240,
                                                160,
                                                -1,
                                                nullptr);
                            gtk_paned_set_position(
                                GTK_PANED(state(split).widget), 100);
                            auto *retained_rows = GTK_LIST_BOX(
                                g_object_ref(state(first).editor));
                            int late_selection = 0;
                            first.on_selection_change.connect([&](int) {
                                ++late_selection;
                                return false;
                            });
                            gboolean handled = false;
                            g_signal_emit_by_name(state(shell).widget,
                                                  "close-request",
                                                  &handled);
                            for (int i = 0;
                                 i < 50 &&
                                 g_main_context_pending(nullptr);
                                 ++i)
                                g_main_context_iteration(nullptr,
                                                         false);
                            expect(!shell.get_created() &&
                                       !split.get_created(),
                                   "closing an owning window cancels "
                                   "moved-splitter delivery");
                            late_selection = 0;
                            g_signal_emit_by_name(
                                retained_rows,
                                "row-selected",
                                gtk_list_box_get_row_at_index(
                                    retained_rows, 0));
                            expect(
                                late_selection == 0,
                                "retained GTK list rows cannot "
                                "dispatch into closed splitter panes");
                            g_object_unref(retained_rows);
                        }

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
