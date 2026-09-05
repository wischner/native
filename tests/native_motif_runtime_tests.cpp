//
// Exercises Motif peers on a real Xt display: early graphics, focus,
// table scrolling/grids, bounded repainting, and native splitter sashes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/openmotif/globals.h"
#include "../lib/native/toolkits/openmotif/theme_support.h"

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <Xm/DrawingA.h>
#include <Xm/List.h>
#include <Xm/MessageB.h>
#include <Xm/SashP.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
    namespace motif = linux::openmotif;
    int failures = 0;

    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    Widget widget(native::wnd &owner) {
        return motif::wnd_bindings.handle_from_object(&owner);
    }

    void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    // Force queued X requests through before checking for expose backlogs.
    void pump() {
        XSync(motif::cached_display, False);
        int count = 0;
        while (XtAppPending(motif::app_instance)) {
            expect(++count < 4000, "unbounded Motif redraw backlog");
            XtAppProcessEvent(motif::app_instance, XtIMAll);
            XSync(motif::cached_display, False);
        }
    }

    void key(Widget target, KeySym symbol, unsigned int modifiers = 0) {
        XEvent event{};
        event.xkey.type = KeyPress;
        event.xkey.display = XtDisplay(target);
        event.xkey.window = XtWindow(target);
        event.xkey.root = RootWindowOfScreen(XtScreen(target));
        event.xkey.time = XtLastTimestampProcessed(XtDisplay(target));
        event.xkey.keycode = XKeysymToKeycode(XtDisplay(target), symbol);
        event.xkey.state = modifiers;
        event.xkey.same_screen = True;
        XtDispatchEvent(&event);
        pump();
    }

    void pointer(Widget target, int type, int x, int y) {
        XEvent event{};
        event.xbutton.type = type;
        event.xbutton.display = XtDisplay(target);
        event.xbutton.window = XtWindow(target);
        event.xbutton.root = RootWindowOfScreen(XtScreen(target));
        event.xbutton.time = XtLastTimestampProcessed(XtDisplay(target));
        event.xbutton.button = Button1;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.same_screen = True;
        XtDispatchEvent(&event);
        pump();
    }

    unsigned long pixel(native::wnd &owner, int x, int y) {
        auto *cache = motif::wnd_gpx_bindings.object_from_handle(&owner);
        expect(cache && cache->backbuffer, "painted peer has a backbuffer");
        XImage *image = XGetImage(motif::cached_display, cache->backbuffer,
            x, y, 1, 1, AllPlanes, ZPixmap);
        expect(image != nullptr, "read painted pixel");
        const auto result = XGetPixel(image, 0, 0);
        XDestroyImage(image);
        return result;
    }

    void checks(native::app_wnd &root) {
        native::modeless_wnd early(root, "Early graphics", 80, 80, 300, 200);
        early.create();
        auto appearance = native::theme::create(early.get_gpx());
        expect(appearance->get_status_bar_height() > 0,
               "unrealized window supports chrome metrics");
        appearance.reset();
        native::combo_box combo({"One", "Two"},
            native::combo_box_style::editable, 10, 10, 180, 26);
        attach(combo, early);
        combo.set_selected_index(1);
        early.show();
        pump();
        early.destroy();

        native::modal_wnd modal(root, "Modal palette", 80, 80, 300, 200);
        modal.create();
        Widget message = XmCreateInformationDialog(
            motif::shell_bindings.handle_from_object(&root),
            const_cast<char *>("message_box"), nullptr, 0);
        for (const char *resource : {XmNbackground, XmNforeground,
                 XmNtopShadowColor, XmNbottomShadowColor}) {
            Pixel expected = 0, actual = 0;
            XtVaGetValues(message, resource, &expected, nullptr);
            XtVaGetValues(widget(modal), resource, &actual, nullptr);
            expect(actual == expected, "modal uses message-box palette");
        }
        XtDestroyWidget(XtParent(message));
        modal.show();
        pump();
        expect(modal.get_dimensions().w >= 280 && modal.get_dimensions().h >= 180,
               "modal shell retains requested dimensions on first show");
        {
            Pixel foreground = 0;
            XtVaGetValues(widget(modal), XmNforeground, &foreground, nullptr);
            const auto colors = native::theme::create(modal.get_gpx())->native_palette();
            const auto expected = motif::theme_pixel_color(widget(modal), foreground);
            expect(colors.button_text == expected,
                   "custom modal labels use the native dialog foreground");
        }
        modal.destroy();

        {
            native::ruler horizontal(root, native::window_edge::top, 24);
            native::ruler vertical(root, native::window_edge::left, 24);
            root.invalidate();
            pump();
            const auto corner = motif::theme_pixel_color(widget(root), pixel(root, 10, 10));
            const auto colors = native::theme::create(root.get_gpx())->native_palette();
            expect(corner == colors.menu_bar_bg,
                   "perpendicular ruler corner uses menu paper");
        }

        native::list general({"General"}, 0, 0, 220, 100);
        native::list advanced({"Advanced", "Second", "Third"}, 0, 0, 220, 100);
        native::tab_view tabs(10, 10, 300, 140);
        tabs.add_item("General", general);
        tabs.add_item("Advanced", advanced);
        attach(tabs, root);
        pump();
        auto *notebook = motif::tab_view_bindings.object_from_handle(&tabs);
        // Exercise the actual native tab callback, including first creation.
        XEvent tab_event{};
        tab_event.type = ButtonRelease;
        XmPushButtonCallbackStruct activate{};
        activate.reason = XmCR_ACTIVATE;
        activate.event = &tab_event;
        activate.click_count = 1;
        XtCallCallbacks(notebook->tabs[1], XmNactivateCallback, &activate);
        pump();
        expect(tabs.get_selected_index() == 1, "native Advanced tab selected");
        Dimension content_width = 0, content_height = 0;
        Dimension page_width = 0, page_height = 0;
        XtVaGetValues(widget(advanced), XmNwidth, &content_width,
            XmNheight, &content_height, nullptr);
        XtVaGetValues(notebook->pages[1], XmNwidth, &page_width,
            XmNheight, &page_height, nullptr);
        expect(page_width > 250 && page_height > 80 &&
                   content_width == page_width && content_height == page_height,
               "first selected page fills the native notebook page");
        tabs.destroy();

        unsigned char policy = XmPOINTER;
        XtVaGetValues(motif::shell_bindings.handle_from_object(&root),
                      XmNkeyboardFocusPolicy, &policy, nullptr);
        expect(policy == XmEXPLICIT, "editors use click/keyboard focus");
        native::text_edit single("First", native::text_edit_mode::single_line,
                                 10, 10, 200, 28);
        native::text_edit multi("Second", native::text_edit_mode::multi_line,
                                10, 50, 200, 60);
        attach(single, root);
        attach(multi, root);
        pump();
        XmProcessTraversal(widget(single), XmTRAVERSE_CURRENT);
        XmTextFieldSetInsertionPosition(widget(single), 5);
        key(widget(single), XK_x);
        expect(single.get_text() == "Firstx" && multi.get_text() == "Second",
               "single-line typing cannot change multiline editor");
        XmProcessTraversal(widget(multi), XmTRAVERSE_CURRENT);
        XmTextSetInsertionPosition(widget(multi), 6);
        key(widget(multi), XK_y);
        expect(single.get_text() == "Firstx" && multi.get_text() == "Secondy",
               "multiline typing cannot change single-line editor");
        key(widget(multi), XK_a, ControlMask);
        char *selection = XmTextGetSelection(widget(multi));
        const std::string selected = selection ? selection : "";
        if (selection)
            XtFree(selection);
        expect(selected == "Secondy", "Ctrl+A selects native text");
        single.destroy();
        multi.destroy();

        native::list sample({"One", "Two"}, 10, 10, 200, 80);
        attach(sample, root);
        pump();
        Widget list = motif::list_content_bindings.object_from_handle(&sample);
        XmFontList fonts = nullptr;
        XtVaGetValues(list, XmNfontList, &fonts, nullptr);
        XmString text = XmStringCreateLocalized(const_cast<char *>("Mg"));
        const int row_height = XmStringHeight(fonts, text) + 2;
        XmStringFree(text);
        expect(native::theme::create(root.get_gpx())->defaults().list_item_height
                   == row_height, "theme list uses native list font metrics");
        sample.destroy();

        std::vector<native::icon_view_item> items;
        for (int index = 0; index < 50; ++index)
            items.push_back({"Icon " + std::to_string(index), nullptr});
        native::icon_view icons(items, 0, 0, 280, 120);
        native::accordion accordion(10, 10, 300, 180);
        accordion.add_item("Icons", icons);
        accordion.set_expanded_index(0);
        attach(accordion, root);
        pump();
        Widget scroll = nullptr;
        XtVaGetValues(widget(icons), XmNverticalScrollBar, &scroll, nullptr);
        expect(scroll != nullptr, "accordion body retains a native scrollbar");
        Position x = 0;
        Dimension width = 0, host_width = 0;
        XtVaGetValues(scroll, XmNx, &x, XmNwidth, &width, nullptr);
        XtVaGetValues(widget(icons), XmNwidth, &host_width, nullptr);
        expect(x >= 0 && x + width <= host_width &&
                   host_width <= accordion.get_dimensions().w,
               "accordion scrollbar stays inside the section width");
        accordion.destroy();

        auto icon = std::make_shared<native::img>(48, 48);
        icon->get_gpx().clear(native::rgba(0, 0, 0, 0))
            .set_ink(native::rgba(255, 0, 0, 255))
            .draw_rect(native::rect(6, 8, 36, 32), true);
        native::tree_view tree({{"Folder", icon, 1}}, 10, 10, 200, 80);
        attach(tree, root);
        auto *tree_state = motif::tree_view_bindings.object_from_handle(&tree);
        XmFontList native_fonts = nullptr;
        XtVaGetValues(tree_state->content, XmNrenderTable, &native_fonts, nullptr);
        XmString label = XmStringCreateLocalized(const_cast<char *>("Folder"));
        expect(XmStringBaseline(tree_state->tree_label_fonts, label) + 2 ==
                   XmStringBaseline(native_fonts, label),
               "tree label baseline is raised exactly two pixels");
        XmStringFree(label);
        unsigned char alignment = XmALIGNMENT_BASELINE_BOTTOM;
        XtVaGetValues(tree_state->items.front(), XmNalignment, &alignment, nullptr);
        expect(alignment == XmALIGNMENT_CENTER,
               "tree gadgets center icons relative to their labels");
        Pixmap icon_pixmap = None;
        XtVaGetValues(tree_state->items.front(),
                      XmNsmallIconPixmap, &icon_pixmap, nullptr);
        const auto icon_size = tree.get_icon_size();
        XImage *icon_pixels = XGetImage(motif::cached_display, icon_pixmap,
            0, 0, icon_size.w, icon_size.h, AllPlanes, ZPixmap);
        int first_ink = icon_size.h, last_ink = -1;
        const auto background = XGetPixel(icon_pixels, 0, 0);
        for (int y = 0; y < icon_size.h; ++y) {
            if (XGetPixel(icon_pixels, icon_size.w / 2, y) != background) {
                first_ink = std::min(first_ink, y);
                last_ink = y;
            }
        }
        XDestroyImage(icon_pixels);
        expect(last_ink >= first_ink &&
                   std::abs(first_ink + last_ink - (icon_size.h - 1)) <= 1,
               "full source icon is scaled and centered, not cropped");
        tree.destroy();

        native::table_store store;
        for (int row = 1; row <= 100; ++row)
            store.add_row({static_cast<native::table_row_id>(row),
                {{1, {"Name"}}, {2, {"Value"}}}});
        native::table_view materialized(10, 10, 300, 180);
        native::table_view virtualized(330, 10, 300, 180);
        materialized.set_data_mode(native::table_data_mode::materialized);
        virtualized.set_data_mode(native::table_data_mode::virtualized);
        for (auto *table : {&materialized, &virtualized}) {
            table->set_columns({{1, "Name", 120}, {2, "Value", 140}});
            table->set_model(&store);
            attach(*table, root);
        }
        pump();
        for (auto *table : {&materialized, &virtualized}) {
            auto *state = motif::table_view_bindings.object_from_handle(table);
            expect(XmIsDrawingArea(widget(*table)) &&
                       XmIsScrollBar(state->vertical_scrollbar),
                   "both table modes use matching hosts and native scrollbars");
            const auto metrics = native::theme::create(table->get_gpx())->defaults();
            auto theme = native::theme::create(table->get_gpx());
            const auto colors = theme->native_palette();
            const auto alternate = theme->get_content_alternate_background_color();
            const auto distance = [](native::rgba a, native::rgba b) {
                return std::abs(int(a.r) - b.r) + std::abs(int(a.g) - b.g) +
                       std::abs(int(a.b) - b.b);
            };
            expect(distance(colors.content_bg, alternate) >= 24,
                   "alternating table rows have a visible tint");
            expect(distance(colors.content_bg, colors.separator) >= 120,
                   "table grids contrast with the native data background");
            const int y = metrics.header_height + metrics.table_row_height - 1;
            table->set_grid_lines(native::table_grid_lines::none);
            table->set_alternating_rows(true);
            pump();
            const int second_row_y = metrics.header_height + metrics.table_row_height + 5;
            const auto tinted = pixel(*table, 270, second_row_y);
            table->set_alternating_rows(false);
            pump();
            expect(tinted != pixel(*table, 270, second_row_y),
                   "alternating-row toggle changes actual table pixels");
            table->set_alternating_rows(true);
            pump();
            std::array<unsigned long, 4> plain{};
            for (int offset = 0; offset < 4; ++offset)
                plain[offset] = pixel(*table, 100, y + offset - 1);
            table->set_grid_lines(native::table_grid_lines::both);
            pump();
            // Grid rows may include a one-pixel viewport inset.
            bool changed = false;
            for (int offset = 0; offset < 4; ++offset)
                changed = changed || pixel(*table, 100, y + offset - 1)
                    != plain[offset];
            expect(changed, "grid checkbox changes painted row edges");
            XmScrollBarCallbackStruct scroll{};
            scroll.reason = XmCR_DRAG;
            scroll.value = 50;
            XtCallCallbacks(state->vertical_scrollbar, XmNdragCallback, &scroll);
            pump();
            expect(table->get_vertical_scroll_row() == 50,
                   "native scrollbar drag updates logical table rows");
        }
        materialized.destroy();
        virtualized.destroy();

        native::code_edit code("int answer = 42;", 10, 10, 300, 100);
        attach(code, root);
        pump();
        const auto colors = native::theme::create(code.get_gpx())->native_palette();
        expect(colors.content_bg.r == 255 && colors.content_text.r == 0,
               "code editor has a light surface for default syntax colors");
        code.destroy();

        native::list first({"Left"}, 0, 0, 100, 100);
        native::list second({"Right"}, 0, 0, 100, 100);
        native::split_view split(first, second,
            native::split_orientation::horizontal, 10, 10, 500, 250);
        attach(split, root);
        pump();
        const auto before = split.get_ratio();
        const auto bar = split.get_splitter_bounds();
        pointer(widget(split), ButtonPress, bar.p.x + 1, 40);
        pointer(widget(split), ButtonRelease, bar.p.x + 70, 40);
        expect(split.get_ratio() > before + 0.05f,
               "full divider dragging changes ratio");
        Dimension first_width = 0;
        XtVaGetValues(widget(first), XmNwidth, &first_width, nullptr);
        expect(first_width > 280, "native first pane follows dragged ratio");
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(widget(split), XmNchildren, &children,
                      XmNnumChildren, &count, nullptr);
        bool sash = false;
        for (Cardinal index = 0; index < count; ++index)
            sash = sash || (XmIsSash(children[index]) && XtIsManaged(children[index]));
        expect(sash, "splitter exposes a native Motif grip");
        split.destroy();
        pump();
    }
}

int program(int, char **) {
    native::app_wnd root("Motif peer regressions", 20, 20, 660, 400);
    root.on_wnd_create.connect([&root] {
        native::app::post([&root] {
            try {
                checks(root);
                std::cout << "Motif peer regressions passed\n";
            } catch (const std::exception &error) {
                ++failures;
                std::cerr << error.what() << '\n';
            }
            root.destroy();
        });
        return true;
    });
    return native::app::run(root) || failures ? 1 : 0;
}
