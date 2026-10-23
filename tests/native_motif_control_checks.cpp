//
// Checks Motif property editors, native combo parity, navigation
// spacing, checkbox relief and continuous table selection against live
// widgets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/openmotif/globals.h"
#include "../lib/native/picker_peer.h"
#include "../lib/native/combo_picker.h"
#include "../lib/native/platforms/linux/x_image.h"
#include <Xm/ComboBox.h>
#include <Xm/Container.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <dlfcn.h>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <vector>

namespace
{
    namespace motif = linux::openmotif;
    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    // Compare real server cursors, including initially unfocused edits.
    std::vector<unsigned long> cursor_pixels() {
        static auto *library = dlopen("libXfixes.so.3", RTLD_NOW);
        using proc = XFixesCursorImage *(*)(Display *);
        auto capture = reinterpret_cast<proc>(
            dlsym(library, "XFixesGetCursorImage"));
        expect(capture, "XFixes provides cursor-image checks");
        auto *image = capture(motif::cached_display);
        expect(image, "read current server cursor");
        std::vector<unsigned long> pixels(
            image->pixels,
            image->pixels + image->width * image->height);
        XFree(image);
        return pixels;
    }
    // Read the native toggle font selected by the active CDE resources.
    Font toggle_font(Widget widget) {
        XmFontList fonts = nullptr;
        XtVaGetValues(widget, XmNfontList, &fonts, nullptr);
        XmFontContext context = nullptr;
        expect(XmFontListInitFontContext(&context, fonts),
               "toggle has a native font");
        char *tag = nullptr;
        XFontStruct *font = nullptr;
        const bool found = XmFontListGetNextFont(context, &tag, &font);
        if (tag)
            XtFree(tag);
        XmFontListFreeFontContext(context);
        expect(found && font, "toggle has a core font entry");
        return font->fid;
    }
    Widget widget(native::wnd &owner) {
        return motif::wnd_bindings.handle_from_object(&owner);
    }
    void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }
    void pump() {
        XSync(motif::cached_display, False);
        int count = 0;
        while (XtAppPending(motif::app_instance)) {
            expect(++count < 4000,
                   "Motif control redraws remain bounded");
            XtAppProcessEvent(motif::app_instance, XtIMAll);
            XSync(motif::cached_display, False);
        }
    }
    void descendants(Widget owner, std::vector<Widget> &result) {
        result.push_back(owner);
        if (!XtIsComposite(owner))
            return;
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(owner,
                      XmNchildren,
                      &children,
                      XmNnumChildren,
                      &count,
                      nullptr);
        for (Cardinal index = 0; index < count; ++index)
            descendants(children[index], result);
    }
    std::vector<unsigned long> capture(Widget owner) {
        Position x = 0, y = 0;
        Dimension width = 0, height = 0;
        XtTranslateCoords(owner, 0, 0, &x, &y);
        XtVaGetValues(
            owner, XmNwidth, &width, XmNheight, &height, nullptr);
        XImage *image = XGetImage(motif::cached_display,
                                  RootWindowOfScreen(XtScreen(owner)),
                                  x,
                                  y,
                                  width,
                                  height,
                                  AllPlanes,
                                  ZPixmap);
        expect(image, "capture server-composited Motif control");
        std::vector<unsigned long> result;
        for (int row = 0; row < height; ++row)
            for (int column = 0; column < width; ++column)
                result.push_back(XGetPixel(image, column, row));
        XDestroyImage(image);
        return result;
    }
    unsigned long table_pixel(native::table_view &table, int x, int y) {
        auto *cache =
            motif::wnd_gpx_bindings.object_from_handle(&table);
        expect(cache && cache->backbuffer,
               "table has a painted backing store");
        XImage *image = XGetImage(motif::cached_display,
                                  cache->backbuffer,
                                  x,
                                  y,
                                  1,
                                  1,
                                  AllPlanes,
                                  ZPixmap);
        expect(image, "read selected row boundary");
        const auto result = XGetPixel(image, 0, 0);
        XDestroyImage(image);
        return result;
    }
    void snapshot(native::wnd &owner) {
        const char *path = std::getenv("NATIVE_MOTIF_CONTROL_SNAPSHOT");
        if (!path)
            return;
        const auto pixels = capture(widget(owner));
        Dimension width = 0, height = 0;
        XtVaGetValues(widget(owner),
                      XmNwidth,
                      &width,
                      XmNheight,
                      &height,
                      nullptr);
        native::img image(width, height);
        for (std::size_t index = 0; index < pixels.size(); ++index)
            image.pixels()[index] = native::detail::rgba_from_x_pixel(
                DefaultVisualOfScreen(XtScreen(widget(owner))),
                pixels[index]);
        image.save(path);
    }
} // namespace

// Exercise native resources and server pixels in an owned desktop
// window.
void check_motif_control_styles(native::app_wnd &root) {
    using namespace native;
    modeless_wnd window(
        root, "Motif control styling", 50, 50, 800, 580);
    property_grid properties(10, 10, 360, 190);
    auto content = std::make_shared<property_drop_down>();
    content->to_text = [](const property_value &) {
        return "One";
    };
    content->create_content = [](const property_value &,
                                 property_drop_down::commit) {
        return std::make_unique<canvas>();
    };
    properties.set_row_height(32).set_items({{"text",
                                              "Text",
                                              property_kind::text,
                                              std::string("Value"),
                                              {},
                                              false,
                                              {}},
                                             {"number",
                                              "Number",
                                              property_kind::number,
                                              12.0,
                                              {},
                                              false,
                                              {}},
                                             {"check",
                                              "Top border",
                                              property_kind::boolean,
                                              false,
                                              {},
                                              false,
                                              {}},
                                             {"choice",
                                              "Choice",
                                              property_kind::choice,
                                              std::string("One"),
                                              {"One", "Two"},
                                              false,
                                              {}},
                                             {"custom",
                                              "Custom canvas",
                                              property_kind::drop_down,
                                              std::string("One"),
                                              {},
                                              false,
                                              content}});
    settings_sidebar settings(390, 10, 400, 550);
    panel page;
    check exclude("Exclude Saturdays", 5, 10, 170, 30);
    exclude.set_parent(&page);
    page.on_wnd_create.connect([&] {
        exclude.create();
        exclude.show();
        return true;
    });
    settings.set_search_visible(true).set_sidebar_width(190);
    settings << settings_page(1, "General", page, {}, "Settings");
    check unchecked("Native unchecked", 10, 220, 170, 30);
    check checked("Native checked", 10, 260, 170, 30);
    radio option("Native radio", 10, 295, 170, 30);
    text_edit single(
        "Single", text_edit_mode::single_line, 10, 475, 170, 24);
    text_edit multi(
        "Multiline", text_edit_mode::multi_line, 195, 475, 175, 60);
    date_picker datefield({2026, 12, 24}, rect(10, 545, 170, 24));
    time_picker timefield({9, 30, 0}, rect(195, 545, 175, 24));
    checked.set_checked(true);
    canvas primitive(190, 220, 180, 70);
    primitive.on_wnd_paint.connect([&](wnd_paint_event event) {
        auto appearance = theme::create(event.g);
        appearance->draw_surface(
            primitive.get_client_bounds(), surface_kind::panel, {});
        appearance->draw_check(
            rect(0, 0, 180, 30), "Theme unchecked", {});
        theme::state selected;
        selected.selected = true;
        appearance->draw_check(
            rect(0, 40, 180, 30), "Theme checked", selected);
        return true;
    });
    table_store store;
    for (int row = 1; row <= 3; ++row)
        store.add_row(
            {static_cast<table_row_id>(row), {{1, {"Selected"}}}});
    table_view table(10, 330, 360, 130);
    table.set_model(&store)
        .set_columns({{1, "Value", 330}})
        .set_header_visible(false)
        .set_row_height(26)
        .set_alternating_rows(false)
        .set_selection_mode(table_selection_mode::multiple)
        .set_selected_rows({1, 2, 3})
        .set_grid_lines(table_grid_lines::none);
    window.create();
    for (wnd *child : std::array<wnd *, 11>{&properties,
                                            &settings,
                                            &unchecked,
                                            &checked,
                                            &primitive,
                                            &table,
                                            &option,
                                            &single,
                                            &multi,
                                            &datefield,
                                            &timefield})
        attach(*child, window);
    window.show();
    XRaiseWindow(
        motif::cached_display,
        XtWindow(motif::shell_bindings.handle_from_object(&window)));
    for (int cycle = 0; cycle < 5; ++cycle) {
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    snapshot(window);
    expect(toggle_font(widget(unchecked)) ==
               toggle_font(widget(option)),
           "native checkbox and radio retain the same toggle font");
    auto *display = motif::cached_display;
    XWarpPointer(
        display, None, XtWindow(widget(window)), 0, 0, 0, 0, 780, 575);
    pump();
    const auto beam = XCreateFontCursor(display, XC_xterm);
    XDefineCursor(display, XtWindow(widget(window)), beam);
    XSync(display, False);
    const auto expected_cursor = cursor_pixels();
    XUndefineCursor(display, XtWindow(widget(window)));
    XFreeCursor(display, beam);
    for (auto *edit : {&single, &multi}) {
        Dimension width = 0, height = 0;
        XtVaGetValues(widget(*edit),
                      XmNwidth,
                      &width,
                      XmNheight,
                      &height,
                      nullptr);
        expect(width == edit->get_dimensions().w &&
                   height == edit->get_dimensions().h,
               "native text widgets preserve the allocated geometry "
               "after font/value updates");
        XWarpPointer(display,
                     None,
                     XtWindow(widget(*edit)),
                     0,
                     0,
                     0,
                     0,
                     edit->get_dimensions().w - 8,
                     10);
        pump();
        const auto actual_cursor = cursor_pixels();
        if (actual_cursor != expected_cursor)
            throw std::runtime_error(
                std::string(edit == &single ? "single" : "multiline") +
                " native I-beam differs: actual pixels " +
                std::to_string(actual_cursor.size()) + ", expected " +
                std::to_string(expected_cursor.size()));
    }
    for (panel *picker : {static_cast<panel *>(&datefield),
                          static_cast<panel *>(&timefield)}) {
        auto &field = detail::picker_field(*picker);
        expect(
            field.get_dimensions().w < 170,
            "Motif date/time fields fit their text and arrow column");
    }
    auto &date_entry = detail::picker_field(datefield);
    auto *date_actions =
        detail::peer_state<detail::combo_picker_actions>(date_entry);
    date_actions->open();
    pump();
    auto *calendar = detail::picker_calendar(datefield);
    expect(calendar && calendar->get_created(),
           "native calendar popup opens");
    motif::track_picker_popup(date_entry);
    pump();
    const auto shell =
        motif::shell_bindings.handle_from_object(calendar);
    XWindowAttributes attributes{};
    XGetWindowAttributes(
        motif::cached_display, XtWindow(shell), &attributes);
    expect(attributes.border_width == 1,
           "calendar popup has its own native server-drawn border");
    Position popup_x = 0, popup_y = 0;
    XtTranslateCoords(shell, 0, 0, &popup_x, &popup_y);
    auto *border = XGetImage(motif::cached_display,
                             DefaultRootWindow(motif::cached_display),
                             popup_x - 1,
                             popup_y + 5,
                             1,
                             1,
                             AllPlanes,
                             ZPixmap);
    expect(border && XGetPixel(border, 0, 0) ==
                         BlackPixelOfScreen(XtScreen(shell)),
           "calendar border is visible on the desktop");
    XDestroyImage(border);
    calendar->destroy();
    pump();
    auto &time_entry = detail::picker_field(timefield);
    auto time_widget = widget(time_entry);
    Widget up = XtNameToWidget(time_widget,
                               const_cast<char *>("pickerArrowUp"));
    Widget down = XtNameToWidget(time_widget,
                                 const_cast<char *>("pickerArrowDown"));
    Dimension uw = 0, uh = 0, dw = 0, dh = 0;
    XtVaGetValues(up, XmNwidth, &uw, XmNheight, &uh, nullptr);
    XtVaGetValues(down, XmNwidth, &dw, XmNheight, &dh, nullptr);
    expect(uw == uh && uw == dw && uh == dh,
           "native time arrows are equal squares after Form layout");
    const auto glyph_size = [&](Widget arrow) {
        const auto pixels = capture(arrow);
        Pixel ink = 0;
        XtVaGetValues(arrow, XmNforeground, &ink, nullptr);
        int left = uw, top = uh, right = -1, bottom = -1;
        for (int y = 0; y < uh; ++y)
            for (int x = 0; x < uw; ++x)
                if (pixels[y * uw + x] == ink) {
                    left = std::min(left, x);
                    top = std::min(top, y);
                    right = std::max(right, x);
                    bottom = std::max(bottom, y);
                }
        expect(right >= left, "native arrow glyph is visible");
        return std::pair{right - left + 1, bottom - top + 1};
    };
    const auto up_size = glyph_size(up), down_size = glyph_size(down);
    expect(up_size == down_size,
           ("native arrow glyph sizes " +
            std::to_string(up_size.first) + "x" +
            std::to_string(up_size.second) + "/" +
            std::to_string(down_size.first) + "x" +
            std::to_string(down_size.second))
               .c_str());
    std::vector<Widget> values;
    descendants(widget(properties), values);
    std::vector<Widget> combos;
    int plain_values = 0, checks = 0;
    Font value_font = None;
    for (Widget value : values) {
        auto *owner = motif::wnd_bindings.object_from_handle(value);
        if (dynamic_cast<text_edit *>(owner) ||
            dynamic_cast<check *>(owner)) {
            Pixel foreground = ~0UL;
            XtVaGetValues(value, XmNforeground, &foreground, nullptr);
            expect(foreground == BlackPixelOfScreen(XtScreen(value)),
                   "plain property values use black text");
            const auto font = toggle_font(value);
            if (!value_font)
                value_font = font;
            expect(font == value_font,
                   "all property values share the control font");
            if (dynamic_cast<check *>(owner))
                ++checks;
            else
                ++plain_values;
        }
        if (XmIsComboBox(value)) {
            Widget text = nullptr;
            XtVaGetValues(value, XmNtextField, &text, nullptr);
            Pixel foreground = ~0UL;
            XtVaGetValues(text, XmNforeground, &foreground, nullptr);
            if (!value_font)
                value_font = toggle_font(text);
            expect(foreground == BlackPixelOfScreen(XtScreen(text)),
                   "choice and custom property values use black ink");
            expect(toggle_font(text) == value_font,
                   "choice and custom property values use the common "
                   "control font");
            combos.push_back(value);
        }
    }
    expect(
        plain_values == 2 && checks == 1 && combos.size() == 2,
        "property rows retain native text, toggle and combo editors");
    expect(capture(combos[0]) == capture(combos[1]),
           "custom property dropdown matches the native choice combo "
           "pixel for pixel");
    // Compare complete indicator patches with native toggles, allowing
    // each widget's own focus/label margins to position the indicator.
    const auto themed = capture(widget(primitive));
    for (int selected = 0; selected < 2; ++selected) {
        Widget toggle = widget(selected ? checked : unchecked);
        Dimension size = 0;
        XtVaGetValues(toggle, XmNindicatorSize, &size, nullptr);
        const int side = std::max(5, std::min(int(size), 26));
        const int edge = side - 3 - std::max(0, (side - 10) / 10);
        const int inset = (side - edge) / 2;
        const int theme_x = 3 + inset;
        const int theme_y = selected * 40 + (30 - side) / 2 + inset;
        const auto actual = capture(toggle);
        bool match = false;
        for (int y = 0; y <= 30 - edge; ++y)
            for (int x = 0; x <= 24 - edge; ++x) {
                bool equal = true;
                for (int row = 0; row < edge && equal; ++row)
                    for (int column = 0; column < edge && equal;
                         ++column)
                        equal = actual[(y + row) * 170 + x + column] ==
                                themed[(theme_y + row) * 180 + theme_x +
                                       column];
                match = match || equal;
            }
        expect(
            match,
            "theme check indicator matches the native toggle pixels");
    }
    unsigned char alignment = XmALIGNMENT_CENTER;
    XtVaGetValues(widget(exclude), XmNalignment, &alignment, nullptr);
    expect(alignment == XmALIGNMENT_BEGINNING,
           "settings checkbox labels align left");
    values.clear();
    descendants(widget(settings), values);
    bool found_tree = false, found_search = false;
    const auto navigation =
        detail::settings_navigation_bounds(settings);
    for (Widget value : values) {
        auto *owner = motif::wnd_bindings.object_from_handle(value);
        if (auto *tree = dynamic_cast<tree_view *>(owner)) {
            auto *state =
                motif::tree_view_bindings.object_from_handle(tree);
            Dimension indent = 0;
            XtVaGetValues(state->content,
                          XmNoutlineIndentation,
                          &indent,
                          nullptr);
            expect(indent == 8,
                   "settings navigator halves the normal tree "
                   "indentation");
            expect(state->items.size() == 2,
                   "settings category owns its General entry");
            Position child_y = -1;
            Dimension category_height = 0;
            XtVaGetValues(
                state->items[0], XmNheight, &category_height, nullptr);
            XtVaGetValues(state->items[1], XmNy, &child_y, nullptr);
            expect(child_y >= category_height && child_y < 100,
                   "expanded settings category displays its indented "
                   "child below the heading");
            found_tree = true;
        }
        if (auto *edit = dynamic_cast<text_edit *>(owner)) {
            if (edit->get_parent() &&
                edit->get_parent()->get_parent() == &settings) {
                const auto search = edit->get_parent()->get_bounds();
                expect(search.x1() == 6 && search.y1() == 6 &&
                           navigation.y1() - search.y2() == 6 &&
                           settings.get_dimensions().h -
                                   navigation.y2() ==
                               6,
                       "search gap and navigation outer margins are "
                       "all six pixels");
                found_search = true;
            }
        }
    }
    expect(found_tree && found_search,
           "settings exposes native search and outline peers");
    table.on_native_focus(true);
    pump();
    const auto fill = table_pixel(table, 100, 20);
    for (int y : {25, 26, 51, 52, 77})
        expect(table_pixel(table, 100, y) == fill,
               "multiselected rows have continuous fill before grid "
               "toggles");
    table.set_grid_lines(table_grid_lines::both);
    pump();
    expect(table_pixel(table, 100, 25) != fill,
           "enabled table grid is visible");
    table.set_grid_lines(table_grid_lines::none);
    pump();
    for (int y : {25, 26, 51, 52, 77})
        expect(table_pixel(table, 100, y) == fill,
               "selection boundaries remain continuous after disabling "
               "the grid");
    snapshot(window);
    window.destroy();
    pump();
}
