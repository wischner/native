//
// Verifies settings builders and date/time policies, plus native widget
// callbacks, selected-page lifecycle and destruction/recreation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/picker_peer.h"
#include <iostream>
#include <stdexcept>
#include <string_view>
#ifdef NATIVE_SETTINGS_GTK
#include "../lib/native/toolkits/wayland/picker_widgets.h"
#include <algorithm>
#include <array>
#include <cstdint>
#endif
#ifdef _WIN32
#include "../lib/native/platforms/windows/globals.h"
#endif
#ifdef __APPLE__
#include "../lib/native/platforms/mac/globals.h"
#include <cmath>
#endif
#ifdef __HAIKU__
#include "../lib/native/platforms/haiku/globals.h"
#include <DateTimeEdit.h>
#include <Autolock.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <ScrollBar.h>
#include <Button.h>
#include <CalendarView.h>
#include <FilePanel.h>
#include <TextControl.h>
#include <cmath>
#include <chrono>
#include <thread>
#endif

namespace
{
    void expect(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
#ifdef _WIN32
    void expect_padding(HWND field) {
        wchar_t text[128]{};
        const int length = GetWindowTextW(field, text, 128);
        expect(length > 2 && text[0] == L' ' && text[length - 1] == L' ',
               "native Windows pickers retain padding before and after text");
    }
#endif
    template<typename action> void invalid(action run) {
        bool caught = false;
        try { run(); } catch (const std::invalid_argument &) { caught = true; }
        expect(caught, "invalid model update must throw");
    }
    void models() {
        using namespace native;
        text_edit entry;
        combo_box choice;
        date_picker date_input;
        time_picker time_input;
        expect(entry.get_dimensions().h == choice.get_dimensions().h &&
            entry.get_dimensions().h == date_input.get_dimensions().h &&
            entry.get_dimensions().h == time_input.get_dimensions().h,
            "single-line controls have the same portable default height");
        expect(date{2000, 2, 29}.valid() && !date{1900, 2, 29}.valid() &&
            !date{2100, 2, 29}.valid() && !date{2026, 4, 31}.valid(), "Gregorian leap rules");
        expect(date{1601, 1, 1}.weekday() == day_of_week::monday &&
            date{2000, 1, 1}.weekday() == day_of_week::saturday &&
            date{2026, 10, 21}.weekday() == day_of_week::wednesday, "weekday anchors");
        expect(date{2026, 1, 2}.to_string() == "2026-01-02", "stable civil formatting");
        expect(!time_of_day{24, 0, 0}.valid() && !time_of_day{0, 60, 0}.valid() &&
            !time_of_day{0, 0, 60}.valid(), "wall-clock limits");
        date_picker dates({2026, 12, 24}, rect(0, 0, 220, 32));
        int changes = 0, rejected = 0;
        dates.on_change.connect([&](date) { ++changes; return false; });
        dates.on_rejected.connect([&](date) { ++rejected; return false; });
        dates.set_range({2026, 1, 1}, {2027, 12, 31})
            .set_disabled_dates({{2026, 12, 25}, {2026, 12, 25}})
            .set_disabled_weekdays({day_of_week::saturday, day_of_week::sunday});
        expect(dates.get_disabled_dates().size() == 1 && changes == 0, "silent normalized policies");
        expect(!dates.on_native_date({2026, 12, 25}) &&
            !dates.on_native_date({2026, 12, 26}) && rejected == 2 &&
            dates.get_date() == date{2026, 12, 24}, "holiday/weekend proposals are rejected");
        invalid([&] { dates.set_date({2026, 12, 25}); });
        invalid([&] { dates.set_range({2027, 1, 1}, {2026, 1, 1}); });
        expect(dates.on_native_date({2026, 12, 28}) && dates.on_native_date({2026, 12, 28}) && changes == 1,
            "distinct accepted user selections emit once");
        dates.set_validator([](date value) { return value.day != 28; });
        expect(!dates.get_valid() && dates.get_date().day == 28 && changes == 1,
            "rule changes preserve cached values without signals");
        dates.set_validator([](date) -> bool { throw std::runtime_error("app filter"); });
        expect(!dates.is_date_enabled({2026, 12, 29}), "throwing predicates reject safely");
        dates.set_validator({}).set_date({2026, 12, 29});
        expect(changes == 1 && dates.get_valid(), "programmatic dates remain silent");

        time_picker times({9, 30, 15}, rect(0, 0, 200, 32));
        int time_changes = 0;
        times.on_change.connect([&](time_of_day) { ++time_changes; return false; });
        times.set_range({8, 0, 0}, {18, 0, 0}).set_minute_step(15);
        invalid([&] { times.set_minute_step(7); });
        expect(times.get_minute_step() == 15 && !times.on_native_time({9, 31, 0}), "minute-step validation");
        expect(!times.on_native_time({7, 45, 0}) && times.on_native_time({9, 45, 30}) &&
            times.on_native_time({9, 45, 30}) && time_changes == 1, "time range and deduplicated signal");
        times.set_seconds_visible(true).set_seconds_visible(false);
        expect(times.get_time().second == 30 && time_changes == 1, "hidden seconds are preserved");
        times.set_validator([](time_of_day value) { return value.hour != 9; });
        expect(!times.get_valid(), "time availability can change without coercion");

        panel first, second, third;
        settings_sidebar sidebar;
        sidebar << settings_page(10, "General", first) << settings_page(20, "Dates", second)
                << settings_page(30, "Disabled", third, {}, {}, false);
        int selections = 0;
        sidebar.on_selection_change.connect([&](int) { ++selections; return false; });
        invalid([&] { sidebar.add_item(settings_page(10, "Duplicate", third)); });
        invalid([&] { sidebar.set_selected_item(30); });
        sidebar.set_selected_item(20).set_item_text(20, "Calendar");
        expect(selections == 0 && sidebar.get_selected_item() == 20, "silent stable-ID selection");
        int searches = 0;
        sidebar.on_search_change.connect([&](std::string) { ++searches; return true; });
        invalid([&] { sidebar.set_search_text("two\nlines"); });
        sidebar.set_search_visible(true).set_search_text("CAL");
        expect(detail::settings_visible_items(sidebar).size() == 1 &&
            sidebar.get_selected_item() == 20 && searches == 0, "silent case-insensitive search");
        sidebar.on_native_search("no match");
        expect(detail::settings_visible_items(sidebar).empty() && searches == 1 &&
            sidebar.get_selected_item() == 20, "search preserves active page when there are no matches");
        sidebar.set_search_visible(false);
        expect(detail::settings_visible_items(sidebar).size() == 3 &&
            sidebar.get_search_text() == "no match", "hidden search suspends filtering and retains query");
        sidebar.on_native_selection(10); sidebar.on_native_selection(10); sidebar.on_native_selection(30);
        expect(selections == 1 && sidebar.get_selected_item() == 10, "enabled distinct user navigation");
        sidebar.set_item_enabled(10, false);
        expect(sidebar.get_selected_item() == 20 && selections == 1, "disabling active page chooses an enabled page silently");
        sidebar.remove_item(20);
        expect(sidebar.get_selected_item() == 0 && !second.get_parent(), "removal detaches borrowed content");
        sidebar.clear_items();
        expect(!first.get_parent() && !third.get_parent(), "clear detaches every page");
    }
    void native_date(native::date_picker &owner, native::date value) {
#ifdef NATIVE_SETTINGS_GTK
        auto *calendar = linux::wayland::find_widget(linux::wayland::state(owner).widget, GTK_TYPE_CALENDAR);
        expect(calendar != nullptr, "native GTK calendar exists");
        auto *date = g_date_time_new_local(value.year, value.month, value.day, 12, 0, 0);
        gtk_calendar_select_day(GTK_CALENDAR(calendar), date); g_date_time_unref(date);
#elif defined(_WIN32)
        HWND control = GetWindow(windows::wnd_bindings.handle_from_object(&owner), GW_CHILD);
        wchar_t name[64]{}; GetClassNameW(control, name, 64);
        expect(std::wstring(name) == DATETIMEPICK_CLASSW, "Win32 common-control picker exists");
        expect_padding(control);
        SYSTEMTIME date{}; date.wYear = value.year; date.wMonth = value.month; date.wDay = value.day;
        DateTime_SetSystemtime(control, GDT_VALID, &date);
        // Programmatic DTM_SETSYSTEMTIME need not send a user notification.
        // Deliver the same native WM_NOTIFY payload as a field/calendar edit.
        NMDATETIMECHANGE event{};
        event.nmhdr.hwndFrom = control; event.nmhdr.code = DTN_DATETIMECHANGE;
        event.dwFlags = GDT_VALID; event.st = date;
        SendMessageW(GetParent(control), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&event));
#elif defined(__APPLE__)
        auto *view = mac::panel_bindings.object_from_handle(&owner)->view;
        auto *field = [[view subviews] firstObject];
        auto *picker = static_cast<NSDatePicker *>([[field subviews] firstObject]);
        expect([picker isKindOfClass:[NSDatePicker class]], "AppKit date picker exists");
        auto *parts = [[[NSDateComponents alloc] init] autorelease];
        [parts setYear:value.year]; [parts setMonth:value.month]; [parts setDay:value.day];
        [picker setDateValue:[[picker calendar] dateFromComponents:parts]];
        [picker sendAction:[picker action] to:[picker target]];
#elif defined(__HAIKU__)
        auto *view = haiku::panel_bindings.object_from_handle(&owner)->view;
        BAutolock lock(view->Window());
        auto *picker = dynamic_cast<BPrivate::DateEdit *>(view->ChildAt(0));
        expect(picker, "Haiku shared-kit date editor exists");
        picker->SetDate(value.year, value.month, value.day); picker->Invoke();
#else
        owner.on_native_date(value);
#endif
    }
    void native_time(native::time_picker &owner, native::time_of_day value) {
#ifdef NATIVE_SETTINGS_GTK
        auto &peer = *native::detail::peer_state<linux::wayland::picker_state>(owner);
        if (peer.entry) {
            expect(std::string_view(G_OBJECT_TYPE_NAME(peer.entry)) == "GraniteTimePicker", "Granite time picker exists");
            auto *time = g_date_time_new_local(2000, 1, 1, value.hour, value.minute, value.second);
            g_object_set(peer.entry, "time", time, nullptr); g_date_time_unref(time);
            g_signal_emit_by_name(peer.entry, "time-changed");
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.second), value.second);
        } else {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.hour), value.hour);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.minute), value.minute);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(peer.second), value.second);
        }
#elif defined(_WIN32)
        HWND control = GetWindow(windows::wnd_bindings.handle_from_object(&owner), GW_CHILD);
        expect_padding(control);
        SYSTEMTIME time{}; time.wYear = 2000; time.wMonth = 1; time.wDay = 1;
        time.wHour = value.hour; time.wMinute = value.minute; time.wSecond = value.second;
        DateTime_SetSystemtime(control, GDT_VALID, &time);
        NMDATETIMECHANGE event{};
        event.nmhdr.hwndFrom = control; event.nmhdr.code = DTN_DATETIMECHANGE;
        event.dwFlags = GDT_VALID; event.st = time;
        SendMessageW(GetParent(control), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&event));
#elif defined(__APPLE__)
        auto *view = mac::panel_bindings.object_from_handle(&owner)->view;
        auto *field = [[view subviews] firstObject];
        auto *picker = static_cast<NSDatePicker *>([[field subviews] firstObject]);
        expect([picker isKindOfClass:[NSDatePicker class]], "AppKit time picker exists");
        auto *parts = [[[NSDateComponents alloc] init] autorelease];
        [parts setYear:2000]; [parts setMonth:1]; [parts setDay:1];
        [parts setHour:value.hour]; [parts setMinute:value.minute]; [parts setSecond:value.second];
        [picker setDateValue:[[picker calendar] dateFromComponents:parts]];
        [picker sendAction:[picker action] to:[picker target]];
#elif defined(__HAIKU__)
        auto *view = haiku::panel_bindings.object_from_handle(&owner)->view;
        BAutolock lock(view->Window());
        auto *picker = dynamic_cast<BPrivate::TimeEdit *>(view->ChildAt(0));
        expect(picker, "Haiku shared-kit time editor exists");
        picker->SetTime(value.hour, value.minute, value.second); picker->Invoke();
#else
        owner.on_native_time(value);
#endif
    }
    class runtime_window final : public native::app_wnd
    {
    public:
        runtime_window() : app_wnd("Settings acceptance", 0, 0, 720, 420),
            dates({2026, 12, 24}, native::rect(10, 10, 300, 34)),
            times({9, 30, 0}, native::rect(10, 10, 240, 34)) {
            dates.set_range({2026, 1, 1}, {2027, 12, 31}).set_disabled_dates({{2026, 12, 25}});
            first.on_wnd_create.connect([this] {
                for (native::wnd *control : std::initializer_list<native::wnd *>{
                        &dates, &entry, &choice, &editable}) attach(*control, first);
                return true;
            });
            second.on_wnd_create.connect([this] { attach(times, second); return true; });
            std::shared_ptr<const native::img> image;
#ifdef NATIVE_SETTINGS_GTK
            auto pixels = std::make_shared<native::img>(24, 24);
            std::fill_n(pixels->pixels(), 24 * 24, native::rgba(240, 20, 60, 255));
            image = pixels;
            on_wnd_resize.connect([this](native::size) {
                sidebar.set_bounds(get_client_bounds()); return true;
            });
#endif
#if defined(__APPLE__) || defined(_WIN32) || defined(__HAIKU__)
            sidebar << native::settings_page(1, "General", first, image, "Settings")
                    << native::settings_page(2, "Dates", second, {}, "Controls")
                    << native::settings_page(3, "Disabled", third, {}, "Settings", false);
#else
            sidebar << native::settings_page(1, "General", first, image)
                    << native::settings_page(2, "Dates", second)
                    << native::settings_page(3, "Disabled", third, {}, {}, false);
#endif
            on_wnd_create.connect([this] {
                attach(sidebar, *this);
#ifdef NATIVE_SETTINGS_GTK
                g_timeout_add(20, +[](gpointer data) -> gboolean {
                    return static_cast<runtime_window *>(data)->check_resize();
                }, this);
#else
                native::app::post([this] { check(); });
#endif
                return true;
            });
        }
        int failures = 0;
    private:
        native::panel first, second, third;
        native::date_picker dates;
        native::time_picker times;
        native::text_edit entry{"Baseline", native::text_edit_mode::single_line,
            native::rect(10, 66, 240, 40)};
        native::combo_box choice{{"One", "Two"}, native::combo_box_style::drop_down_list,
            native::rect(10, 116, 240, 40)};
        native::combo_box editable{{"One", "Two"}, native::combo_box_style::editable,
            native::rect(10, 166, 240, 40)};
        native::settings_sidebar sidebar{0, 0, 700, 400};
#ifdef NATIVE_SETTINGS_GTK
        int resize_stage = 0, resize_turns = 0;
        native::size initial_size;
        // Inspect native pixels, then let the compositor grow and shrink a
        // real window whose settings content follows its allocated bounds.
        gboolean check_resize() {
            try {
                if (++resize_turns > 250)
                    throw std::runtime_error("Settings window did not complete grow/shrink resize");
                auto *drawing = linux::wayland::state(*this).drawing;
                const int height = gtk_widget_get_height(drawing);
                if (!resize_stage) {
                    if (height < 420) return G_SOURCE_CONTINUE;
                    auto *rows = linux::wayland::find_widget(
                        linux::wayland::state(sidebar).widget, GTK_TYPE_LIST_BOX);
                    auto *row = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(rows), 0));
                    auto *picture = linux::wayland::find_widget(row, GTK_TYPE_PICTURE);
                    auto *image = linux::wayland::find_widget(row, GTK_TYPE_IMAGE);
                    auto *paintable = picture ? gtk_picture_get_paintable(GTK_PICTURE(picture))
                        : image ? gtk_image_get_paintable(GTK_IMAGE(image)) : nullptr;
                    expect(GDK_IS_TEXTURE(paintable), "navigator must present its supplied image");
                    auto *texture = GDK_TEXTURE(paintable);
                    expect(gdk_texture_get_width(texture) == 24 &&
                           gdk_texture_get_height(texture) == 24, "navigator image extent");
                    std::array<std::uint32_t, 24 * 24> pixels{};
                    gdk_texture_download(texture, reinterpret_cast<guchar *>(pixels.data()), 24 * 4);
                    expect(pixels.front() == 0xfff0143cU, "navigator native texture preserves image pixels");
                    initial_size = get_dimensions();
                    resize_stage = 1;
                    set_dimensions({native::dim(initial_size.w + 100), native::dim(initial_size.h + 180)});
                } else if (resize_stage == 1) {
                    if (height < int(initial_size.h) + 170) return G_SOURCE_CONTINUE;
                    resize_stage = 2;
                    set_dimensions(initial_size);
                } else {
                    if (height > int(initial_size.h) + 2) return G_SOURCE_CONTINUE;
                    expect(gtk_widget_get_width(drawing) <= int(initial_size.w) + 2,
                           "settings window can shrink horizontally too");
                    check();
                    return G_SOURCE_REMOVE;
                }
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n'; ++failures;
                destroy(); return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }
#endif
        static void attach(native::wnd &child, native::wnd &parent) {
            child.set_parent(&parent); child.create(); child.show();
        }
        void check() {
            try {
                const int input_height = native::theme::create(get_gpx())->get_text_edit_height();
                expect(dates.get_dimensions().h == input_height &&
                    entry.get_dimensions().h == input_height &&
                    choice.get_dimensions().h == input_height &&
                    editable.get_dimensions().h == input_height,
                    "native entries, combos and dates resolve to one theme height");
                const auto entry_position = entry.get_position();
                for (int pass = 0; pass < 2; ++pass)
                    entry.set_dimensions({240, 40});
                expect(entry.get_position().x == entry_position.x &&
                    entry.get_position().y == entry_position.y &&
                    entry.get_dimensions().h == input_height,
                    "repeated input sizing preserves position and standard height");
#ifdef _WIN32
                {
                    bool visible_island = false;
                    EnumChildWindows(windows::wnd_bindings.handle_from_object(&sidebar),
                        +[](HWND child, LPARAM data) -> BOOL {
                            wchar_t name[128]{};
                            GetClassNameW(child, name, 128);
                            if (std::wstring(name).find(L"DesktopChildSiteBridge") != std::wstring::npos && IsWindowVisible(child))
                                *reinterpret_cast<bool *>(data) = true;
                            return TRUE;
                        }, reinterpret_cast<LPARAM>(&visible_island));
                    expect(visible_island, "Windows settings navigation must show its native WinUI island");
                    HWND editor = windows::wnd_bindings.handle_from_object(&entry);
                    RECT client{}, frame{};
                    GetClientRect(editor, &client);
                    GetWindowRect(editor, &frame);
                    POINT text_origin{};
                    ClientToScreen(editor, &text_origin);
                    HDC dc = GetDC(editor);
                    HGDIOBJ previous = SelectObject(dc, windows::control_font());
                    TEXTMETRICW metrics{};
                    GetTextMetricsW(dc, &metrics);
                    SelectObject(dc, previous);
                    ReleaseDC(editor, dc);
                    expect(client.bottom == metrics.tmHeight &&
                        std::abs(2 * text_origin.y + client.bottom -
                            frame.top - frame.bottom) <= 1,
                        "Win32 single-line text and caret share the centered native client");
                }
#endif
#ifdef __HAIKU__
                auto *navigation_host = haiku::panel_bindings.object_from_handle(&sidebar)->view;
                {
                    BAutolock lock(navigation_host->Window());
                    for (int32 index = 0; auto *child = navigation_host->ChildAt(index); ++index)
                        if (auto *scroll = dynamic_cast<BScrollView *>(child);
                            scroll && dynamic_cast<BOutlineListView *>(scroll->Target()))
                        {
                            expect(scroll->ScrollBar(B_VERTICAL)->IsHidden(scroll->ScrollBar(B_VERTICAL)),
                                   "Haiku settings navigation hides an unnecessary native scrollbar");
                            const auto frame = scroll->Frame();
                            expect(scroll->Border() == B_FANCY_BORDER && frame.left >= 6 &&
                                navigation_host->Bounds().bottom - frame.bottom >= 6,
                                "Haiku navigation keeps its frame and left/bottom panel margins");
                        }
                }
#endif
                sidebar.set_search_visible(true).set_search_text("absent");
                expect(first.get_created() && sidebar.get_selected_item() == 1,
                    "filtering every navigation row retains the active page");
                sidebar.set_search_text("");
#ifdef __APPLE__
                auto *host = mac::panel_bindings.object_from_handle(&sidebar)->view;
                auto *material = [[host subviews] firstObject];
                expect([material isKindOfClass:[NSVisualEffectView class]] &&
                    [(NSVisualEffectView *)material material] == NSVisualEffectMaterialSidebar,
                    "native sidebar material");
                NSSearchField *search = nil;
                NSOutlineView *outline = nil;
                for (NSView *view in [material subviews]) {
                    if ([view isKindOfClass:[NSSearchField class]]) search = (NSSearchField *)view;
                    if ([view isKindOfClass:[NSScrollView class]]) outline = (NSOutlineView *)[(NSScrollView *)view documentView];
                }
                expect(search && outline && [outline style] == NSTableViewStyleSourceList,
                    "AppKit search and source-list controls");
                [search setStringValue:@"Dates"];
                [[search delegate] controlTextDidChange:[NSNotification notificationWithName:NSControlTextDidChangeNotification object:search]];
                expect(sidebar.get_search_text() == "Dates" && [outline numberOfRows] == 2,
                    "native search edits filter the AppKit source list");
                [search setStringValue:@""];
                [search sendAction:[search action] to:[search target]];
                expect(sidebar.get_search_text().empty(), "native search/cancel actions update the query");
                expect([outline numberOfRows] == 6 && [[outline delegate] outlineView:outline
                    isGroupItem:[outline itemAtRow:0]], "native section rows, including repeated section names");
                auto *date_host = mac::panel_bindings.object_from_handle(&dates)->view;
                auto *field = (NSTextField *)[[date_host subviews] firstObject];
                auto *picker = (NSDatePicker *)[[field subviews] firstObject];
                expect([[date_host subviews] count] == 1 &&
                    [field isKindOfClass:[NSTextField class]] && [field isBezeled] &&
                    ![picker isBezeled] && [field frame].size.width < 220,
                    "one compact native field encloses the date editor and accessories");
                auto *standard = [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
                [standard setStringValue:@"Standard text"];
                const CGFloat standard_height = std::ceil([standard intrinsicContentSize].height);
                const NSRect date_editor = [picker alignmentRectForFrame:[picker frame]];
                expect([picker datePickerStyle] == NSDatePickerStyleTextField &&
                    [field frame].size.height == standard_height &&
                    std::abs(NSMidY([field frame]) - NSMidY([date_host bounds])) <= 0.5 &&
                    std::abs(NSMaxY([picker frame]) - [picker alignmentRectInsets].bottom -
                        [picker lastBaselineOffsetFromBottom] - standard_height +
                        [field lastBaselineOffsetFromBottom]) <= 0.5,
                    "date has no stepper and shares the standard text-field baseline");
                // A bezeled NSTextField can fold its default background into
                // native bezel painting rather than its drawsBackground flag.
                if ([picker drawsBackground] ||
                    ![[field backgroundColor] isEqual:[NSColor controlColor]])
                    throw std::runtime_error("picker background: field=" +
                        std::to_string([field drawsBackground]) + " editor=" +
                        std::to_string([picker drawsBackground]) + " color=" +
                        std::string([[[field backgroundColor] description] UTF8String]));
                auto *calendar_button = (NSButton *)[[field subviews] lastObject];
                expect([calendar_button isKindOfClass:[NSButton class]] &&
                    ![calendar_button isBordered] &&
                    NSContainsRect([field bounds], [calendar_button frame]) &&
                    !NSIntersectsRect(date_editor, [calendar_button frame]),
                    "calendar trigger stays inside the date field beside its editor");
                const NSPoint trigger_point = [calendar_button convertPoint:
                    NSMakePoint(NSMidX([calendar_button bounds]),
                        NSMidY([calendar_button bounds])) toView:date_host];
                auto *hit = [field hitTest:trigger_point];
                expect(hit == calendar_button || [hit isDescendantOf:calendar_button],
                    "native field hit testing reaches its internal calendar action");
                [calendar_button performClick:nil];
                auto *popover = (NSPopover *)[[calendar_button target] valueForKey:@"popover"];
                expect([popover isShown], "calendar button opens a native popover");
                auto *calendar = (NSDatePicker *)[[popover contentViewController] view];
                expect([calendar datePickerStyle] == NSDatePickerStyleClockAndCalendar,
                    "calendar popover contains AppKit's calendar picker");
                auto *holiday = [[[NSDateComponents alloc] init] autorelease];
                [holiday setYear:2026]; [holiday setMonth:12]; [holiday setDay:25];
                [calendar setDateValue:[[calendar calendar] dateFromComponents:holiday]];
                [calendar sendAction:[calendar action] to:[calendar target]];
                expect(dates.get_date() == native::date{2026, 12, 24},
                    "native calendar popover applies the same holiday policy");
                [popover close];
#endif
                int changed = 0;
                dates.on_change.connect([&](native::date) { ++changed; return false; });
                native_date(dates, {2026, 12, 25});
                expect(dates.get_date() == native::date{2026, 12, 24} && changed == 0, "native holiday selection rolls back");
                native_date(dates, {2026, 12, 28});
                expect(dates.get_date() == native::date{2026, 12, 28} && changed == 1, "native callback commits an enabled date");
                dates.set_disabled_dates({dates.get_date()});
                native_date(dates, dates.get_date());
                expect(!dates.get_valid() && changed == 1, "invalidated cached native date does not loop or coerce");
                dates.set_disabled_dates({});
#ifdef NATIVE_SETTINGS_GTK
                auto *rows = linux::wayland::find_widget(linux::wayland::state(sidebar).widget, GTK_TYPE_LIST_BOX);
                expect(rows, "native sidebar navigation exists");
                gtk_list_box_select_row(GTK_LIST_BOX(rows), gtk_list_box_get_row_at_index(GTK_LIST_BOX(rows), 1));
#elif defined(__APPLE__)
                [outline selectRowIndexes:[NSIndexSet indexSetWithIndex:[outline rowForItem:@2]] byExtendingSelection:NO];
#else
                sidebar.on_native_selection(2);
#endif
                expect(sidebar.get_selected_item() == 2 && second.get_created() && !first.get_created() && !third.get_created(),
                    "selected-page creation and inactive-page teardown");
                expect(times.get_dimensions().h == input_height,
                    "native time inputs use the same standard height");
#ifdef __APPLE__
                auto *time_host = mac::panel_bindings.object_from_handle(&times)->view;
                auto *time_field = (NSTextField *)[[time_host subviews] firstObject];
                auto *time_picker = (NSDatePicker *)[[time_field subviews] firstObject];
                const NSRect time_editor = [time_picker alignmentRectForFrame:[time_picker frame]];
                expect([[time_host subviews] count] == 1 &&
                    [time_field isKindOfClass:[NSTextField class]] &&
                    [time_field isBezeled] && [[time_field subviews] count] == 1 &&
                    [time_picker datePickerStyle] == NSDatePickerStyleTextFieldAndStepper &&
                    [time_picker controlSize] == NSControlSizeSmall &&
                    [time_field frame].size.height == standard_height &&
                    std::abs(NSMidY([time_field frame]) - NSMidY([time_host bounds])) <= 0.5 &&
                    NSMinY([time_picker frame]) >= 0 &&
                    std::abs(NSMaxX(time_editor) - NSMaxX([time_field bounds])) <= 0.5 &&
                    std::abs(NSMaxY([time_picker frame]) - [time_picker alignmentRectInsets].bottom -
                        [time_picker lastBaselineOffsetFromBottom] - standard_height +
                        [time_field lastBaselineOffsetFromBottom]) <= 0.5,
                    "time shares the text-field baseline and its stepper meets the right edge");
#elif defined(__HAIKU__)
                {
                    auto *host = haiku::panel_bindings.object_from_handle(&times)->view;
                    BAutolock lock(host->Window());
                    auto *time = dynamic_cast<BPrivate::TimeEdit *>(host->ChildAt(0));
                    BTextControl standard("measure", nullptr, "", nullptr);
                    expect(time && time->Bounds().Height() == standard.PreferredSize().height &&
                        time->SectionArea().Width() >= time->CountSections() * time->StringWidth("00") + 10,
                        "Haiku time text fits at standard text-control height");
                }
#endif
                times.set_seconds_visible(true).set_minute_step(15);
                expect(!times.on_native_time({9, 31, 0}), "live time rules reject literal off-step proposals");
                native_time(times, {9, 45, 30});
                expect(times.get_time() == native::time_of_day{9, 45, 30}, "native time callback commits fields");
                sidebar.set_selected_item(1).set_sidebar_width(180);
                expect(dates.get_created() && dates.get_date() == native::date{2026, 12, 28}, "page recreation retains date");
                sidebar.set_selected_item(0);
                expect(!first.get_created() && !second.get_created(), "explicit empty selection hides all pages");
                sidebar.clear_items();
                sidebar << native::settings_page(1, "Re-added", first);
                sidebar.destroy(); sidebar.create(); sidebar.show();
                expect(first.get_created() && dates.get_created(), "sidebar and native picker recreate");
                sidebar.clear_items();
                native::date_picker unrestricted;
                attach(unrestricted, *this);
                expect(unrestricted.get_created(), "default full date range creates without materializing all years");
#ifdef NATIVE_SETTINGS_GTK
                auto &peer = *native::detail::peer_state<linux::wayland::picker_state>(unrestricted);
                if (GTK_IS_EDITABLE(peer.entry)) {
                    expect(*gtk_editable_get_text(GTK_EDITABLE(peer.entry)), "initial native date entry is populated");
                    expect(gtk_editable_get_width_chars(GTK_EDITABLE(peer.entry)) >= 10,
                           "native date entry reserves space for its formatted value");
                    expect(gtk_editable_get_editable(GTK_EDITABLE(peer.entry)), "native date entry permits typing");
                    gtk_editable_set_text(GTK_EDITABLE(peer.entry), "2026-12-23");
                    g_signal_emit_by_name(peer.entry, "activate");
                    expect(unrestricted.get_date() == native::date{2026, 12, 23}, "typed date commits through the native entry");
                }
#elif defined(__HAIKU__)
                auto *view = haiku::panel_bindings.object_from_handle(&unrestricted)->view;
                BAutolock lock(view->Window());
                expect(view->ChildAt(0)->Bounds().Width() < 180 &&
                    view->ChildAt(0)->Bounds().Height() < 32,
                    "Haiku date field stays compact at standard input height");
                auto *date = dynamic_cast<BPrivate::DateEdit *>(view->ChildAt(0));
                auto *calendar = dynamic_cast<BButton *>(date->ChildAt(0));
                BTextControl standard("measure", nullptr, "", nullptr);
                expect(view->CountChildren() == 1 && calendar &&
                    date->Bounds().Contains(calendar->Frame()) &&
                    calendar->Bounds().Width() + 1 <= 14,
                    "Haiku calendar trigger is inside the single date field");
                expect(date->Bounds().Height() == standard.PreferredSize().height &&
                    date->SectionArea().Width() >= 3 * date->StringWidth("0000") + 20,
                    "Haiku date fits complete year fields at standard text-control height");
                const auto compact_width = date->Bounds().Width();
                for (native::dim width : {340, 80}) {
                    unrestricted.set_dimensions({width, unrestricted.get_dimensions().h});
                    expect(date->Bounds().Width() <= compact_width &&
                        date->Bounds().Width() <= width - 1,
                        "wide allocations do not stretch dates and narrow allocations still clip");
                    calendar->Invoke();
                    expect(calendar->Value() == B_CONTROL_ON,
                        "calendar arrow stays highlighted while its dropdown is open");
                    BWindow *popup = nullptr;
                    for (int32 index = 0; auto *candidate = be_app->WindowAt(index); ++index)
                        if (std::string_view(candidate->Title()) == "Choose date") popup = candidate;
                    expect(popup, "Haiku date opens its native calendar dropdown");
                    BAutolock popup_lock(popup);
                    const auto frame = popup->Frame();
                    const auto anchor = date->ConvertToScreen(date->Bounds());
                    expect(frame.Width() >= anchor.Width() && frame.left == anchor.left &&
                        frame.top == anchor.bottom + 2,
                        "Haiku calendar opens below the field and never becomes narrower");
                    auto *days = dynamic_cast<BPrivate::BCalendarView *>(popup->FindView("native_calendar"));
                    expect(days && days->Bounds().Width() >= 7 * (days->StringWidth("00") + 8) + 8,
                        "narrow date fields retain legible calendar day columns");
                    auto *previous = dynamic_cast<BButton *>(popup->FindView("previous"));
                    auto *next = dynamic_cast<BButton *>(popup->FindView("next"));
                    expect(previous && next &&
                        previous->Bounds().Width() == next->Bounds().Width() &&
                        previous->Bounds().Width() <= standard.PreferredSize().height &&
                        previous->Bounds().Height() == next->Bounds().Height(),
                        "calendar month buttons are equal compact squares");
                    const int month = days->Month();
                    expect(next->Target() == popup && previous->Target() == popup,
                        "native month buttons target their calendar window");
                    popup->MessageReceived(next->Message());
                    expect(days->Month() == month % 12 + 1, "next month button advances the native calendar");
                    popup->MessageReceived(previous->Message());
                    expect(days->Month() == month, "previous month button restores the native calendar");
                    calendar->Invoke();
                    expect(calendar->Value() == B_CONTROL_OFF,
                        "a second calendar action closes the dropdown and clears its highlight");
                }
                lock.Unlock();
                set_bounds(native::rect(150, 150, 940, 700));
                const auto centered = [this](native::file_dialog &dialog) {
                    dialog.create(); dialog.show();
                    auto *panel = haiku::file_dialog_bindings.object_from_handle(&dialog)->panel;
                    BRect frame;
                    { BAutolock panel_lock(panel->Window()); frame = panel->Window()->Frame(); }
                    const auto parent = haiku::wnd_bindings.handle_from_object(this)->Frame();
                    const bool correct = std::abs(frame.left + frame.right - parent.left - parent.right) <= 2 &&
                        std::abs(frame.top + frame.bottom - parent.top - parent.bottom) <= 2;
                    // Tracker enumerates the directory on its own worker.
                    // Let that native work settle before destroying its panel.
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    dialog.on_native_cancel();
                    expect(correct, "Haiku file panels center after native Show placement");
                };
                native::open_file_dialog open(*this); centered(open);
                native::save_file_dialog save(*this); centered(save);
                native::directory_dialog directory(*this); centered(directory);
#elif defined(_WIN32)
                RECT frame{};
                GetWindowRect(GetWindow(windows::wnd_bindings.handle_from_object(&unrestricted), GW_CHILD), &frame);
                expect(frame.right - frame.left < 220 && frame.bottom - frame.top < 32,
                       "Windows date field keeps its native ideal size");
#endif
                unrestricted.destroy();
                std::cout << "Settings native runtime checks passed\n";
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n'; ++failures;
            }
            destroy();
        }
    };
}
int program(int argc, char **argv) {
    try {
        models();
        if (argc > 1 && std::string_view(argv[1]) == "--runtime") {
            runtime_window window;
            const int result = native::app::run(window);
            return result ? result : window.failures;
        }
        std::cout << "Settings model checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAILED: " << error.what() << '\n'; return 1;
    }
}
