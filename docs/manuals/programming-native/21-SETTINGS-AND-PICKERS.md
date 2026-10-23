# Chapter 21: Settings, Dates, and Times

Build a settings window in three steps: create page objects, describe the
navigation, and create the sidebar after its parent. This chapter is a complete
program; it needs no external images or holiday database.

## Step 1: Keep pages and their controls alive

A `settings_sidebar` borrows its pages. Declare the pages before the sidebar
so they outlive it. The sidebar creates the selected page and destroys that
page's native resources when leaving it. Keep the C++ controls alive and create
their native resources in the page's `on_wnd_create` handler on every visit.

## Step 2: Describe navigation and availability

Use positive stable page IDs with `<<` or `add_item()`. Optional images are
shared immutable `img` objects; section titles group related pages. Search is
optional and filters titles and section names without closing the active page.

Use `date_picker` for civil dates and `time_picker` for wall-clock times.
They have no time zone. Set explicit ranges, excluded dates, weekdays, and pure
predicates to express your application's rules. Native does not choose a
country's holidays for you.

## Step 3: Run the window

```cpp
//
// Builds searchable settings pages with native-first date and time fields.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <memory>

class booking_window : public native::app_wnd {
public:
    // Configure page models and connect their repeatable creation handlers.
    booking_window() : app_wnd("Booking settings", 80, 80, 760, 460) {
        _help.set_read_only(true);
        _help.set_border_sides(native::border_sides::none);
        _summary.set_read_only(true);
        auto icon = std::make_shared<native::img>(24, 24);
        icon->get_gpx().clear(native::rgba(0, 0, 0, 0))
            .set_ink(native::rgba(45, 100, 170, 255))
            .draw_ellipse(native::rect(3, 3, 18, 18), true);
        _navigation << native::settings_page(
            1, "General", _general, icon, "Settings")
                    << native::settings_page(
            2, "Appointment", _appointment, icon, "Booking");
        _navigation.set_search_visible(true);
        _date.set_range({2026, 1, 1}, {2026, 12, 31});
        _date.set_disabled_dates({{2026, 12, 25}});
        _date.set_disabled_weekdays({native::day_of_week::saturday,
                                     native::day_of_week::sunday});
        _time.set_range({8, 0, 0}, {18, 0, 0}).set_minute_step(15);
        _time.set_validator([](native::time_of_day value) {
            return value.hour != 12; // No appointments during lunch.
        });
        _general.on_wnd_create.connect([this] {
            attach(_help, _general);
            return true;
        });
        _appointment.on_wnd_create.connect([this] {
            attach(_date, _appointment);
            attach(_time, _appointment);
            attach(_summary, _appointment);
            update_summary();
            return true;
        });
        _date.on_change.connect([this](native::date) {
            update_summary();
            return true;
        });
        _time.on_change.connect([this](native::time_of_day) {
            update_summary();
            return true;
        });
        on_wnd_create.connect([this] {
            _navigation.set_bounds(get_client_bounds());
            attach(_navigation, *this);
            return true;
        });
        on_wnd_resize.connect([this](native::size) {
            _navigation.set_bounds(get_client_bounds());
            return true;
        });
    }

private:
    // Parent pages precede their controls; the borrowing sidebar comes last.
    native::panel _general, _appointment;
    native::text_edit _help{
        "Choose Appointment to book a weekday.\n"
        "Christmas Day and lunchtime are unavailable.",
        native::text_edit_mode::multi_line, 16, 16, 440, 100};
    native::date_picker _date{{2026, 12, 24}, {16, 16, 220, 24}};
    native::time_picker _time{{9, 30, 0}, {16, 64, 160, 24}};
    native::text_edit _summary{"", native::text_edit_mode::single_line,
                               16, 112, 440, 24};
    native::settings_sidebar _navigation;

    // Create one borrowed child after its parent has native resources.
    static void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    // Read the cached civil values after an accepted user change.
    void update_summary() {
        _summary.set_text(_date.get_date().to_string() + " at " +
                          _time.get_time().to_string(false));
    }
};

// Keep the complete object hierarchy alive throughout the event loop.
int program(int, char **) {
    booking_window window;
    return native::app::run(window);
}
```

Open Appointment, select a weekday, try Christmas Day, and type a time outside
08:00–18:00. Accepted user changes update the summary; unavailable proposals
restore the previous value. Search for `booking`, then clear the query and
switch pages. Values survive the round trip.

## Change rules and observe results

These are additions inside `booking_window`, not separate programs:

```cpp
_date.set_validator([](native::date value) {
    return !(value.month == 12 && value.day == 31);
});
_time.set_seconds_visible(true);
_date.on_rejected.connect([](native::date value) {
    // Display an application-specific explanation for value if required.
    (void)value;
    return true;
});
```

Value setters (`set_date`, `set_time`) reject unavailable values with
`std::invalid_argument` and emit no user signal. Changing rules preserves the
cached value, which may now be unavailable: check `get_valid()` and decide how
your application handles that state. Test a candidate with `is_date_enabled()`
or `is_time_enabled()` before assigning it. A minute step must divide 60;
hiding seconds preserves their cached value. Range endpoints are inclusive.

`set_selected_item(id)` and `set_search_text(query)` are silent. User navigation
emits `on_selection_change(int id)`; user search emits `on_search_change`.
`set_item_enabled`, `set_item_text`, `set_item_image`, `remove_item`, and
`clear_items` also work after creation. Remove a page before deleting it.

## Native presentation

Use these public controls rather than drawing replacement pickers. Windows
uses WinUI 3 NavigationView and its search control; macOS uses an AppKit source
list and search field. Elementary uses Granite where its API matches. Other
adapters reuse their toolkit's text, tree, button, and calendar controls where
available. Image support depends on the adapter; text-only navigation remains
usable without images. The exact native/hybrid/custom classifications are in
the [control audit](../../notes/CUSTOM.md).

Single-line fields share the active theme's standard input height. Mac dates
have a calendar trigger inside the field and no date stepper. Motif time
arrows change the selected component or caret component; both buttons are
square and use native Motif relief. Motif dates use the same native combo
border and arrow as editable choices. Click the arrow again or click outside
the calendar to dismiss it after typing or selecting a date. Settings check
labels align left, and search/navigation use equal outside margins and gap. Window Maker,
Motif, GEM, and OPEN LOOK composed dates use `DD.MM.YYYY`; Athena uses `YYYY-MM-DD`. Display formatting follows
other native platforms.
Use `date::to_string()` for unambiguous `YYYY-MM-DD` storage, independently of
the field's presentation.

GEM and Athena fit picker widths to their formatted text with half-character
left and right text margins. Enabling seconds adjusts the time field width.
The calendar can be wider than the compact field to keep day cells readable.
Athena's calendar retains the normal arrow cursor and its time-step buttons
are equal squares. Motif keeps its normal native date-field style and frames
the calendar popup itself.

GEM keeps the editable-combo border around date and time text. Its equal
square time buttons change the hour, minute or second at the caret or start
of the selected text. Search and navigation have equal six-pixel outside
margins and separation, with a smaller inset inside the navigation frame.

SDL date and time fields use compact text-fitting widths within their
allocated bounds; enabling seconds expands the time field as needed. Its
time stepper has two equal square buttons with matching up/down arrows.
The settings search field, its gap to the navigator, and the top, left and
bottom margins use the same six-pixel spacing.

Try the larger acceptance window with `vision --settings`. See
[Chapter 12](12-BUILDING-AND-DISTRIBUTING.md) for the WinUI runtime dependency,
and [Chapter 22](22-TOOLBARS-AND-PROPERTIES.md) for commands and inspectors.


Athena search includes a native `Search:` prompt. The search row and framed
navigator use equal six-pixel outside margins and separation. Its date popup
uses native calendar day buttons with insensitive excluded dates. Time's two
equal square arrow buttons share a half-height column at the right edge and adjust the
selected hour, minute, or second while retaining that selection.

Window Maker settings use a native WINGs list with fixed bold category labels,
optional page icons and compact indentation. Categories do not collapse;
search/list spacing matches the outer margins. Its date field fits the date
text and arrow, and the calendar has a border and is at least as wide as that
field. OPEN LOOK search has a Search: prompt, and nested page creation keeps
native date/time controls above their page background.


OPEN LOOK and Window Maker pickers also fit their text within a wider layout
allocation. OPEN LOOK reserves a character margin on each side of the value;
its time arrows continue to adjust the selected hour, minute or second across
successive clicks. Search and navigator spacing is measured from their native
rectangles, so their gap matches the outside margins. Motif uses compact
text/arrow fields and a framed calendar popup.
