//
// Exercises stock Athena scrollbar peers and their portable range mapping.
// Uses real XTest middle-button dragging as well as endpoint callbacks.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/x11/globals.h"
#include "../lib/native/toolkits/x11/scrollbars.h"
#include <X11/StringDefs.h>
#include <X11/Xaw/Scrollbar.h>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>

namespace
{
    using namespace native;
    using namespace linux::x11;

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    class model final : public table_model
    {
        std::size_t row_count() const override { return 1000000; }
        table_row_id row_id(std::size_t row) const override { return row + 1; }
        table_cell cell(std::size_t, table_column_id) const override { return {"Row", nullptr}; }
    };

    class test_window final : public app_wnd
    {
    public:
        test_window() : app_wnd("Native Athena scrollbars", 0, 0, 1000, 700) {
            table_column column;
            column.id = 1; column.title = "Wide column"; column.width = 700;
            table.set_columns({column}).set_model(&rows).set_fill_last_column(false);
            std::vector<icon_view_item> items;
            for (int i = 0; i < 80; ++i) items.push_back({"Item", nullptr});
            icons.set_items(items);
            sections.add_item("Items", icons);
            sections.add_item("Other", other);
            for (int i = 1; i <= 120; ++i) tree.add_item(tree_node("Node", i));
            surface.set_content_bounds({-100, -200, 4000, 3000});
            on_wnd_create.connect([this] {
                for (wnd *child : std::initializer_list<wnd *>{&table, &sections, &tree, &surface}) {
                    child->set_parent(this); child->create(); child->show();
                }
                surface.on_wnd_paint.connect([](wnd_paint_event event) {
                    event.g.clear(rgba(255, 255, 255, 255)); return true;
                });
                later(); return true;
            });
        }

        int failures = 0;

        ~test_window() override { if (input_library) dlclose(input_library); }

    private:
        model rows;
        table_view table{10, 10, 300, 220};
        icon_view icons;
        list other{{"Other"}};
        accordion sections{330, 10, 260, 240};
        tree_view tree{{}, 610, 10, 240, 240};
        canvas surface{10, 280, 300, 180};
        int phase = 0;
        void *input_library = nullptr;

        xaw_scrollbars &bars(wnd &owner) {
            auto *state = native::detail::peer_state<xaw_scrollbars>(owner);
            expect(state, "scrollbar state belongs to window peer");
            return *state;
        }

        void later() {
            XtAppAddTimeOut(app_instance, 120, [](XtPointer data, XtIntervalId *) {
                static_cast<test_window *>(data)->check();
            }, this);
        }

        void pointer(Widget bar, int y, int press) {
            using button_proc = int (*)(Display *, unsigned int, Bool, unsigned long);
            if (!input_library) input_library = dlopen("libXtst.so.6", RTLD_NOW);
            expect(input_library, "XTest runtime");
            auto button = reinterpret_cast<button_proc>(dlsym(input_library, "XTestFakeButtonEvent"));
            expect(button, "XTest button entry point");
            XWarpPointer(cached_display, None, XtWindow(bar), 0, 0, 0, 0, 6, y);
            if (press >= 0) button(cached_display, Button2, press, 0);
            XFlush(cached_display);
        }

        void check() {
            try {
                if (phase == 0) {
                    for (wnd *owner : std::initializer_list<wnd *>{&table, &icons, &tree, &surface}) {
                        auto &axis = bars(*owner).vertical;
                        expect(axis.widget && XtIsSubclass(axis.widget, scrollbarWidgetClass) &&
                            XtIsManaged(axis.widget), "overflow uses a real managed Xaw Scrollbar");
                        float end = 1;
                        XtCallCallbacks(axis.widget, XtNjumpProc, &end);
                    }
                    for (wnd *owner : std::initializer_list<wnd *>{&table, &surface}) {
                        auto &axis = bars(*owner).horizontal;
                        expect(axis.widget && XtIsSubclass(axis.widget, scrollbarWidgetClass),
                            "horizontal overflow uses native Athena scrolling");
                        float end = 1;
                        XtCallCallbacks(axis.widget, XtNjumpProc, &end);
                    }
                } else if (phase == 1) {
                    expect(table.get_vertical_scroll_row() == bars(table).vertical.total - bars(table).vertical.page,
                        "million-row native thumb reaches exact final page");
                    expect(table.get_horizontal_scroll_offset() > 300, "horizontal table scrolling updates cache");
                    expect(icons.get_scroll_offset() > 0 && tree.get_scroll_offset() > 0,
                        "accordion icons and tree respond to native scrolling");
                    expect(surface.get_scroll_position().x > 3000 && surface.get_scroll_position().y > 2000,
                        "canvas origin and large native ranges map correctly");
                    float start = 0;
                    XtCallCallbacks(bars(table).vertical.widget, XtNjumpProc, &start);
                } else if (phase == 2) {
                    pointer(bars(table).vertical.widget, 20, 1);
                } else if (phase == 3) {
                    pointer(bars(table).vertical.widget, bars(table).vertical.length - 1, -1);
                } else if (phase == 4) {
                    expect(table.get_vertical_scroll_row() > 800000,
                        "real middle-button thumb dragging changes the virtual viewport");
                    pointer(bars(table).vertical.widget, bars(table).vertical.length - 1, 0);
                } else if (phase == 5) {
                    table.set_vertical_scrollbar_policy(scrollbar_policy::never)
                        .set_horizontal_scrollbar_policy(scrollbar_policy::never);
                    icons.set_items({}); tree.clear_items();
                    surface.set_content_bounds({0, 0, 1, 1});
                } else {
                    for (wnd *owner : std::initializer_list<wnd *>{&table, &icons, &tree, &surface})
                        expect(!XtIsManaged(bars(*owner).vertical.widget),
                            "native scrollbar hides when overflow or policy no longer requires it");
                    destroy();
                    std::cout << "Native Athena scrollbar tests passed\n";
                    return;
                }
                ++phase; later();
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n'; failures = 1; destroy();
            }
        }
    };
}

int program(int, char **) {
    test_window window;
    const int result = app::run(window);
    return result ? result : window.failures;
}
