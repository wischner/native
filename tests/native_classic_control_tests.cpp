//
// Exercises classic toolkit table modifiers, completion input, joined
// picker fields and native calendars. X backends receive server input;
// GEM uses its AES packet adapters and the same picker action
// callbacks.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/combo_picker.h"
#include "../lib/native/post_backend.h"
#include "../lib/native/wnd_peer.h"
#include "../lib/native/code_render.h"
#include "../lib/native/emulated_tree.h"
#include <array>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <functional>
#if defined(NATIVE_TEST_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#elif defined(NATIVE_TEST_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#include <Xm/TextF.h>
#include <Xm/Form.h>
#include <Xm/ArrowB.h>
#else
#include "../lib/native/toolkits/gemix/globals.h"
#endif
#if !defined(NATIVE_TEST_GEMIX)
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#endif
namespace
{
    using namespace native;
    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
#if !defined(NATIVE_TEST_GEMIX)
    Display *display() {
#if defined(NATIVE_TEST_WMAKER)
        return linux::wmaker::display;
#else
        return linux::openmotif::cached_display;
#endif
    }
    Window drawable(wnd &owner) {
#if defined(NATIVE_TEST_WMAKER)
        return linux::wmaker::drawable(&owner);
#else
        return XtWindow(
            linux::openmotif::wnd_bindings.handle_from_object(&owner));
#endif
    }
#endif
    void pump() {
        for (int i = 0; i < 25; ++i) {
#if defined(NATIVE_TEST_WMAKER)
            XSync(display(), False);
            while (XPending(display())) {
                XEvent event{};
                XNextEvent(display(), &event);
                WMHandleEvent(&event);
            }
            linux::wmaker::dispatch_deferred();
#elif defined(NATIVE_TEST_MOTIF)
            XSync(display(), False);
            while (XtAppPending(linux::openmotif::app_instance))
                XtAppProcessEvent(linux::openmotif::app_instance,
                                  XtIMAll);
#else
            linux::gemix::flush_repaints();
#endif
            detail::drain_posted_work();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
#if !defined(NATIVE_TEST_GEMIX)
    void key(KeySym symbol, bool down) {
        static void *library = dlopen("libXtst.so.6", RTLD_NOW);
        expect(library, "XTest is available");
        using proc = int (*)(Display *, unsigned, Bool, unsigned long);
        auto send =
            reinterpret_cast<proc>(dlsym(library, "XTestFakeKeyEvent"));
        send(display(), XKeysymToKeycode(display(), symbol), down, 0);
    }
    void key(KeySym symbol) {
        key(symbol, true);
        key(symbol, false);
        pump();
    }
    void click(wnd &owner, int x, int y) {
        static void *library = dlopen("libXtst.so.6", RTLD_NOW);
        using proc = int (*)(Display *, unsigned, Bool, unsigned long);
        auto send = reinterpret_cast<proc>(
            dlsym(library, "XTestFakeButtonEvent"));
        XWarpPointer(
            display(), None, drawable(owner), 0, 0, 0, 0, x, y);
        pump();
        send(display(), 1, True, 0);
        pump();
        send(display(), 1, False, 0);
        pump();
    }
#endif
#if !defined(NATIVE_TEST_GEMIX)
    // Capture server-composited child widgets, including native field
    // arrows.
    void snapshot(wnd &owner, const char *variable) {
        const auto path = std::getenv(variable);
        if (!path)
            return;
        const auto target = drawable(owner);
        XWindowAttributes bounds{};
        XGetWindowAttributes(display(), target, &bounds);
        int x = 0, y = 0;
        Window child = None;
        XTranslateCoordinates(display(),
                              target,
                              DefaultRootWindow(display()),
                              0,
                              0,
                              &x,
                              &y,
                              &child);
        auto *pixels = XGetImage(display(),
                                 DefaultRootWindow(display()),
                                 x,
                                 y,
                                 bounds.width,
                                 bounds.height,
                                 AllPlanes,
                                 ZPixmap);
        expect(pixels, "native screenshot is available");
        img capture(bounds.width, bounds.height);
        for (int row = 0; row < bounds.height; ++row)
            for (int column = 0; column < bounds.width; ++column) {
                const auto color = XGetPixel(pixels, column, row);
                capture.pixels()[row * bounds.width + column] =
                    rgba(color >> 16, color >> 8, color, 255);
            }
        XDestroyImage(pixels);
        capture.save(path);
    }
    Window named_window(Window parent,
                        const std::string &name,
                        int depth = 0) {
        if (depth > 5)
            return None;
        char *title = nullptr;
        XFetchName(display(), parent, &title);
        const bool match = title && name == title;
        if (title)
            XFree(title);
        XWindowAttributes attributes{};
        XGetWindowAttributes(display(), parent, &attributes);
        if (match && attributes.map_state == IsViewable)
            return parent;
        Window root = None, ancestor = None, *children = nullptr;
        unsigned count = 0;
        if (!XQueryTree(
                display(), parent, &root, &ancestor, &children, &count))
            return None;
        Window found = None;
        for (unsigned i = 0; i < count && !found; ++i)
            found = named_window(children[i], name, depth + 1);
        if (children)
            XFree(children);
        return found;
    }
    struct dialog_check
    {
        app_wnd &owner;
        std::string title;
        bool done = false, centered = false;
        int attempts = 0;
#if defined(NATIVE_TEST_WMAKER)
        static void inspect(void *data) {
            static_cast<dialog_check *>(data)->inspect();
        }
        void schedule() {
            WMAddTimerHandler(100, inspect, this);
        }
#else
        static void inspect(XtPointer data, XtIntervalId *) {
            static_cast<dialog_check *>(data)->inspect();
        }
        void schedule() {
            XtAppAddTimeOut(
                linux::openmotif::app_instance, 100, inspect, this);
        }
#endif
        void inspect() {
            const auto panel =
                named_window(DefaultRootWindow(display()), title);
            if (!panel && ++attempts < 30) {
                schedule();
                return;
            }
            if (panel) {
                XWindowAttributes a{}, b{};
                XGetWindowAttributes(display(), panel, &a);
                XGetWindowAttributes(display(), drawable(owner), &b);
                int x = 0, y = 0, px = 0, py = 0;
                Window child = None;
                XTranslateCoordinates(display(),
                                      panel,
                                      DefaultRootWindow(display()),
                                      0,
                                      0,
                                      &x,
                                      &y,
                                      &child);
                XTranslateCoordinates(display(),
                                      drawable(owner),
                                      DefaultRootWindow(display()),
                                      0,
                                      0,
                                      &px,
                                      &py,
                                      &child);
                centered =
                    std::abs(x + a.width / 2 - px - b.width / 2) <=
                        24 &&
                    std::abs(y + a.height / 2 - py - b.height / 2) <=
                        24;
                if (!centered)
                    std::cerr << title << " center " << x + a.width / 2
                              << ',' << y + a.height / 2 << " owner "
                              << px + b.width / 2 << ','
                              << py + b.height / 2 << '\n';
                XSetInputFocus(
                    display(), panel, RevertToParent, CurrentTime);
#if defined(NATIVE_TEST_WMAKER)
                if (title != "Center message test") {
                    static void *library =
                        dlopen("libXtst.so.6", RTLD_NOW);
                    using proc = int (*)(
                        Display *, unsigned, Bool, unsigned long);
                    auto send = reinterpret_cast<proc>(
                        dlsym(library, "XTestFakeButtonEvent"));
                    XWarpPointer(display(),
                                 None,
                                 panel,
                                 0,
                                 0,
                                 0,
                                 0,
                                 a.width - 137,
                                 a.height - 21);
                    send(display(), 1, True, 0);
                    send(display(), 1, False, 0);
                    XFlush(display());
                    done = true;
                    return;
                }
#endif
            }
            key(XK_Escape, true);
            key(XK_Escape, false);
            XFlush(display());
            done = true;
        }
    };
    void check_dialog(app_wnd &owner,
                      const std::string &title,
                      const std::function<void()> &show) {
        dialog_check check{owner, title};
        check.schedule();
        show();
        while (!check.done)
            pump();
        pump();
        expect(check.centered,
               "standard modal panel is centered over its parent");
    }
    void dialogs(app_wnd &owner) {
        open_file_dialog open(owner, "Center open test");
        open.set_initial_path("/tmp");
        open.create();
        check_dialog(owner, open.get_title(), [&] {
            open.show();
        });
        expect(!open.get_modal_active(),
               "Escape cancels the native open panel");
        save_file_dialog save(owner, "Center save test");
        save.set_initial_path("/tmp");
        save.create();
        check_dialog(owner, save.get_title(), [&] {
            save.show();
        });
        directory_dialog folder(owner, "Center folder test");
        folder.set_initial_path("/tmp");
        folder.create();
        check_dialog(owner, folder.get_title(), [&] {
            folder.show();
        });
        check_dialog(owner, "Center message test", [&] {
            message_box::show(owner,
                              "Centered native message",
                              "Center message test",
                              message_box_buttons::ok_cancel,
                              message_box_icon::information);
        });
    }
#endif
    struct model final : table_model
    {
        std::size_t row_count() const override {
            return 10;
        }
        table_row_id row_id(std::size_t row) const override {
            return row + 1;
        }
        table_cell cell(std::size_t row,
                        table_column_id) const override {
            return {std::to_string(row + 1), nullptr};
        }
    };
    void attach(wnd &child, wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
#if defined(NATIVE_TEST_MOTIF)
    // Deliver real mouse clicks to native spin buttons with each text
    // component selected. Relayout must not hide these child widgets.
    void motif_time_checks(time_picker &owner) {
        owner.set_seconds_visible(true).set_time({12, 30, 10});
        owner.set_bounds(rect(370, 120, 300, 34));
        auto &field = detail::picker_field(owner);
        auto root =
            linux::openmotif::wnd_bindings.handle_from_object(&field);
        expect(XmIsForm(root),
               "Motif picker owns a joined native field, not an empty "
               "list popup");
        auto text =
            XtNameToWidget(root, const_cast<char *>("comboText"));
        auto up =
            XtNameToWidget(root, const_cast<char *>("pickerArrowUp"));
        auto down =
            XtNameToWidget(root, const_cast<char *>("pickerArrowDown"));
        expect(text && up && down && XmIsArrowButton(up) &&
                   XmIsArrowButton(down),
               "Motif time field owns two native arrow buttons");
        XRaiseWindow(display(), drawable(field));
        pump();
        for (auto button : {up, down}) {
            XWindowAttributes bounds{};
            XGetWindowAttributes(display(), XtWindow(button), &bounds);
            expect(bounds.map_state == IsViewable && bounds.width > 0 &&
                       bounds.height > 0,
                   "both time buttons stay visible after raising and "
                   "resizing the field");
        }
        const time_of_day values[] = {
            {13, 30, 10}, {12, 45, 10}, {12, 30, 11}};
        for (int component = 0; component < 3; ++component) {
            const int first = component * 3;
            XmTextFieldSetSelection(
                text, first, first + 2, CurrentTime);
            XmTextFieldSetInsertionPosition(text, first + 2);
            click(field,
                  field.get_dimensions().w - 10,
                  field.get_dimensions().h / 4);
            expect(owner.get_time() == values[component],
                   "Motif up button changes the selected hour, minute "
                   "or second");
            click(field,
                  field.get_dimensions().w - 10,
                  field.get_dimensions().h * 3 / 4);
            expect(owner.get_time() == time_of_day{12, 30, 10},
                   "Motif down button retains the selected component "
                   "across repeated clicks");
        }
        XmTextFieldClearSelection(text, CurrentTime);
        XmTextFieldSetInsertionPosition(text, 7);
        click(field,
              field.get_dimensions().w - 10,
              field.get_dimensions().h / 4);
        expect(owner.get_time() == time_of_day{12, 30, 11},
               "Motif time stepping also follows the caret without a "
               "selection");
    }
#endif
    void checks(app_wnd &root) {
        model data;
        modeless_wnd window(
            root, "Classic control input", 40, 40, 780, 640);
        table_view table(10, 10, 330, 170);
        code_edit editor("abc", 10, 200, 330, 170);
        combo_box combo({"10 mm", "25 mm", "50 mm"},
                        combo_box_style::editable,
                        370,
                        10,
                        240,
                        30);
        date_picker dates(date{2026, 12, 24}, rect(370, 70, 240, 30));
        time_picker times(time_of_day{12, 45, 0},
                          rect(370, 120, 240, 30));
        panel page;
        list standard({"Controls", "Images", "Fonts", "Clipboard"},
                      10,
                      400,
                      155,
                      120);
        standard.set_selected_index(0);
        canvas primitive(180, 400, 155, 120);
        primitive.on_wnd_paint.connect([&](wnd_paint_event event) {
            auto appearance = theme::create(event.g);
            appearance->draw_list(primitive.get_client_bounds(),
                                  standard.get_items(),
                                  0,
                                  {});
            return true;
        });
        settings_sidebar sidebar(370, 200, 390, 400);
        auto image = std::make_shared<img>(16, 16);
        image->get_gpx().clear(rgba(0, 0, 0, 255));
        sidebar.set_search_visible(true).set_sidebar_width(200);
        sidebar << settings_page(1, "General", page, image, "Settings");
        table.set_model(&data)
            .set_columns({table_column{1, "Value", 260}})
            .set_header_visible(false)
            .set_row_height(24)
            .set_selection_mode(table_selection_mode::multiple);
        dates.set_disabled_dates({date{2026, 12, 25}});
        times.set_minute_step(15);
        window.create();
        window.show();
        for (auto *child : std::array<wnd *, 6>{
                 &table, &editor, &combo, &dates, &times, &sidebar})
            attach(*child, window);
#if !defined(NATIVE_TEST_MOTIF)
        attach(standard, window);
        attach(primitive, window);
#endif
        pump();
#if !defined(NATIVE_TEST_GEMIX)
#if defined(NATIVE_TEST_MOTIF)
        XRaiseWindow(display(),
                     XtWindow(linux::openmotif::shell_bindings
                                  .handle_from_object(&window)));
#else
        XRaiseWindow(display(), drawable(window));
#endif
        XSetInputFocus(
            display(), drawable(window), RevertToParent, CurrentTime);
        pump();
#endif
#if defined(NATIVE_TEST_GEMIX)
        const auto origin = linux::gemix::origin_in_root(table);
        linux::gemix::activate_collection(
            &window, point(origin.x + 30, origin.y + 12), 0);
        linux::gemix::activate_collection(
            &window, point(origin.x + 30, origin.y + 36), 4);
#else
        click(table, 30, 12);
        key(XK_Control_L, true);
        click(table, 30, 36);
        key(XK_Control_L, false);
        pump();
#endif
        if (table.get_selected_rows().size() != 2)
            std::cerr << "selected rows after Ctrl: "
                      << table.get_selected_rows().size() << '\n';
        expect(table.get_selected_rows().size() == 2,
               "Ctrl-click adds a classic table selection");
#if defined(NATIVE_TEST_GEMIX)
        linux::gemix::activate_collection(
            &window, point(origin.x + 30, origin.y + 84), 1);
#else
        key(XK_Shift_L, true);
        click(table, 30, 84);
        key(XK_Shift_L, false);
        pump();
#endif
        expect(table.get_selected_rows().size() >= 3,
               "Shift-click extends a classic table selection");
#if !defined(NATIVE_TEST_GEMIX)
        click(editor, 100, 12);
        const auto before = editor.get_caret_offset();
        key(XK_Left);
        expect(editor.get_caret_offset() < before,
               "classic code arrow moves the caret");
        std::string accepted;
        editor.on_complete.connect([&](completion_item item) {
            accepted = item.label;
            return true;
        });
        // Leave focus on a different control before opening completion.
        click(combo, 20, 12);
        editor.show_completion({{"One", "1", ""}, {"Two", "2", ""}});
        pump();
        key(XK_Down);
        key(XK_Return);
        expect(accepted == "Two",
               "completion receives arrows and Enter after opening "
               "from another control");
        accepted.clear();
        editor.show_completion({{"One", "1", ""}, {"Two", "2", ""}});
        pump();
        auto *geometry =
            detail::peer_state<detail::code_completion_geometry>(
                editor);
        expect(geometry, "completion records pointer geometry");
        click(editor,
              geometry->bounds.x1() + 10,
              geometry->bounds.y1() + geometry->row_height + 4);
        expect(accepted == "Two",
               "completion accepts its painted mouse row");
        editor.show_completion({{"One", "1", ""}});
        pump();
        key(XK_Escape);
        expect(!editor.get_completion_visible(),
               "Escape closes classic completion");
        combo.set_text("");
#if defined(NATIVE_TEST_WMAKER)
        auto *state =
            linux::wmaker::combo_box_bindings.object_from_handle(
                &combo);
        WMSetFocusToWidget(state->field);
#else
        Widget field = nullptr;
        XtVaGetValues(
            linux::openmotif::wnd_bindings.handle_from_object(&combo),
            XmNtextField,
            &field,
            nullptr);
        XmProcessTraversal(field, XmTRAVERSE_CURRENT);
#endif
        pump();
        for (auto symbol : {XK_5, XK_0, XK_space, XK_m, XK_m})
            key(symbol);
        expect(
            combo.get_text() == "50 mm" &&
                combo.get_selected_index() == 2,
            "classic editable combo typing synchronizes its selection");
#else
        auto *state =
            linux::gemix::combo_box_bindings.object_from_handle(&combo);
        expect(state && state->editor,
               "GEM editable combo owns a real text editor");
        combo.set_text("");
        auto *entry =
            linux::gemix::text_edit_bindings.object_from_handle(
                state->editor.get());
        entry->focused = true;
        for (const auto character : std::string("50 mm"))
            linux::gemix::handle_text_edit_key(&window, 0, character);
        expect(combo.get_text() == "50 mm" &&
                   combo.get_selected_index() == 2,
               "GEM editable text synchronizes its combo selection");
#endif
#if defined(NATIVE_TEST_MOTIF)
        dates.set_bounds(rect(370, 70, 300, 34)); pump();
#endif
        auto &date_field = detail::picker_field(dates);
#if defined(NATIVE_TEST_MOTIF)
        XRaiseWindow(display(), drawable(date_field));
        pump();
        expect(
            XmIsForm(linux::openmotif::wnd_bindings.handle_from_object(
                &date_field)),
            "Motif date trigger cannot open an empty native combo "
            "list");
#endif
        auto *date_actions =
            detail::peer_state<detail::combo_picker_actions>(
                date_field);
        date_field.set_text("25.12.2026");
        date_actions->commit();
        expect(dates.get_date() == date{2026, 12, 24} &&
                   date_field.get_text() == "24.12.2026",
               "dot-date entry rejects an excluded holiday");
        date_field.set_text("26.12.2026");
        date_actions->commit();
        expect(dates.get_date() == date{2026, 12, 26},
               "dot-date entry commits a valid date");
#if defined(NATIVE_TEST_GEMIX)
        date_actions->open();
#else
        click(date_field,
              date_field.get_dimensions().w - 10,
              date_field.get_dimensions().h / 2);
#endif
        pump();
        auto *calendar = detail::picker_calendar(dates);
        expect(calendar && calendar->get_created(),
               "date arrow opens a real calendar window");
#if !defined(NATIVE_TEST_GEMIX)
        snapshot(*calendar, "NATIVE_CLASSIC_CALENDAR_SNAPSHOT");
#endif
#if defined(NATIVE_TEST_GEMIX)
        date_actions->open();
#else
        click(date_field,
              date_field.get_dimensions().w - 10,
              date_field.get_dimensions().h / 2);
#endif
        pump();
        expect(!calendar->get_created(),
               "second date-arrow click closes its calendar");
        date_actions->open();
        pump();
        expect(calendar->get_created(),
               "a closed calendar can be reopened");
        // December 2026 begins on Tuesday. Its 27th is the fifth
        // Sunday.
        const int cell = (calendar->get_dimensions().w - 4) / 7;
        const int row_height = (calendar->get_dimensions().h - 4) / 8;
        auto *day = dynamic_cast<button *>(detail::deepest_at(
            *calendar,
            point(2 + cell / 2, 2 + row_height * 6 + row_height / 2)));
        expect(day, "calendar exposes native day buttons");
#if defined(NATIVE_TEST_MOTIF)
        XWindowAttributes day_bounds{};
        XGetWindowAttributes(display(), drawable(*day), &day_bounds);
        expect(
            day_bounds.map_state == IsViewable,
            "Motif dropdown contains visible native calendar buttons");
#endif
#if defined(NATIVE_TEST_GEMIX)
        day->on_click.emit();
#else
        click(*day,
              day->get_dimensions().w / 2,
              day->get_dimensions().h / 2);
#endif
        pump();
        expect(!calendar->get_created() &&
                   dates.get_date() == date{2026, 12, 27},
               "calendar native day selection updates and closes the "
               "picker");
        auto &time_field = detail::picker_field(times);
        time_field.set_text("12:45");
        auto *time_actions =
            detail::peer_state<detail::combo_picker_actions>(
                time_field);
        time_actions->commit();
#if defined(NATIVE_TEST_GEMIX)
        time_actions->step(1);
#else
        click(time_field,
              time_field.get_dimensions().w - 10,
              time_field.get_dimensions().h / 4);
#endif
        pump();
        expect(times.get_time() == time_of_day{13, 0, 0},
               "time up arrow applies the configured minute step");
        expect(time_field.get_text() == "13:00",
               "time field keeps joined colon text");
#if defined(NATIVE_TEST_MOTIF)
        motif_time_checks(times);
#endif
        sidebar.set_search_text("missing");
        pump();
        sidebar.set_search_text("");
        pump();
        expect(page.get_created(),
               "clearing classic settings search restores its page");
#if !defined(NATIVE_TEST_GEMIX)
        snapshot(window, "NATIVE_CLASSIC_CONTROL_SNAPSHOT");
        dialogs(window);
#endif
        window.destroy();
        pump();
    }
} // namespace
int program(int, char **) {
    native::app_wnd root("Classic controls", 10, 10, 840, 700);
    int failures = 0;
    root.on_wnd_create.connect([&] {
        native::app::post([&] {
            try {
                checks(root);
                std::cout << "Classic control checks passed\n";
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n';
                failures = 1;
            }
            root.destroy();
        });
        return true;
    });
    return native::app::run(root) || failures ? 1 : 0;
}
