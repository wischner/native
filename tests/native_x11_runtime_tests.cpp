//
// Exercises Athena geometry, native list selection, buffered collection
// painting and live separator input against a real X server.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/x11/globals.h"
#include "../lib/native/toolkits/x11/alert_icons.h"
#include <X11/StringDefs.h>
#include <X11/Xutil.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Command.h>
#include <dlfcn.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
    using namespace native;
    using namespace linux::x11;

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    Widget widget(wnd &owner) { return wnd_bindings.handle_from_object(&owner); }

    size dimensions(wnd &owner) {
        Dimension w = 0, h = 0, border = 0;
        XtVaGetValues(widget(owner), XtNwidth, &w, XtNheight, &h,
            XtNborderWidth, &border, nullptr);
        if (XtIsRealized(widget(owner))) {
            XWindowAttributes attributes{};
            XGetWindowAttributes(cached_display, XtWindow(widget(owner)), &attributes);
            expect(attributes.width == w && attributes.height == h,
                "Xt cached size matches the actual X window");
        }
        return size(w + 2 * border, h + 2 * border);
    }

    unsigned long pixel(Widget target, int x, int y) {
        XImage *image = XGetImage(cached_display, XtWindow(target),
            x, y, 1, 1, AllPlanes, ZPixmap);
        expect(image, "read window pixels");
        const auto result = XGetPixel(image, 0, 0);
        XDestroyImage(image);
        return result;
    }

    unsigned long pixel(wnd &owner, int x, int y) {
        return pixel(widget(owner), x, y);
    }

    // Exercise server hit testing and grabs, not direct XtDispatchEvent.
    class server_input
    {
    public:
        server_input() : library(dlopen("libXtst.so.6", RTLD_NOW)) {
            expect(library, "XTest runtime is required for real pointer tests");
            button = reinterpret_cast<button_proc>(dlsym(library, "XTestFakeButtonEvent"));
            expect(button, "XTest button entry point");
        }
        ~server_input() { dlclose(library); }
        void send(Widget target, int type, int x, int y) {
            XWarpPointer(cached_display, None, XtWindow(target), 0, 0, 0, 0, x, y);
            if (type != MotionNotify)
                button(cached_display, Button1, type == ButtonPress, 0);
            XFlush(cached_display);
        }
        void click(Widget target, int x, int y) {
            send(target, ButtonPress, x, y);
            send(target, ButtonRelease, x, y);
        }
    private:
        using button_proc = int (*)(Display *, unsigned int, Bool, unsigned long);
        void *library;
        button_proc button;
    };

    void border(wnd &owner) {
        const auto d = dimensions(owner);
        const auto ink = BlackPixel(cached_display, DefaultScreen(cached_display));
        expect(pixel(owner, d.w / 2, 0) == ink &&
            pixel(owner, d.w / 2, d.h - 1) == ink &&
            pixel(owner, 0, d.h / 2) == ink &&
            pixel(owner, d.w - 1, d.h / 2) == ink,
            "all four collection/table borders remain visible");
    }

    class rows final : public table_model
    {
        std::size_t row_count() const override { return 100; }
        table_row_id row_id(std::size_t row) const override { return row + 1; }
        table_cell cell(std::size_t, table_column_id) const override { return {"Row", nullptr}; }
    };

    class colored_table final : public table_view
    {
    public:
        using table_view::table_view;
        void draw_row_background(gpx &g, theme &, table_row_id, std::size_t,
            const rect &r, const theme::state &) override {
            g.set_ink(rgba(190, 190, 190, 255)).draw_rect(r, true);
        }
    };

    class test_window final : public app_wnd
    {
    public:
        test_window() : app_wnd("X11 regressions", 10, 10, 740, 650),
            choices({"Short", "Longer"}, 10, 10, 160, 90),
            first({"First"}, 0, 0, 240, 150),
            second({"Second"}, 0, 0, 240, 150),
            third({"Third"}, 0, 0, 240, 150),
            sections(180, 10, 250, 220),
            tree({tree_node("Root", 1, {tree_node("Leaf", 2)}, true)}, 450, 10, 250, 220),
            table(10, 250, 440, 183),
            left({"Left"}), right({"Right"}),
            split(left, right, split_orientation::horizontal, 10, 450, 440, 140),
            layout(*this, "Layout", 770, 10, 300, 200),
            button("Fit", 0, 0, 100, 30),
            chrome(*this, "Input and chrome", 770, 250, 300, 200),
            combo({"One", "Two"}, combo_box_style::editable, 20, 20, 180, 28),
            page({"Body"}), tabs(450, 250, 250, 150) {
            menu << "File" << menu_items("Exit");
            tabs.add_item("Selected", page);
            sections.add_item("Shapes", first);
            sections.add_item("Colors", second);
            sections.add_item("Backgrounds", third);
            table_column column;
            column.id = 1; column.title = "Name"; column.width = 120;
            table.set_columns({column}).set_model(&model);
            choices.set_selected_index(0);
            std::fill_n(artwork->pixels(), 16 * 16, rgba(255, 34, 51, 255));
            icons.set_items({{"Color", artwork}});
            combo.on_selection_change.connect([this](int) {
                ++combo_selections; return false;
            });
            on_wnd_create.connect([this] { return create_children(); });
            sections.on_wnd_paint.connect([this](wnd_paint_event) { ++paints; return false; });
            tree.on_wnd_paint.connect([this](wnd_paint_event) { ++paints; return false; });
        }

        int failures = 0;

    private:
        list choices, first, second, third;
        accordion sections;
        tree_view tree;
        rows model;
        colored_table table;
        list left, right;
        split_view split;
        modeless_wnd layout;
        native::button button;
        modeless_wnd chrome;
        combo_box combo;
        server_input input;
        int combo_selections = 0;
        std::shared_ptr<img> artwork = std::make_shared<img>(16, 16);
        icon_view icons{{}, 650, 450, 80, 100};
        canvas picture{550, 450, 60, 40};
        list page;
        tab_view tabs;
        int phase = 0, paints = 0, idle_paints = 0;

        bool create_children() {
            // The gallery queries its status metric in on_create, before
            // the shell is realized. This used to throw and abort the app.
            expect(!XtIsRealized(widget(*this)), "test starts before realization");
            expect(theme::create(get_gpx())->get_status_bar_height() > 0,
                "theme lookup before show");
            for (wnd *child : std::initializer_list<wnd *>{&choices, &sections,
                    &tree, &table, &split, &tabs, &icons, &picture}) {
                child->set_parent(this); child->create(); child->show();
            }
            picture.on_wnd_paint.connect([this](wnd_paint_event event) {
                event.g.clear(rgba(255, 255, 255, 255)).draw_img(*artwork, point(0, 0));
                return true;
            });
            picture.invalidate();
            layout.create();
            button.set_parent(&layout); button.create(); button.show();
            auto grid = std::make_unique<grid_layout_manager>(1, 1);
            grid->add(button, 0, 0, 1, 1, 8);
            layout.set_layout(std::move(grid)); layout.show();
            chrome.create();
            expect(theme::create(chrome.get_gpx())->get_status_bar_height() > 0,
                "modeless theme lookup before show");
            combo.set_parent(&chrome); combo.create(); combo.show(); chrome.show();
            later();
            return true;
        }

        void later() {
            XtAppAddTimeOut(app_instance, 120, [](XtPointer data, XtIntervalId *) {
                static_cast<test_window *>(data)->check();
            }, this);
        }

        void check_combo_popup() {
            auto *binding = combo_box_bindings.object_from_handle(&combo);
            XWindowAttributes attributes{};
            XGetWindowAttributes(cached_display, XtWindow(binding->menu), &attributes);
            expect(attributes.map_state == IsViewable && attributes.border_width == 1 &&
                attributes.width + 2 == combo.get_dimensions().w,
                "combo popup has a border and full field width");
            Position field_x = 0, field_y = 0, menu_x = 0, menu_y = 0;
            XtTranslateCoords(binding->root, 0, 0, &field_x, &field_y);
            XtTranslateCoords(binding->menu, 0, 0, &menu_x, &menu_y);
            expect(menu_x == field_x + 1 && menu_y == field_y + combo.get_dimensions().h + 1,
                "combo popup is anchored beneath the complete field");
        }

        void choose_combo(int index) {
            auto *binding = combo_box_bindings.object_from_handle(&combo);
            WidgetList entries = nullptr;
            Cardinal count = 0;
            XtVaGetValues(binding->menu, XtNchildren, &entries, XtNnumChildren, &count, nullptr);
            expect(index < int(count), "combo entry exists");
            Position y = 0; Dimension height = 1;
            XtVaGetValues(entries[index], XtNy, &y, XtNheight, &height, nullptr);
            input.click(binding->menu, 12, y + height / 2);
        }

        void check_tab_join() {
            const rect tab = tabs.get_tab_bounds(0);
            const rect content = tabs.get_content_bounds();
            int x = tab.x1() + tab.d.w / 2;
            int y = tab.y1() + tab.d.h / 2;
            switch (tabs.get_tab_placement()) {
            case tab_placement::top: y = content.y1() - 1; break;
            case tab_placement::bottom: y = content.y2(); break;
            case tab_placement::left: x = content.x1() - 1; break;
            case tab_placement::right: x = content.x2(); break;
            }
            expect(pixel(tabs, x, y) == WhitePixel(cached_display, 0),
                "selected tab has no separating line in any orientation");
        }

        void check_message() {
            bool centered = false;
            std::pair<test_window *, bool *> state{this, &centered};
            XtAppAddTimeOut(app_instance, 120, [](XtPointer data, XtIntervalId *) {
                auto &state = *static_cast<std::pair<test_window *, bool *> *>(data);
                Widget dialog = XtNameToWidget(shell_bindings.handle_from_object(state.first),
                    const_cast<char *>("*message_box.message"));
                if (!dialog) return;
                WidgetList children = nullptr; Cardinal count = 0;
                Dimension width = 0;
                XtVaGetValues(dialog, XtNchildren, &children, XtNnumChildren, &count,
                    XtNwidth, &width, nullptr);
                int left = width, right = 0, buttons = 0;
                Widget first = nullptr;
                for (Cardinal i = 0; i < count; ++i) {
                    if (!XtIsSubclass(children[i], commandWidgetClass)) continue;
                    Position x = 0; Dimension w = 0, border = 0;
                    XtVaGetValues(children[i], XtNx, &x, XtNwidth, &w,
                        XtNborderWidth, &border, nullptr);
                    left = std::min(left, int(x));
                    right = std::max(right, int(x + w + 2 * border));
                    if (!first) first = children[i];
                    ++buttons;
                }
                *state.second = buttons == 3 && std::abs(left - (int(width) - right)) <= 1;
                Widget shell = XtParent(dialog);
                Widget parent = shell_bindings.handle_from_object(state.first);
                XWindowAttributes owner_bounds{}, bounds{};
                XGetWindowAttributes(cached_display, XtWindow(parent), &owner_bounds);
                XGetWindowAttributes(cached_display, XtWindow(shell), &bounds);
                Position x = 0, y = 0, owner_x = 0, owner_y = 0;
                XtTranslateCoords(shell, 0, 0, &x, &y);
                XtTranslateCoords(parent, 0, 0, &owner_x, &owner_y);
                const int screen_width = WidthOfScreen(XtScreen(shell));
                const int screen_height = HeightOfScreen(XtScreen(shell));
                const bool visible = owner_bounds.map_state == IsViewable;
                const int center_x = visible ? owner_x + owner_bounds.width / 2 : screen_width / 2;
                const int center_y = visible ? owner_y + owner_bounds.height / 2 : screen_height / 2;
                const int w = bounds.width + 2 * bounds.border_width;
                const int h = bounds.height + 2 * bounds.border_width;
                *state.second &= x - bounds.border_width ==
                    std::clamp(center_x - w / 2, 0, std::max(0, screen_width - w)) &&
                    y - bounds.border_width ==
                    std::clamp(center_y - h / 2, 0, std::max(0, screen_height - h));
                if (!*state.second)
                    std::cerr << "Message placement: " << x << ',' << y
                        << " owner " << owner_x << ',' << owner_y
                        << " size " << w << ',' << h << " visible " << visible << '\n';
                if (first) XtCallCallbacks(first, XtNcallback, nullptr);
            }, &state);
            const auto result = message_box::show(*this,
                "A sufficiently wide message to check the three-button row is centered.",
                "Centered buttons", message_box_buttons::yes_no_cancel,
                message_box_icon::question);
            expect(centered && result == message_box_result::yes,
                "message shell and buttons are centered, screen-clamped and return the chosen result");
        }

        void check() {
            try {
                if (phase == 0) {
                    expect(pixel(picture, 5, 5) == 0xff2233 && artwork->pixels()[0].g == 34,
                        "canvas imagery and source pixels retain full color");
                    XImage *image = XGetImage(cached_display, XtWindow(widget(icons)),
                        0, 0, 80, 100, AllPlanes, ZPixmap);
                    int dark = 0;
                    bool monochrome = true;
                    for (int y = 0; y < 100; ++y)
                        for (int x = 0; x < 80; ++x) {
                            const auto color = XGetPixel(image, x, y);
                            dark += color == BlackPixel(cached_display, 0);
                            monochrome &= color == BlackPixel(cached_display, 0) ||
                                color == WhitePixel(cached_display, 0);
                        }
                    XDestroyImage(image);
                    expect(monochrome && dark >= 256, "collection icons render only black and white");
                    expect(theme::create(get_gpx())->get_content_alternate_background_color().r == 255,
                        "Athena alternating rows retain white paper");
                    for (auto icon : {message_box_icon::information, message_box_icon::warning,
                            message_box_icon::question, message_box_icon::error}) {
                        const Pixmap bitmap = create_message_icon(cached_display,
                            DefaultRootWindow(cached_display), icon);
                        Window root = None; int x = 0, y = 0;
                        unsigned int w = 0, h = 0, border = 0, depth = 0;
                        expect(bitmap && XGetGeometry(cached_display, bitmap, &root, &x, &y,
                            &w, &h, &border, &depth) && w == 32 && h == 32 && depth == 1,
                            "all copied alert icons are valid 32-pixel monochrome bitmaps");
                        XFreePixmap(cached_display, bitmap);
                    }
                    auto *bar = menu_bindings.object_from_handle(menu.id());
                    Dimension width = 0, height = 0;
                    XtVaGetValues(bar->menu_bar, XtNwidth, &width, XtNheight, &height, nullptr);
                    expect(width == 740 && pixel(bar->menu_bar, width - 4, height - 1) ==
                        WhitePixel(cached_display, 0), "menu bar has no full-width bottom line");
                    Widget popup = XtNameToWidget(bar->menu_bar,
                        const_cast<char *>("*menu_button.menu"));
                    expect(popup, "main menu popup exists");
                    Dimension popup_border = 0;
                    XtVaGetValues(popup, XtNborderWidth, &popup_border, nullptr);
                    expect(popup_border == 1, "main menu popup retains its own border");
                    expect(dimensions(sections).w == 250 && dimensions(sections).h == 220,
                        "accordion must not shrink to first child");
                    expect(sections.get_header_bounds(2).y2() <= 220,
                        "all accordion group headers fit");
                    border(sections); border(tree); border(table);
                    expect(pixel(sections, 120, sections.get_header_bounds(1).y1()) ==
                        BlackPixel(cached_display, 0), "header below expanded section has a top rule");
                    expect(dimensions(combo).w == 180 && dimensions(combo).h == 28,
                        "composite combo keeps its requested size");
                    auto *combo_state = combo_box_bindings.object_from_handle(&combo);
                    Dimension arrow_height = 0;
                    XtVaGetValues(combo_state->button, XtNheight, &arrow_height, nullptr);
                    expect(arrow_height == 26, "combo arrow fills field height");
                    const auto tab = tabs.get_tab_bounds(0);
                    int white = 0;
                    for (int y = tab.y1() + 3; y < tab.y2() - 3; ++y)
                        for (int x = tab.x1() + 4; x < tab.x2() - 4; ++x)
                            white += pixel(tabs, x, y) == WhitePixel(cached_display, 0);
                    expect(white > 10, "selected tab text contrasts with its background");
                    expect(pixel(tabs, tab.x1() + tab.d.w / 2, tab.y1() + 2) ==
                        WhitePixel(cached_display, 0), "selected X11 tab stays white");
                    check_tab_join();
                    expect(pixel(choices, 150, 5) == BlackPixel(cached_display, 0),
                        "native List selection spans its row");
                    const auto metrics = theme::create(get_gpx())->defaults();
                    expect(pixel(table, 440 - metrics.scrollbar_extent, 150) ==
                        BlackPixel(cached_display, 0), "scrollbar track has a left separator");
                    expect(pixel(table, 220, 181) != WhitePixel(cached_display, 0),
                        "rows occupy the bottom of the table viewport");
                    layout.set_dimensions(size(500, 300));
                    sections.get_item(1).set_expanded(true);
                    tree.set_selected_item(2);
                } else if (phase == 1) {
                    expect(dimensions(button).w == 484 && dimensions(button).h == 284,
                        "grid grows native child");
                    layout.set_dimensions(size(200, 120));
                    combo.set_dimensions(size(220, 32));
                    choices.set_dimensions(size(130, 90));
                    idle_paints = paints;
                } else if (phase == 2) {
                    expect(dimensions(button).w == 184 && dimensions(button).h == 104,
                        "grid shrinks native child");
                    expect(paints <= idle_paints + 2, "collections must settle without redraw loop");
                    Dimension arrow_height = 0;
                    XtVaGetValues(combo_box_bindings.object_from_handle(&combo)->button,
                        XtNheight, &arrow_height, nullptr);
                    expect(arrow_height == 30, "combo children follow resized host");
                    expect(pixel(choices, 120, 5) == BlackPixel(cached_display, 0),
                        "full-row selection follows resize");
                    const auto divider = split.get_splitter_bounds();
                    expect(pixel(split, divider.x1() + 1, 20) == pixel(*this, 470, 450),
                        "separator uses window background");
                    input.send(widget(split), ButtonPress, divider.x1(), 30);
                } else if (phase == 3) {
                    input.send(widget(split), MotionNotify, 300, 30);
                } else if (phase == 4) {
                    expect(split.get_ratio() > 0.65f && dimensions(left).w > 280 &&
                        dimensions(right).w < 155, "whole separator drag resizes both panes live");
                    auto *panes = split_view_bindings.object_from_handle(&split);
                    for (Widget pane : {panes->first, panes->second}) {
                        Dimension width = 1, height = 1;
                        XtVaGetValues(pane, XtNwidth, &width, XtNheight, &height, nullptr);
                        expect(pixel(pane, 0, height / 2) == BlackPixel(cached_display, 0) &&
                            pixel(pane, width - 1, height / 2) == BlackPixel(cached_display, 0),
                            "both pane controls retain left and right borders");
                    }
                    input.send(widget(split), ButtonRelease, 300, 30);
                } else if (phase == 5) {
                    auto *binding = combo_box_bindings.object_from_handle(&combo);
                    Position button_x = 0;
                    Dimension text_width = 0;
                    XtVaGetValues(binding->button, XtNx, &button_x, nullptr);
                    XtVaGetValues(binding->text, XtNwidth, &text_width, nullptr);
                    expect(button_x == text_width + 1, "combo separator is one pixel wide");
                    input.click(binding->button, 8, 8);
                } else if (phase == 6) {
                    check_combo_popup(); choose_combo(1);
                } else if (phase == 7) {
                    char *value = nullptr;
                    XtVaGetValues(combo_box_bindings.object_from_handle(&combo)->text,
                        XtNstring, &value, nullptr);
                    expect(value && std::string(value) == "Two" && combo.get_text() == "Two" &&
                        combo.get_selected_index() == 1 && combo_selections == 1,
                        "one popup click updates native text, cache and selection exactly once");
                    combo.set_style(combo_box_style::drop_down_list);
                    input.click(combo_box_bindings.object_from_handle(&combo)->text, 10, 8);
                } else if (phase == 8) {
                    check_combo_popup(); choose_combo(0);
                } else if (phase == 9) {
                    expect(combo.get_text() == "One" && combo_selections == 2,
                        "selection-only field opens and selects with one click");
                    tabs.set_tab_placement(tab_placement::bottom);
                } else if (phase == 10) {
                    check_tab_join(); tabs.set_tab_placement(tab_placement::left);
                } else if (phase == 11) {
                    check_tab_join(); tabs.set_tab_placement(tab_placement::right);
                } else if (phase == 12) {
                    check_tab_join();
                    split.set_orientation(split_orientation::vertical);
                    split.set_dimensions(size(440, 100));
                    split.set_ratio(0);
                } else if (phase == 13) {
                    expect(dimensions(left).h >= 1 && dimensions(right).h >= 1,
                        "minimum native pane backing survives vertical resize and zero ratio");
                    input.click(combo_box_bindings.object_from_handle(&combo)->button, 8, 8);
                } else if (phase == 14) {
                    auto *binding = combo_box_bindings.object_from_handle(&combo);
                    input.send(binding->menu, MotionNotify, 150, 5);
                } else if (phase == 15) {
                    auto *binding = combo_box_bindings.object_from_handle(&combo);
                    WidgetList entries = nullptr; Cardinal count = 0;
                    XtVaGetValues(binding->menu, XtNchildren, &entries, XtNnumChildren, &count, nullptr);
                    expect(count == 2 && XawSimpleMenuGetActiveEntry(binding->menu) == entries[0],
                        "combo hover highlights first item without a held button");
                    Position y = 0; Dimension height = 1;
                    XtVaGetValues(entries[1], XtNy, &y, XtNheight, &height, nullptr);
                    input.send(binding->menu, MotionNotify, 150, y + height / 2);
                } else if (phase == 16) {
                    auto *binding = combo_box_bindings.object_from_handle(&combo);
                    WidgetList entries = nullptr; Cardinal count = 0;
                    XtVaGetValues(binding->menu, XtNchildren, &entries, XtNnumChildren, &count, nullptr);
                    expect(XawSimpleMenuGetActiveEntry(binding->menu) == entries[1] &&
                        combo_selections == 2, "hover changes highlight, not committed selection");
                    choose_combo(1);
                } else if (phase == 17) {
                    check_message();
                } else if (phase == 18) {
                    set_dimensions(size(100, 100));
                    XMoveWindow(cached_display, XtWindow(shell_bindings.handle_from_object(this)),
                        WidthOfScreen(XtScreen(widget(*this))) - 100,
                        HeightOfScreen(XtScreen(widget(*this))) - 100);
                } else if (phase == 19) {
                    check_message();
                } else if (phase == 20) {
                    XtPopdown(shell_bindings.handle_from_object(this));
                    check_message();
                } else {
                    chrome.destroy(); layout.destroy(); destroy();
                    std::cout << "X11 geometry, painting and separator regressions passed\n";
                    return;
                }
                ++phase; later();
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n'; failures = 1;
                chrome.destroy(); layout.destroy(); destroy();
            }
        }
    };
}

int program(int, char **) {
    test_window window;
    const int result = app::run(window);
    return result ? result : window.failures;
}
