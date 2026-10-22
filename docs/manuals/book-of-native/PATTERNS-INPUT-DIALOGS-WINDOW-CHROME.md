# Patterns: Input, Standard Dialogs, and Window Chrome

GTK editable combos retain the native entry's caret and selection when
synchronizing a typed exact match with its popup. Ordinary keystrokes do not
rewrite unchanged text or reset an unchanged active index. Windows date/time
pickers retain their native segmented editors, with literal space fields
before and after the value; dates read the current locale's short-date pattern.
Property-grid outer right edges occupy a separate one-pixel canvas outside
the scrolling strip, including its native scrollbar painting.

Ruler paper and shared corners use the distinct `ruler` and `ruler_corner`
surface roles. Motif gives both native menu-bar paper, with menu-foreground
ticks, labels and edge marks; other backends retain header/panel defaults.
Native Motif modal windows inherit `XmDialogShell`
colors just like message boxes, including the foreground. Custom dialog
labels should obtain `theme::get_button_foreground_color()`, not hard-code
black; Vision's modal demonstration follows that rule.

This chapter describes the portable contracts for combo boxes, list boxes,
standard directory and message dialogs, and edge-attached rulers and status
bars. Public objects keep their state in C++; each backend adapts that state to
its native widget or to its existing native-theme renderer.

Single-line editors, both combo styles and date/time pickers use the same
active-theme input height with vertically centered content. A taller bounds
allocation centers that standard-height field; multiline editors still fill
their bounds. A status bar always occupies the outermost bottom strip, so
bottom toolbars appear above it regardless of construction order. Setting a
status bar's edge to another edge throws `invalid_argument`.

## Settings navigation and civil pickers

`settings_sidebar` is a composite navigation and page host, with the same
borrowed-content convention as `tab_view`. A `settings_page` holds a positive
stable ID, title, uncreated content window, optional shared image, section
and enabled flag. Stream insertion and `add_item()` share the same model.
Selection uses IDs, never native row numbers. Zero clears selection. Mutation
functions update native presentation without emitting user signals.

Only the selected enabled page has native resources. Its `on_wnd_create`
handler recreates child controls on each visit; their C++ values survive.
Pages outlive the sidebar or are explicitly removed before deletion. Removing
or clearing destroys native resources and detaches pages without deleting
their objects. The requested sidebar width and `get_content_bounds()` define
the logical geometry. Sections are native headings or non-selectable text
rows; disabled proposals restore the previous selection.

Search is optional and hidden by default. `set_search_visible(true)` shows a
query editor; `set_search_text()` silently filters titles and section names
with the shared case-insensitive UTF-8 substring matcher. The selected page
stays open even when its row is filtered out or no rows match. Hiding search
suspends filtering while retaining the query. User edits emit `on_search_change`
after refresh. The Mac adapter uses `NSSearchField`; Windows uses WinUI
`AutoSuggestBox`. Other adapters reuse the existing native text editor above
navigation.

Granite navigation uses `GraniteSettingsSidebar` and a private `GtkStack` of
metadata pages. A private concrete GType derives from Granite's abstract page
class; no Vala source or native type enters the public API. Native manages the
real page windows. Its native sidebar CSS class enables the installed
stylesheet's row padding and colors. Granite 7's row grid receives a
six-logical-pixel icon/title gap only when its native column spacing is zero.
macOS has a dedicated `NSOutlineView` source list with native group rows,
`NSVisualEffectMaterialSidebar` and `NSSearchField`. Its reusable cells contain
AppKit text and image views; AppKit draws selection and focus. Workspace icons resolve lazy native image
representations into a Quartz premultiplied buffer, then convert to the
portable straight-RGBA format; an empty render uses the existing fallback. Other backends
reuse native image trees (Haiku outline, Motif container, GTK rows). GTK rows
create native texture-backed image widgets for optional page icons. The fixed
page host is clipped and excluded from overlay measurement, allowing a
client-filling sidebar to shrink after the window has been enlarged. Athena, XView and
WINGs instead use native text lists: their tree adapters require painting,
while the sidebar's flat navigation can remain native. These text lists
retain optional image ownership but display text only. SDL2 and GEM reuse
the existing themed tree and its input/scrolling contract.

Windows hosts the actual WinUI 3 `NavigationView` in a
`DesktopWindowXamlSource` island inside the sidebar panel. The peer owns the
island, native section/item objects, optional `AutoSuggestBox`, and image
buffers. An opaque UI-thread session initializes the App SDK bootstrap,
XAML application/resources and dispatcher. Native input is pretranslated by
WinUI; selection/search delivery is deferred and guarded against destroyed
windows. Events are revoked and islands closed before their parent HWNDs;
the UI session shuts down after the application loop. No WinUI types enter
the public API. The build and distribution dependency is documented in
[Build System](BUILD-SYSTEM.md#windows-vm-preparation).

`date_picker` and `time_picker` derive from `panel`; each keeps its values
and rules in portable C++. The panel's one opaque peer owns native picker
state. Dedicated widgets are hosted as native children of the panel, so
existing geometry, clipping and parent teardown remain applicable.
Granite supplies date/time widgets (a GTK seconds field extends its minute
editor); AppKit and Win32 supply their native date/time pickers. Haiku uses
the installed shared-kit `DateEdit`/`TimeEdit` through private SDK headers,
`libshared` and `localestub`, isolated in its platform adapter. Generic GTK
uses a native calendar/popover and spin fields. Other toolkits compose the
existing year entry and month/day choices, or hour/minute/second choices.
A numeric year entry avoids materializing the entire 1601..9999 range.
No new calendar painter or input dispatcher exists. AppKit editors retain
one enclosing field at the standard NSTextField intrinsic height, using the
native adaptive control background. AppKit baseline metrics align text with
a standard field after applying the native editor height. The enclosing field
clips native decoration; the time stepper's alignment edge meets its right
edge without an added gutter. Calendar image-cell padding is accounted for
separately so the painted symbol centers vertically. Dates use a text-only
editor without a stepper and include a borderless calendar-symbol button
inside that same field and
an `NSPopover` containing AppKit's calendar-style `NSDatePicker`; the same
validation applies to both. Time retains its native stepper inside the same
standard-height field.
Borderless read-only AppKit text fields/views draw no background, allowing
help and status text to share their containing page's native surface.

Dates are Gregorian civil fields in 1601..9999; times are wall-clock fields
within one day. They carry no time zone. AppKit's private UTC Gregorian
conversion preserves these fields through DST transitions without exposing
timestamps. Standard C++ arithmetic implements calendar validation and weekday
calculation because some maintained cross-toolchains lack chrono calendars.

Date availability combines inclusive limits, explicit dates, weekday
exclusions and a pure application predicate. Time availability combines
inclusive same-day limits, a minute step dividing 60 and a predicate.
Applications can define annual holidays, booking calendars or office hours
without adding country-specific policy to Native. Predicates run on the UI
thread; exceptions mean unavailable. Changing rules never silently moves the
cached value. `get_valid()` reports a value invalidated by new rules.

Value setters throw for unavailable proposals and remain silent. Native
proposals are validated before committing; rejection restores the native
widget and emits `on_rejected`, while accepted changes emit once after the
cache is updated. A native one-minute stepper move is translated to the
configured minute step. Hidden seconds are retained. GTK marks unavailable
calendar dates and explains the marks; native calendars without per-day
disabling reject selection, and composed day choices omit excluded dates.

Granite's date property can queue notifications until an active notification
handler returns. The adapter compares civil fields before setting it; a
temporary suppression flag alone would allow endless notification replay.
The SDK includes `gsettings-desktop-schemas` because Granite's time picker
reads `org.gnome.desktop.interface` to choose its clock format.

Vision's **Settings and pickers** window (`--settings`) exercises these
contracts with optional filesystem images, independent date/time pages,
weekend and annual-holiday toggles, explicit excluded dates, office-hour
limits, minute steps, seconds visibility and accepted/rejected status text.

## Combo boxes and list boxes

Athena lists synchronize their native column width with the viewport for
full-row selection. Its composite combo keeps its assigned Form dimensions,
and changing the arrow bitmap cannot shrink the MenuButton below the field
height. Pre-show graphics support status-metric lookup during creation.

GEMix paints open combo lists as the final overlay, inside a separate one-pixel
frame, so ordinary lists or editors below cannot cover popup choices. Its AES
menu tree uses the conventional root siblings for the bar and popup container.
File selectors remain synchronous AES dialogs; the hosted GEM runtime restores
exposed caller pixels while a selector moves. Native supplies buffers large
enough for the GEM selector's path/name contract and releases pressed-button
feedback before entering a blocking dialog.
Selector buttons themselves remain inverted while held and activate only on
release inside the original button. GEM list selection invalidates the list's
own region rather than relying on an application callback to repaint it.
Window destruction detaches its menu while AES/VDI are still live; AES removes
the bar and restores popup pixels before their backing tree is released.
GEM modal windows and AES message boxes use untitled `1011` enclosures
(black/white/black/black from outside inward). File selectors retain their
native titled, movable AES hosts.

`combo_box` supports `drop_down_list` and `editable` styles. Its item model,
selected index, and displayed text remain available before creation and after
native resources are destroyed. An index of `-1` means no list item is
selected. Selection-only text must identify an existing item; editable text
may contain any UTF-8 value.

Programmatic setters are silent. Native user actions first update the cache,
then enter the protected-compatible virtual event hooks
`on_native_selection()`, `on_native_text()`, and `on_native_drop_down()`.
Their base implementations emit `on_selection_change`, `on_text_change`, and
`on_drop_down`. Derived controls can add behavior and call the base hook when
the normal signal is still wanted.

Backends use their standard combo widget where one exists: Win32 `COMBOBOX`,
AppKit `NSComboBox`, Motif `XmComboBox`, WINGs popup and
text controls, and XView choice and text controls. Athena composes its standard
text, menu-button, and menu widgets. SDL2 and GEMix route the control through
their existing backend-owned theme and input systems.
Athena's text and arrow borders overlap by one pixel, leaving one divider.
Text margins follow the actual text-sink font metrics for vertical centering.
Its bordered popup anchors to the complete field, not the arrow, and menu
entries have full-width hit areas. Ordinary pointer motion highlights the
hovered entry without committing selection. Both click-then-click and
press/drag/release selection work. Selection updates the native text field before notifying C++
subscribers; selection-only fields also open when their text is clicked.
Athena menu titles and popups retain their individual borders; no extra rule
spans the unused menu-bar width. Menu callbacks detach before their owning
widget hierarchy is destroyed. Message dialogs center their native Command
button row using Form constraints, including after resize. Before mapping,
their shell centers over the owner's current screen position, falling back
to the X screen center when the owner is hidden. Requested shell bounds are
clamped to that screen, and center gravity preserves the placement when the
window manager adds decorations. They use X11-owned
copies of GEM's 32-pixel monochrome alert artwork, supplied to the native Xaw
Dialog icon resource; there are no GEM headers, libraries or runtime assets
in that path. Information/warning use the exclamation, question uses the
question mark, and error uses the stop sign.
Haiku composes native buttons, `BTextControl`, and `BPopUpMenu`. The editable
arrow child shares the editor paper and draws only the common native arrow;
it adds no second menu-field background or inner bezel. Its narrow hit area
centers the glyph and uses the native pressed background during pointer
tracking and while the menu is open. Native menu tracking latches a second
trigger click as dismissal, including quick repeat clicks. A selection-only
button paints through the system menu-field primitives;
editable mode places its arrow child inside one full-width native text border,
reserving editor space to keep text clear of the arrow. Both popup styles
open immediately below the field and match its width, rather than following
`BMenuField`'s selected-item overlay positioning. Presentation switching tests each
child's own hidden state, not inherited window visibility, and editing targets
are assigned after attachment to the window looper.
In Window Maker editable mode, the arrow-only native popup button is inset at
the right edge of the full-width native text field. It does not consume a
second control-sized region or extend beyond the field. Its WINGs menu remains
the width of the complete combo; the backend shapes only the visible and
clickable closed-button region down to the arrow and centers the disclosure
glyph within that region. Press and release repaint that same centered geometry
on an input-transparent overlay, so the native popup's synchronous selection
animation cannot reveal or leave its own off-center indicator behind.

The emulated interaction follows desktop combo conventions. The indicator is
a compact filled downward arrow on a content-colored button which remains
inset within the continuous outer frame. A selection-only field toggles from
the whole field; an editable field focuses text from its content and toggles
from the arrow.
Popups prefer the space below, fall back above, paint after ordinary sibling
controls, receive pointer input before any sibling they cover, retain a
complete one-pixel frame above their rows, and consume the outside click that
dismisses them. Both the closed value and popup rows select the stock control
font explicitly, so a prior sibling paint or a selection change cannot alter
their text size.

SDL2's fully painted editable field combines those actions: either the text
area or arrow toggles the popup, and the text area retains keyboard input.
Pointer motion through the popup moves one hot-row highlight without changing
the committed selection.

`list_box` is a descriptive alias of `list`, so existing list code and derived
classes remain source compatible:

```cpp
native::combo_box format(
    {"Plain text", "Markdown", "HTML"},
    native::combo_box_style::drop_down_list,
    20, 20, 180, 26);
native::list_box files({"README.md", "LICENSE"}, 20, 60, 240, 140);
```

The fallback `draw_control()` virtual paints the complete combo box. As with
the other controls, overriding it replaces that stage; the library does not
paint a second default pass afterward.

## Filesystem resources

`file_icon` turns a `std::filesystem::path` into an exact-size square PNG.
`from_path()` uses `std::filesystem::is_directory()` to select the entry type.
For a destination that does not exist yet, `for_file()` and
`for_directory()` state the intended type explicitly. The returned
`file_icon_source` distinguishes an operating-system image from the shared
attributed generic file and folder PNG fallbacks.

Linux resolves PNGs through the current Freedesktop icon theme and its
inherited, Adwaita, and hicolor fallbacks. Windows renders the Shell icon,
Haiku renders the Tracker icon, and macOS renders the `NSWorkspace` file icon.
Each adapter transfers only RGBA pixels into shared code; the public class
contains no native handle.

`special_directory::detect()` refreshes the process snapshot of conventional
locations. Its values use `std::filesystem::path` and cover home, desktop,
documents, downloads, music, pictures, videos, public and template folders,
applications, fonts, configuration, application data, cache, and temporary
storage when the platform supplies them. Linux reads the XDG base and user
directory configuration through standard streams, Windows uses Known Folders,
Haiku uses `find_directory`, and macOS uses Foundation search paths. Detection
does not create or require the returned directories. A second `detect()` may
invalidate pointers returned by `at()` or `find()`.

## Standard dialogs

`directory_dialog` extends the existing `file_dialog` state and modal-session
contract. It accepts an initial folder and optionally requests multiple
folders, then reports `accepted` or `cancelled` through `on_modal_close`.
`get_path()` returns the first accepted folder and `get_paths()` returns all
folders supplied by a capable platform chooser.

```cpp
native::directory_dialog folder(window, "Choose workspace");
folder.set_initial_path("/projects");
folder.on_modal_close.connect([&](native::dialog_result result) {
    if (result == native::dialog_result::accepted)
        open_workspace(folder.get_path());
    return true;
});
folder.create();
folder.show();
```

The existing `open_file_dialog` and `save_file_dialog` remain the standard
file-open and file-save APIs. All three chooser objects must outlive an active
native session because some platforms complete asynchronously.

SDL2 consistently uses one library-owned themed browser for file open, file
save, and folder selection. Its path handling, traversal, metadata, and
wildcard matching use the C++ standard library. The window has a Places
table populated from `special_directory`, icon-only back/forward/up
navigation, and a clickable, horizontally eliding breadcrumb. The address
editor occupies the same bounds: a double click toggles between breadcrumb
and direct path modes, while Ctrl+L also opens direct editing. Entering direct
mode places a normal caret without implicitly selecting the complete path, and
switching views preserves uncommitted path text. The main
Name/Type/Size `table_view` and Places rows use decoded
`file_icon` PNGs; file icon lookup is cached by type so large folders remain
responsive. Its classic scrollbar moves through one continuous directory
listing and supports arrows, trough steps, wheel input, and thumb dragging;
the browser never paginates the directory. A single click selects an entry, a
double click enters a folder or accepts a file, and Enter works in the focused
location, filename, or table. File modes keep location and filename in
separate fields; Ctrl+H toggles dot-prefixed hidden entries. The browser also
provides filtering, validation, default save extensions, and overwrite
confirmation. Completion releases capture and returns keyboard focus to the
owner.

`message_box::show()` is synchronous and owner-modal. The button sets are
`ok`, `ok_cancel`, `yes_no`, and `yes_no_cancel`; the result is one of `ok`,
`cancel`, `yes`, `no`, or `none`. Information, warning, error, and question
icons are semantic requests that the backend maps to its standard alert.
SDL2 uses a library-owned modal alert with the same stock control font, panel
surface, semantic icon, and `button` implementation as the rest of its UI. Its
information, warning, error, and question badges come from an attributed,
embedded PNG set, so they do not depend on SDL_ttf glyph coverage or
procedurally approximated marks. Window Maker places the same attributed
semantic badge in its native WINGs alert, preserves the alert's fonts, layout,
and buttons, and applies the requested title to its native frame. Its private
modal dispatcher continues owner expose handling and deferred Native
callbacks. SDL's synchronous wait
continues to process paint, pointer, keyboard, resize, and close events before
restoring the owner.

```cpp
const auto answer = native::message_box::show(
    window, "Save changes before closing?", "Editor",
    native::message_box_buttons::yes_no_cancel,
    native::message_box_icon::question);
```

## Non-client edge strips

`non_client` is the common base for window-owned elements that consume an
edge instead of participating in child layout. Visible elements reserve their
extent from `wnd::get_client_bounds()`. Layout managers receive that reduced,
host-relative rectangle. Hiding a strip or changing its extent immediately
relayouts children and invalidates the host.

Strips stack inside the host's chrome area rather than its raw bounds, so a
host that owns permanent edge furniture of its own keeps them inside it. On a
`canvas`, whose scrollbars take the outermost edges, a horizontal ruler
therefore stops before a visible vertical scrollbar instead of running under
it. See [Structural Panels and Paintable Canvases](PATTERNS-PANELS-AND-CANVASES.md).

The strips are ordinary C++ members owned by the application. They attach in
their constructor and detach in their destructor; the host never deletes
them. They paint through the graphics context supplied by the current window
paint event.

## Rulers

A `ruler` may occupy any edge. Top and bottom rulers are horizontal; left and
right rulers are vertical. `set_origin()` defines the value at the strip's
first pixel and `set_units_per_pixel()` defines the scale. Minor and major
tick intervals are expressed in those same units.

```cpp
native::ruler horizontal(window, native::window_edge::top, 24);
native::ruler vertical(window, native::window_edge::left, 30);

horizontal.set_origin(-20)
          .set_units_per_pixel(0.5)
          .set_minor_tick(5)
          .set_major_tick(25)
          .set_edge_visible(true)
          .set_track_mouse(true);
horizontal.on_tracking.connect([](double value) {
    update_coordinate_readout(value);
    return true;
});
```

Tracking observes host pointer motion and emits only when the tracked ruler
pixel changes. The protected virtual stages `draw_background()`,
`draw_tick()`, `draw_label()`, `draw_edge()`, and `draw_tracker()` perform the
actual default drawing. The optional edge is the bottom rule for horizontal
rulers and the right rule for vertical rulers, using the tick color. A derived
ruler can replace one stage and call its base implementation when it wants the
standard appearance plus an addition.

## Status bars

`status_bar` reserves the bottom edge. `set_text()` displays one flexible
part. `set_parts()` accepts ordered `status_bar_part` values; a positive width
is fixed and width zero shares the remaining width among all flexible parts.
The status bar spans the full window width and owns both lower corners; side
non-client strips stop above it.

```cpp
native::status_bar status(window, 22);
status.set_parts({{"Ready", 0}, {"Line 12, Column 4", 150}});
```

`draw_background()` and `draw_part()` are protected virtual stages and contain
the default themed painting. Status bars and rulers use semantic theme
surfaces and the active backend palette, rather than hard-coded visuals from
another desktop. Painted status parts use the gray panel/chrome role with
theme highlight and shadow edges, leaving white content surfaces for editors
and item views.

Windows hosts `STATUSCLASSNAME` for the exact base `status_bar`, maps portable
parts through `SB_SETPARTS` and `SB_SETTEXT`, and lets the common control own
the size grip and painting. The peer stays above its sibling controls and
enables sibling clipping, protecting both painting and hit-testing in the
reserved strip when the host becomes smaller than its controls.
Derived status bars stay on the painted path so
their protected-stage overrides continue to work.

The Haiku backend uses its native compact status-view recipe: a system
flat panel background, dark top rule, short internal dividers, panel text, and a platform
theme height that aligns with the titled-window resize marker. That height is
distinct from the smaller scrollbar extent. `BStatusBar` is Haiku's progress
indicator; it does not represent this text-only multipart window strip.

Vision exposes these APIs under **Window -> Input and window chrome**.
Its status strip uses `theme::get_status_bar_height()` after window creation.
GEMix reserves the stock font height plus six pixels, matching AES title/menu
chrome and keeping centred glyphs clear of the top separator.
Its GEM status parts draw top and leading divider lines only: the surrounding
window supplies the bottom/right enclosure.
For automated desktop smoke tests, `vision --input-chrome` opens the same
window immediately after the application enters its normal event loop.

Dense ruler scales retain every configured minor and major tick, while
labels are spaced using measured text width to avoid overlap on either
axis. Painting is clipped to the ruler strip and restores the caller’s
graphics state. Choose a strip extent that leaves room for the font above
the ticks (Maestro uses 32 pixels for one-pixel minor/eight-pixel major ticks).

## Compact property grids and edge toolbars

`property_grid` composes a structural panel, separate canvas regions for
labels, the value background, frame edges and the right scrollbar, and existing text/check/combo
controls. The value canvas is the editors’ native parent, so its painting and hit
testing cannot cover a sibling editor. Label, frame and scrollbar regions
do not overlap editor input. The value fields
and labels use the control-host background. Property rows have unique string IDs and
`property_value` values (`std::string`, `double`, `bool`, or custom `std::any`). A choice row
stores one of its declared strings. Invalid types, non-finite numbers,
duplicate IDs, and unknown choices are rejected before changing the model.
Programmatic setters are silent; `on_change` reports a committed user edit.
Intermediate numeric text such as a lone minus sign leaves the last committed
number intact. Read-only rows use read-only text. The automatic row height accommodates the control font plus six pixels and
the native editor/check height plus two pixels. Explicit heights cannot shrink
below that floor. A light dotted grid continues across labels and values.
Labels occupy half the viewport unless `set_label_width()` overrides it.
Only complete visible rows have editor resources. Scrolling preserves
committed values when those resources are recreated.
Win32 property choices retain their native combo popup and arrow; native
owner-draw notifications supply control paper behind the selected value.

A `toolbar` attaches like a ruler, at top, bottom, left, or right. Multiple
bars at the same edge stack in attachment order. Its canvas owns pointer
input and paints the main-menu background and arrow-free toolbar-button primitive.
The window creation/configuration path creates that surface even if an
application consumes the create signal. Painting does not create children.
On Windows, stock bars use the flat toolbar common control; on Haiku they
use BToolBar with native flat buttons and oriented separators. Haiku reserves
the native layout's minimum cross-axis size plus its surrounding border inset,
and updates that layout synchronously when icons or orientation change.
On macOS, exact stock bars create flexible-height native flat `NSButton` tools and
`NSBox` separators during configuration. AppKit owns pressed/checked feedback,
keyboard interaction, disabled appearance and template-image tinting. Native
cell measurements reserve enough width for complete toolbar labels and images. The
portable bar can appear on any edge and several bars can share an edge, so it
does not replace a window's single titlebar-owned `NSToolbar`. Derived bars
retain their painting hook. Stock rounded AppKit buttons keep native bezel
painting for every edge mask; changing border sides only changes clipping.
Momentary buttons emit commands; toggles flip; exclusive tools select one
member of their named group. Setters emit no commands. Disabled tools reject
activation, and releasing outside a pressed tool cancels it. Each bar owns
its tools and shares immutable icon images. `set_icon_size({24, 24})` changes
the image box and grows or shrinks the strip, retaining its padding; 16x16
and 32x32 also work. All neighboring strips move in the same configuration
pass. Horizontal bars span the window width and side bars stop between them.
The image and label are centered together; transparent pixels retain the
menu background. Black monochrome icons use the menu foreground when selected
or disabled, retaining their alpha mask so classic inverted menus remain legible.
Font measurement does not obtain a child-window graphics
context, since composited backends can share the top-level renderer.
Changing edge preserves items and selection. Overflowing tools are clipped;
use additional bars when more strip space is needed.

Mac rounded-button edge masks use the intrinsic native control height and
AppKit alignment rectangle rather than trimming the outer view margin.
The same clip applies to native painting and layer composition, so disabling
an edge removes the inset bezel while retaining AppKit's rounded drawing.

Controls and non-client strips expose `set_border_sides()`, defaulting to
`border_sides::all`. It applies to the outer frame, preserving inner separators
and indicators. A Windows status bar with a partial mask uses the shared
painted path; the default retains the native common control. Vision's
**Properties and toolbars** window (or `vision --properties`) demonstrates all
five property kinds, including a custom color canvas, individual border toggles, multiple edge bars, and all
three icon sizes.

Custom dropdown rows use `property_drop_down::to_text`, `create_content`, and
`content_size`. The factory returns an uncreated window, which the grid owns
and hosts in an independent titleless popup. Content can be a standard control
or a derived panel/canvas with its own children. Its commit callback publishes
a typed value after native event dispatch; Cancel and source-row destruction
discard unfinished edits. Programmatic updates refresh the converted text
silently. `std::any` payloads notify on every accepted commit because arbitrary
C++ types have no universal equality operation.

Native checkboxes retain their assigned compact width across label changes.
Row separators use exactly one filled pixel row. WINGs clips and centers its
native text line within a cell; Haiku resizes and centers the text rectangle
when the grid changes dimensions after creation.

Picker widgets use the standard input height; macOS keeps compact intrinsic
widths. Haiku dates also keep their native preferred width, including the
accessory and complete date segments, capped by the allocation. Both fields
match BTextControl height and use native font metrics for text centering.
The date field contains a flat calendar dropdown button
in place of arrows; time keeps equal-sized native arrows. BCalendarView supplies
month navigation and native disabled-day painting. The popup anchors to the
whole field's bottom-left and is at least that field's width. Its minimum
leaves seven readable day columns, so a narrow editor never squeezes the
calendar. Month buttons are compact native squares. The internal trigger
uses document paper and the same public native arrow painter as both combo
styles, avoiding a second idle menu bezel. Measured date separators remove
fixed spacer cells. The calendar arrow retains its native pressed highlight
until dismissal; a second arrow click closes it. Deactivation over the pressed
trigger lets that click close the existing popup rather than reopen it.
Messenger identities reject delayed close notifications from older popups. The Haiku navigator has
equal outer margins and native frames around both its search field and white
outline-list viewport.
Single-line BTextView controls center their actual text line after resizing.
Haiku file panels center after BFilePanel::Show has applied its native
placement. Docked status chrome retains its top rule and internal dividers,
omitting the outer left/right rules.
GraniteDatePicker displays the initial date and accepts typed locale dates
(or ISO dates) on Enter, through the same acceptance policy as its calendar.
GTK calendars retain native labels and apply insensitive states to unavailable
current-month days; rejected proposals still restore the cached value.
Optional search uses the full navigator width and includes a magnifier.
GTK's editable combo uses its native GtkComboBoxText rather than a separate
entry beside a full dropdown. Matching typed text selects the native list item.

Visible-owner macOS message boxes use native NSAlert sheets, preserving the
synchronous result while AppKit attaches and positions the alert on its owner.
A hidden owner uses AppKit application-modal presentation.

Vision supplies embedded Font Awesome Free navigation tiles on every backend.
The AppKit sidebar allocates 24-pixel images in 32-pixel page rows. Native
selection retains the original full-color image. Attribution and regeneration
are documented in `src/assets/navigation/README.md`.
