---
name: native-controls
description: Select and compose Native UI controls for C++ forms, navigation, dates and times, tables, source editors, and document workspaces. Use when implementing application UI with the existing public API.
---

# Compose controls through the public API

Use the [control index](../../manuals/PROGRAMMING-NATIVE.md#find-every-control)
as the routing table. Resolve these links in the Native checkout that matches
the consumer's dependency, and confirm methods in `include/native/` before
writing code. Read only the chapters needed for the requested interface.

| Interface task | Read |
| --- | --- |
| Inputs, selection, read-only help | Chapters [8](../../manuals/programming-native/08-SELECTION-CONTROLS.md), [11](../../manuals/programming-native/11-CLIPBOARD-AND-TEXT-EDITING.md), [17](../../manuals/programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md) |
| Trees, images, collapsible sections | [13](../../manuals/programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md) |
| Multiple columns, multiselection, virtual rows | [14](../../manuals/programming-native/14-ADVANCED-TABLE-VIEWS.md) |
| Source text and completion | [15](../../manuals/programming-native/15-SOURCE-EDITING.md) |
| Panes, pages, structural hosts, drawing | [16](../../manuals/programming-native/16-SPLIT-VIEWS-AND-TABS.md), [18](../../manuals/programming-native/18-PANELS-AND-CANVASES.md) |
| Searchable settings, holiday rules, time steps | [21](../../manuals/programming-native/21-SETTINGS-AND-PICKERS.md) |
| Toolbars, status, typed/custom properties | [22](../../manuals/programming-native/22-TOOLBARS-AND-PROPERTIES.md) |

Choose the simplest control whose semantics match. `list`/`list_box` is
single-selection text; `table_view` supports multiple selection and models;
`tree_view` represents hierarchy. `text_edit` handles ordinary text;
`code_edit` handles source overlays/completion. A panel hosts children; a
canvas owns application painting. There is no standalone label or scrollbar
class: use read-only text and the scrollable control's policies.

Keep these composition rules visible in generated code:

- Tabs, splits, accordion bodies, and settings pages are borrowed and initially
  uncreated. Declare them before the container and connect page create
  handlers for recreatable child resources. Tables borrow their model and
  cell images; tree/icon items retain shared images.
- Use stable page/tree/table/property/tool IDs for application identity;
  simple list, combo, icon, and tab selections use indices. Do not convert
  IDs into row positions or assume all selection signals share a type.
- Setters are normally silent. Connect the declared user-action signal;
  don't invoke `on_native_*` to simulate a normal property assignment.
  Source-edit commands have their documented edit/undo notifications.
- Configure availability on `date_picker` and `time_picker`, including both
  typing and popup input. Rule changes preserve the current value; check
  `get_valid()`. Holidays are supplied by the application, not inferred from
  locale. A time minute step divides 60; civil values have no time zone.
- Toolbars, rulers, and status bars attach as non-client strips and reserve
  space. A status bar stays at the bottom. Use theme input height and measured
  content rather than compensating for one backend with application offsets.
- Paint only in the paint event using its borrowed `gpx`; retain data rather
  than a drawing context. Use theme colors for native controls. Use shared
  images for optional icons without introducing installed-font dependencies.

Verify interactions on the requested backends. The
[native/custom audit](../../notes/CUSTOM.md) records adapter choices; the
[feature matrix](../../manuals/book-of-native/FEATURE-MATRIX.md) records actual
evidence. A request to use a control does not authorize replacing Native's
backend or adding a toolkit dependency in application code.
