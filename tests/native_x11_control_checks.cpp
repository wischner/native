//
// Checks actual Athena text pixels, clipboard/typing, popup selection,
// picker buttons, settings spacing and nested-grid resize reversal.
// Extends the existing X11 runtime fixture without another test
// program.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/x11/globals.h"
#include "../lib/native/toolkits/x11/scale.h"
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xutil.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <stdexcept>

namespace
{
    using namespace native;
    using namespace linux::x11;

    // Fail with the particular interaction that regressed.
    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }

    // Inspect the server cursor rather than just the cached API shape.
    std::vector<unsigned long> cursor_pixels() {
        static auto *library = dlopen("libXfixes.so.3", RTLD_NOW);
        using proc = XFixesCursorImage *(*)(Display *);
        auto capture = reinterpret_cast<proc>(
            dlsym(library, "XFixesGetCursorImage"));
        expect(capture, "XFixes provides cursor-image checks");
        auto *image = capture(cached_display);
        expect(image, "read the server cursor");
        std::vector<unsigned long> pixels(
            image->pixels,
            image->pixels + image->width * image->height);
        XFree(image);
        return pixels;
    }

    // Drain server notifications without recursively running test
    // timers.
    void pump() {
        XSync(cached_display, False);
        for (int pass = 0; pass < 8; ++pass) {
            int count = 0;
            while (XtAppPending(app_instance) & XtIMXEvent) {
                XtAppProcessEvent(app_instance, XtIMXEvent);
                expect(++count < 20000,
                       "Athena controls settle after layout/input");
            }
            XSync(cached_display, False);
        }
    }

    // Return a widget's actual server rectangle, including its native
    // border.
    rect bounds(Widget widget) {
        Position x = 0, y = 0;
        Dimension width = 0, height = 0, border = 0;
        XtVaGetValues(widget,
                      XtNx,
                      &x,
                      XtNy,
                      &y,
                      XtNwidth,
                      &width,
                      XtNheight,
                      &height,
                      XtNborderWidth,
                      &border,
                      nullptr);
        return rect(x, y, width + 2 * border, height + 2 * border);
    }

    // Count visible glyph ink inside the frame, excluding the left
    // caret.
    void visible_text(Widget widget) {
        const auto area = bounds(widget);
        XWindowAttributes actual{};
        XGetWindowAttributes(cached_display, XtWindow(widget), &actual);
        expect(actual.map_state == IsViewable, "text field is mapped");
        auto *pixels = XGetImage(cached_display,
                                 XtWindow(widget),
                                 0,
                                 0,
                                 actual.width,
                                 actual.height,
                                 AllPlanes,
                                 ZPixmap);
        int ink = 0;
        for (int y = 2; y < actual.height - 2; ++y)
            for (int x = 8; x < actual.width - 2; ++x)
                ink += XGetPixel(pixels, x, y) ==
                       BlackPixel(cached_display, 0);
        XDestroyImage(pixels);
        expect(area.h() > 0 && ink > 8,
               "single-line text paints visible glyphs");
    }

    // Use native server input so focus, editing translations and grabs
    // run.
    class input
    {
    public:
        input()
            : library(dlopen("libXtst.so.6", RTLD_NOW)) {
            expect(library, "XTest is available");
        }
        ~input() {
            dlclose(library);
        }
        void key(KeySym symbol, bool down) {
            using action =
                int (*)(Display *, unsigned, Bool, unsigned long);
            auto call = reinterpret_cast<action>(
                dlsym(library, "XTestFakeKeyEvent"));
            expect(call, "XTest keyboard input is available");
            call(cached_display,
                 XKeysymToKeycode(cached_display, symbol),
                 down,
                 0);
        }
        void press(KeySym symbol) {
            key(symbol, true);
            key(symbol, false);
            pump();
        }
        void shortcut(KeySym symbol) {
            key(XK_Control_L, true);
            key(symbol, true);
            key(symbol, false);
            key(XK_Control_L, false);
            pump();
        }
        void click(Widget widget) {
            const auto area = bounds(widget);
            using action =
                int (*)(Display *, unsigned, Bool, unsigned long);
            auto call = reinterpret_cast<action>(
                dlsym(library, "XTestFakeButtonEvent"));
            expect(call, "XTest pointer input is available");
            XWarpPointer(cached_display,
                         None,
                         XtWindow(widget),
                         0,
                         0,
                         0,
                         0,
                         area.w() / 2,
                         area.h() / 2);
            call(cached_display, Button1, true, 0);
            call(cached_display, Button1, false, 0);
            pump();
        }

    private:
        void *library;
    };

    // Capture the real grouping band rectangle while retaining normal
    // painting.
    class grouping_table : public table_view
    {
    public:
        rect band;

    protected:
        void draw_group(gpx &graphics,
                        theme &appearance,
                        const table_group &group,
                        const rect &area,
                        const theme::state &state) override {
            band = area;
            table_view::draw_group(
                graphics, appearance, group, area, state);
        }
    };

    // Exercise one editable-combo field through the original native
    // widgets.
    xaw_combo_box &picker_field(wnd &picker) {
        Widget root =
            XtNameToWidget(wnd_bindings.handle_from_object(&picker),
                           const_cast<char *>("comboBox"));
        auto *field = dynamic_cast<combo_box *>(
            wnd_bindings.object_from_handle(root));
        expect(field, "picker extends the standard editable combo");
        return *combo_box_bindings.object_from_handle(field);
    }

    // Create an ordinary child only after its containing native window
    // exists.
    void attach(wnd &child, wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
} // namespace

// Run while the existing fixture owns the XTest library and its event
// loop.
void run_x11_control_checks(native::app_wnd &owner) {
    using namespace native;
    using namespace linux::x11;
    modeless_wnd window(
        owner, "Athena field checks", rect(20, 20, 640, 720));
    text_edit source("Live validation",
                     text_edit_mode::single_line,
                     16,
                     16,
                     260,
                     24);
    text_edit destination(
        "", text_edit_mode::single_line, 300, 16, 260, 24);
    combo_box combo({"10 mm", "25 mm", "50 mm"},
                    combo_box_style::editable,
                    16,
                    64,
                    220,
                    24);
    date_picker datefield({2026, 12, 23}, rect(16, 112, 220, 24));
    time_picker timefield({9, 30, 0}, rect(300, 112, 180, 24));
    panel general, advanced;
    settings_sidebar sidebar(16, 160, 600, 300);
    table_store rows({{1, {{1, {"First"}}}}, {2, {{1, {"Second"}}}}},
                     {{1, "Group", 0, 2, true, true}});
    grouping_table table;
    sidebar << settings_page(1, "General", general, {}, "Settings")
            << settings_page(2, "Advanced", advanced, {}, "Controls");
    sidebar.set_search_visible(true);
    table.set_bounds(rect(16, 480, 600, 180));
    table.set_columns({{1, "Name", 300}}).set_model(&rows);
    datefield.set_disabled_dates({{2026, 12, 25}});
    timefield.set_seconds_visible(true).set_minute_step(15);
    window.create();
    for (wnd *child : std::initializer_list<wnd *>{&source,
                                                   &destination,
                                                   &combo,
                                                   &datefield,
                                                   &timefield,
                                                   &sidebar,
                                                   &table})
        attach(*child, window);
    window.show();
    pump();
    input events;
    auto widget = [](wnd &control) {
        return wnd_bindings.handle_from_object(&control);
    };
    visible_text(widget(source));
    events.click(widget(source));
    events.shortcut(XK_a);
    events.shortcut(XK_c);
    events.click(widget(destination));
    events.shortcut(XK_v);
    expect(destination.get_text() == "Live validation",
           "native copy/paste transfers visible text");
    visible_text(widget(destination));

    auto &choice = *combo_box_bindings.object_from_handle(&combo);
    events.click(choice.text);
    events.shortcut(XK_a);
    for (KeySym key : {XK_5, XK_0, XK_space, XK_m, XK_m})
        events.press(key);
    events.press(XK_Return);
    expect(combo.get_text() == "50 mm" &&
               combo.get_selected_index() == 2,
           "typed exact match updates combo model without a newline");
    events.click(choice.button);
    WidgetList entries = nullptr;
    Cardinal count = 0;
    XtVaGetValues(choice.menu,
                  XtNchildren,
                  &entries,
                  XtNnumChildren,
                  &count,
                  nullptr);
    Pixmap current = None, previous = None;
    XtVaGetValues(entries[2], XtNleftBitmap, &current, nullptr);
    XtVaGetValues(entries[0], XtNleftBitmap, &previous, nullptr);
    expect(count == 3 && choice.selection_mark != None &&
               current == choice.selection_mark && previous == None,
           "opening popup marks the typed exact match instead of its "
           "previous value");
    XtPopdown(choice.menu);
    pump();

    Widget search_widget =
        XtNameToWidget(widget(sidebar), const_cast<char *>("textEdit"));
    Widget prompt = XtNameToWidget(widget(sidebar),
                                   const_cast<char *>("searchPrompt"));
    Widget navigation = XtNameToWidget(
        widget(sidebar), const_cast<char *>("settingsNavigation"));
    expect(search_widget && prompt && navigation,
           "settings has a native Search prompt/editor/navigation");
    const auto search = bounds(search_widget), label = bounds(prompt),
               nav = bounds(navigation);
    expect(nav.x1() == to_device(6) && label.x1() == nav.x1() &&
               search.y1() == to_device(6) &&
               nav.y1() - search.y2() == to_device(6) &&
               to_device(sidebar.get_dimensions().h) - nav.y2() ==
                   to_device(6) &&
               search.x2() == nav.x2(),
           "search/navigation use equal outer margins and spacing");
    events.click(search_widget);
    for (KeySym key : {XK_a, XK_d, XK_v})
        events.press(key);
    if (sidebar.get_search_text() != "adv" ||
        sidebar.get_selected_item() != 1)
        throw std::runtime_error(
            "native search filters without changing the active page: "
            "query=" +
            sidebar.get_search_text() +
            ", page=" + std::to_string(sidebar.get_selected_item()));
    visible_text(search_widget);
    sidebar.set_search_text("");
    pump();

    auto &date = picker_field(datefield);
    auto &time = picker_field(timefield);
    for (auto *field : {&date, &time}) {
        XFontStruct *font = nullptr;
        char *text = nullptr;
        Dimension left = 0, right = 0;
        XtVaGetValues(field->text,
                      XtNfont,
                      &font,
                      XtNstring,
                      &text,
                      XtNleftMargin,
                      &left,
                      XtNrightMargin,
                      &right,
                      nullptr);
        expect(font && text && left == XTextWidth(font, "0", 1) / 2 &&
                   right == left,
               "picker text has half-character horizontal margins");
        const int column = field == &time ? bounds(field->root).h() / 2
                                          : bounds(field->root).h();
        const int expected = XTextWidth(font, text, std::strlen(text)) +
                             left + right + column + 2;
        expect(std::abs(int(bounds(field->root).w()) - expected) <= 1,
               "picker width fits its text, margins and arrow column");
    }
    expect(
        bounds(date.root).h() == bounds(choice.root).h() &&
            bounds(time.root).h() == bounds(choice.root).h() &&
            bounds(widget(source)).h() == bounds(choice.root).h(),
        "ordinary editors, combos and pickers have equal outer height");
    expect(bounds(date.button).w() == bounds(choice.button).w() &&
               bounds(date.button).h() == bounds(choice.button).h() &&
               bounds(date.button).x2() == bounds(date.root).w(),
           "calendar trigger has standard combo button dimensions");
    events.click(date.button);
    Widget popup =
        XtNameToWidget(date.root, const_cast<char *>("dateCalendar"));
    expect(popup && XtIsRealized(popup),
           "real date-arrow click opens a calendar");
    const auto arrow = XCreateFontCursor(cached_display, XC_left_ptr);
    // Establish an independent reference cursor over the popup shell.
    XWarpPointer(
        cached_display, None, XtWindow(popup), 0, 0, 0, 0, 1, 1);
    pump();
    const auto calendar_cursor = cursor_pixels();
    XDefineCursor(cached_display, XtWindow(popup), arrow);
    XSync(cached_display, False);
    expect(calendar_cursor == cursor_pixels(),
           "calendar inherits a normal arrow, never the root X cursor");
    XFreeCursor(cached_display, arrow);
    Widget calendar =
        XtNameToWidget(popup, const_cast<char *>("calendar"));
    XtVaGetValues(calendar,
                  XtNchildren,
                  &entries,
                  XtNnumChildren,
                  &count,
                  nullptr);
    Widget day = nullptr;
    bool holiday_disabled = false;
    for (Cardinal index = 10; index < count; ++index) {
        char *label_text = nullptr;
        XtVaGetValues(entries[index], XtNlabel, &label_text, nullptr);
        if (label_text && std::string_view(label_text) == "24")
            day = entries[index];
        if (label_text && std::string_view(label_text) == "25")
            holiday_disabled = !XtIsSensitive(entries[index]);
    }
    expect(day && holiday_disabled,
           "calendar populates days and disables holidays");
    visible_text(day);
    events.click(day);
    expect(datefield.get_date() == native::date{2026, 12, 24},
           "calendar pointer selection commits a date");
    events.click(date.button);
    events.click(date.button);
    XWindowAttributes state{};
    XGetWindowAttributes(cached_display, XtWindow(popup), &state);
    expect(state.map_state == IsUnmapped,
           "second calendar-arrow click closes the popup");
    events.click(date.text);
    events.shortcut(XK_a);
    for (KeySym key : {XK_2,
                       XK_0,
                       XK_2,
                       XK_6,
                       XK_minus,
                       XK_1,
                       XK_2,
                       XK_minus,
                       XK_2,
                       XK_6})
        events.press(key);
    events.press(XK_Return);
    expect(datefield.get_date() == native::date{2026, 12, 26},
           "typed calendar date commits through the standard combo "
           "editor");
    events.shortcut(XK_a);
    for (KeySym key : {XK_2,
                       XK_0,
                       XK_2,
                       XK_6,
                       XK_minus,
                       XK_1,
                       XK_2,
                       XK_minus,
                       XK_2,
                       XK_5})
        events.press(key);
    events.press(XK_Return);
    expect(datefield.get_date() == native::date{2026, 12, 26} &&
               datefield.get_valid(),
           "typing an excluded holiday restores the accepted date");
    Widget down =
        XtNameToWidget(time.root, const_cast<char *>("timeDown"));
    const auto up_area = bounds(time.button), down_area = bounds(down);
    expect(up_area.w() == down_area.w() && up_area.w() == up_area.h() &&
               up_area.h() == down_area.h() &&
               up_area.x1() == down_area.x1() &&
               bounds(time.root).h() - down_area.y2() <= 1,
           "equal time arrows fill the standard combo column inside "
           "the edit field");
    XawTextSetSelection(time.text, 0, 2);
    events.click(time.button);
    expect(timefield.get_time() == time_of_day{10, 30, 0},
           "time up changes selected hours");
    events.click(down);
    expect(timefield.get_time() == time_of_day{9, 30, 0},
           "repeated time steps preserve the selected component");
    XawTextSetSelection(time.text, 3, 5);
    events.click(time.button);
    expect(timefield.get_time() == time_of_day{9, 45, 0},
           "time minutes use configured step");
    XawTextSetSelection(time.text, 6, 8);
    events.click(time.button);
    expect(timefield.get_time() == time_of_day{9, 45, 1},
           "time seconds use one-second steps");
    expect(table.band.h() > 0, "table paints a grouping band");
    const auto band = rect_to_device(table.band);
    auto *line = XGetImage(cached_display,
                           XtWindow(widget(table)),
                           to_device(100),
                           band.y2() - 1,
                           1,
                           1,
                           AllPlanes,
                           ZPixmap);
    expect(XGetPixel(line, 0, 0) == BlackPixel(cached_display, 0),
           "group bottom separates the first table item");
    XDestroyImage(line);
    if (const char *path = std::getenv("NATIVE_X11_CONTROL_SNAPSHOT")) {
        Widget shell = shell_bindings.handle_from_object(&window);
        XWindowAttributes actual{};
        XGetWindowAttributes(cached_display, XtWindow(shell), &actual);
        auto *pixels = XGetImage(cached_display,
                                 XtWindow(shell),
                                 0,
                                 0,
                                 actual.width,
                                 actual.height,
                                 AllPlanes,
                                 ZPixmap);
        img image(actual.width, actual.height);
        for (int y = 0; y < actual.height; ++y)
            for (int x = 0; x < actual.width; ++x)
                image.pixels()[y * actual.width + x] =
                    XGetPixel(pixels, x, y) ==
                            BlackPixel(cached_display, 0)
                        ? rgba(0, 0, 0, 255)
                        : rgba(255, 255, 255, 255);
        XDestroyImage(pixels);
        image.save(path);
    }
    window.destroy();
    pump();

    std::vector<std::unique_ptr<native::button>> cells;
    modeless_wnd layout(
        owner, "Nested layout resize", rect(20, 20, 560, 420));
    layout.create();
    for (int index = 0; index < 7; ++index) {
        cells.push_back(
            std::make_unique<native::button>("Cell", 0, 0, 100, 28));
        attach(*cells.back(), layout);
    }
    auto grid = std::make_unique<grid_layout_manager>();
    (*grid) << row(pixels(52)) << row(pixels(52)) << row(star())
            << row(pixels(52)) << column(star()) << column(star(2))
            << cell(*cells[0], 1, 0, 1, 2, 6)
            << cell(*cells[1], 2, 0, 1, 1, 6)
            << cell(*cells[2], 3, 0, 1, 2, 6);
    auto nested = std::make_unique<grid_layout_manager>(2, 2);
    for (int index = 3; index < 7; ++index)
        nested->add(
            *cells[index], (index - 3) / 2, (index - 3) % 2, 1, 1, 6);
    (*grid) << child_grid(std::move(nested), 2, 1, 1, 1, 0);
    layout.set_layout(std::move(grid));
    layout.show();
    pump();
    Widget shell = shell_bindings.handle_from_object(&layout);
    XResizeWindow(cached_display,
                  XtWindow(shell),
                  to_device(720),
                  to_device(600));
    pump();
    XtWidgetGeometry wanted{}, answer{};
    wanted.request_mode = CWWidth | CWHeight;
    wanted.width = to_device(300);
    wanted.height = to_device(260);
    expect(
        XtQueryGeometry(main_wnd_bindings.handle_from_object(&layout),
                        &wanted,
                        &answer) == XtGeometryYes,
        "layout host accepts a smaller allocation instead of its "
        "previous preferred size");
    XResizeWindow(
        cached_display, XtWindow(shell), wanted.width, wanted.height);
    pump();
    expect(layout.get_dimensions().w == 300 &&
               layout.get_dimensions().h == 260 &&
               cells[0]->get_dimensions().w == 288,
           "nested layout can grow and shrink while refitting its "
           "native controls");
    layout.destroy();
    pump();
}
