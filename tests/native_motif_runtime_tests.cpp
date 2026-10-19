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
#include <Xm/PushBG.h>
#include <Xm/List.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/SashP.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <array>
#include <iostream>
#include <memory>
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

    // Native focus/default-button reservations can move relief far inward.
    void check_border_metrics(native::app_wnd &root) {
        native::button button("", 360, 40, 180, 80);
        attach(button, root);
        const auto release = [](XImage *image) { if (image) XDestroyImage(image); };
        const auto capture = [&] {
            Position x = 0, y = 0;
            Dimension width = 0, height = 0;
            XtVaGetValues(widget(button), XmNx, &x, XmNy, &y,
                XmNwidth, &width, XmNheight, &height, nullptr);
            auto image = std::unique_ptr<XImage, decltype(release)>(
                XGetImage(motif::cached_display, XtWindow(widget(root)),
                    x, y, width, height, AllPlanes, ZPixmap), release);
            expect(bool(image), "read native button and exposed parent pixels");
            return image;
        };
        const auto sample = [](XImage &image, unsigned side, int depth) {
            const int x[] = {image.width / 2, image.width - 1 - depth,
                image.width / 2, depth};
            const int y[] = {depth, image.height / 2,
                image.height - 1 - depth, image.height / 2};
            return XGetPixel(&image, x[side], y[side]);
        };
        for (int reservation : {0, 1, 2}) {
            XtVaSetValues(widget(button), XmNhighlightThickness, reservation + 2,
                XmNshadowThickness, 2, XmNdefaultButtonShadowThickness, reservation,
                XmNshowAsDefault, reservation ? 1 : 0, nullptr);
            button.set_dimensions({static_cast<native::dim>(180 + reservation), 80});
            pump();
            const auto reference = capture();
            const auto paper = XGetPixel(reference.get(), reference->width / 2, reference->height / 2);
            for (unsigned mask = 0; mask < 16; ++mask) {
                button.set_border_sides(static_cast<native::border_sides>(mask));
                pump();
                const auto actual = capture();
                for (unsigned side = 0; side < 4; ++side)
                    for (int depth = 0; depth < 16; ++depth) {
                        const auto expected = mask & (1u << side)
                            ? sample(*reference, side, depth) : paper;
                        if (sample(*actual, side, depth) != expected)
                            throw std::runtime_error("Motif border pixels: reservation=" +
                                std::to_string(reservation) + " mask=" + std::to_string(mask) +
                                " side=" + std::to_string(side) + " depth=" + std::to_string(depth) +
                                " expected=" + std::to_string(expected) +
                                " actual=" + std::to_string(sample(*actual, side, depth)));
                    }
            }
        }
        button.destroy();
    }

    class counted_pane : public native::list {
    public:
        explicit counted_pane(const char *label)
            : native::list({label}, 0, 0, 100, 100) {}

        int shows = 0;
        int arrangements = 0;

    protected:
        void show_native() override {
            ++shows;
            native::list::show_native();
        }

        void on_bounds_changed() override {
            ++arrangements;
            native::list::on_bounds_changed();
        }
    };

    // Repeated invalidations retain the displayed frame until one expose.
    void check_repaint_batch(native::app_wnd &root) {
        native::canvas surface(10, 10, 220, 100);
        surface.set_horizontal_scrollbar_policy(native::scrollbar_policy::never);
        surface.set_vertical_scrollbar_policy(native::scrollbar_policy::never);
        int paints = 0;
        surface.on_wnd_paint.connect([&](native::wnd_paint_event event) {
            ++paints;
            event.g.clear(native::rgba(200, 30, 80, 255));
            return true;
        });
        attach(surface, root);
        pump();
        paints = 0;
        const auto read_pixel = [&] {
            XImage *image = XGetImage(motif::cached_display,
                XtWindow(widget(surface)), 20, 20, 1, 1, AllPlanes, ZPixmap);
            const auto result = XGetPixel(image, 0, 0);
            XDestroyImage(image);
            return result;
        };
        const auto displayed = read_pixel();
        for (int index = 0; index < 20; ++index)
            surface.invalidate();
        expect(paints == 0, "invalidation stays asynchronous");
        expect(read_pixel() == displayed,
               "invalidation preserves pixels until the next frame");
        pump();
        expect(paints == 1, "a burst of invalidations paints only once");
        surface.invalidate();
        surface.destroy();
        pump();
        surface.create();
        surface.show();
        pump();
        paints = 0;
        surface.invalidate();
        pump();
        expect(paints == 1, "recreated host has a fresh repaint transaction");
        surface.destroy();
    }

    void count_tree_exposes(Widget, XtPointer data,
                             XEvent *event, Boolean *) {
        if (event->type == Expose)
            ++*static_cast<int *>(data);
    }

    // Branch changes repaint the outline without exposing the outer frame.
    void check_tree_content_repaint(native::app_wnd &root) {
        native::tree_view tree({native::tree_node("Folder", 1,
            {native::tree_node("Child", 2)}, true)}, 10, 10, 300, 180);
        attach(tree, root);
        pump();
        auto *state = motif::tree_view_bindings.object_from_handle(&tree);
        const auto original_items = state->items;
        const auto border_pixels = [&] {
            Dimension width = 0, height = 0;
            XtVaGetValues(state->widget, XmNwidth, &width,
                          XmNheight, &height, nullptr);
            XImage *image = XGetImage(motif::cached_display,
                XtWindow(state->widget), 0, 0, width, height, AllPlanes, ZPixmap);
            std::vector<unsigned long> pixels;
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (x < 2 || y < 2 || x >= width - 2 || y >= height - 2)
                        pixels.push_back(XGetPixel(image, x, y));
            XDestroyImage(image);
            return pixels;
        };
        const auto original_border = border_pixels();
        int frame_exposes = 0, content_exposes = 0;
        XtAddEventHandler(state->widget, ExposureMask, False,
                          count_tree_exposes, &frame_exposes);
        XtAddEventHandler(state->content, ExposureMask, False,
                          count_tree_exposes, &content_exposes);
        tree.set_selected_item(2);
        pump();
        frame_exposes = content_exposes = 0;
        tree.set_expanded(1, false);
        pump();
        expect(tree.get_selected_item() == 1,
               "collapsing a selected descendant selects its ancestor");
        expect(frame_exposes == 0 && content_exposes > 0,
               "programmatic collapse exposes content without the frame");
        frame_exposes = content_exposes = 0;
        int changes = 0;
        tree.on_expanded_change.connect([&](native::tree_item_id, bool) {
            ++changes;
            return false;
        });
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(state->content, XmNchildren, &children,
                      XmNnumChildren, &count, nullptr);
        Widget disclosure = nullptr;
        for (Cardinal index = 0; index < count; ++index) {
            if (XmIsPushButtonGadget(children[index]) &&
                XtIsManaged(children[index])) {
                disclosure = children[index];
                break;
            }
        }
        expect(disclosure != nullptr, "tree exposes a native disclosure gadget");
        Position disclosure_x = 0, disclosure_y = 0;
        Dimension disclosure_width = 0, disclosure_height = 0;
        XtVaGetValues(disclosure, XmNx, &disclosure_x, XmNy, &disclosure_y,
                      XmNwidth, &disclosure_width,
                      XmNheight, &disclosure_height, nullptr);
        const auto activate_disclosure = [&] {
            XEvent event{};
            event.xbutton.type = ButtonRelease;
            event.xbutton.display = motif::cached_display;
            event.xbutton.window = XtWindow(state->content);
            event.xbutton.button = Button1;
            event.xbutton.x = disclosure_x + disclosure_width / 2;
            event.xbutton.y = disclosure_y + disclosure_height / 2;
            XmPushButtonCallbackStruct activation{};
            activation.reason = XmCR_ACTIVATE;
            activation.event = &event;
            activation.click_count = 1;
            XtCallCallbacks(disclosure, XmNactivateCallback, &activation);
            pump();
        };
        activate_disclosure();
        expect(tree.get_expanded(1) && changes == 1,
               "native expansion updates the model and signals once");
        expect(frame_exposes == 0 && content_exposes > 0,
               "native expansion exposes content without the frame");
        expect(state->items == original_items,
               "branch changes retain native item resources");
        expect(border_pixels() == original_border,
               "native disclosure leaves frame pixels unchanged");
        frame_exposes = content_exposes = 0;
        activate_disclosure();
        expect(!tree.get_expanded(1) && changes == 2,
               "a second native disclosure click collapses the branch");
        expect(frame_exposes == 0 && border_pixels() == original_border,
               "native collapse neither clears nor changes the outer frame");
        tree.set_border_visible(false);
        pump();
        Dimension shadow = 1;
        XtVaGetValues(state->widget, XmNshadowThickness, &shadow, nullptr);
        expect(shadow == 0, "explicit border changes still update the frame");
        std::vector<native::tree_view_item> rows;
        for (native::tree_item_id id = 1; id <= 40; ++id)
            rows.push_back(native::tree_node("Scroll row", id));
        tree.set_items(std::move(rows));
        tree.set_selected_item(40);
        pump();
        Position content_y = 0;
        XtVaGetValues(state->content, XmNy, &content_y, nullptr);
        expect(content_y < 0,
               "framed tree scrolls its inner work widget to reveal selection");
        tree.destroy();
    }

    void checks(native::app_wnd &root) {
        // A menu must not consume the requested work-area height.
        native::modeless_wnd sized(root, "Menu geometry", 40, 40, 820, 660);
        sized.menu << "File" << native::menu_items("Close");
        sized.create();
        sized.show();
        pump();
        auto check_work_size = [&sized](int width, int height) {
            Dimension actual_width = 0, actual_height = 0;
            XtVaGetValues(widget(sized), XmNwidth, &actual_width,
                          XmNheight, &actual_height, nullptr);
            expect(actual_width == width && actual_height == height,
                   "menu preserves requested work dimensions");
        };
        check_work_size(820, 660);
        sized.set_dimensions({760, 700});
        pump();
        check_work_size(760, 700);
        sized.set_bounds({50, 50, 800, 680});
        pump();
        check_work_size(800, 680);
        sized.destroy();
        check_repaint_batch(root);
        check_tree_content_repaint(root);
        check_border_metrics(root);
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
        Widget list = native::detail::peer_state<motif::motif_list>(sample)->content;
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

        counted_pane first("Left");
        counted_pane second("Right");
        native::split_view split(first, second,
            native::split_orientation::horizontal, 10, 10, 500, 250);
        attach(split, root);
        pump();
        const int first_shows = first.shows;
        const int second_shows = second.shows;
        const int first_arrangements = first.arrangements;
        const int second_arrangements = second.arrangements;
        split.set_bounds(split.get_bounds());
        pump();
        expect(first.arrangements == first_arrangements &&
                   second.arrangements == second_arrangements,
               "unchanged split geometry does not rearrange panes");
        const auto before = split.get_ratio();
        const auto bar = split.get_splitter_bounds();
        pointer(widget(split), ButtonPress, bar.p.x + 1, 40);
        pointer(widget(split), ButtonRelease, bar.p.x + 70, 40);
        expect(first.shows == first_shows && second.shows == second_shows,
               "divider dragging does not show visible panes again");
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

int program(int argc, char **argv) {
    const bool tree_only = argc > 1 && std::string(argv[1]) == "--tree-repaint";
    native::app_wnd root("Motif peer regressions", 20, 20, 660, 400);
    root.on_wnd_create.connect([&root, tree_only] {
        native::app::post([&root, tree_only] {
            try {
                if (tree_only)
                    check_tree_content_repaint(root);
                else
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
