# Programming Native

This manual teaches application programming with the `native` C++ user
interface library. It starts with the smallest possible window and then
introduces painting, input events, menus, controls, layouts, independent
windows, system dialogs, graphics, fonts, images, clipboard access, text
editing, collection and tree controls, and virtual multi-column tables.
It also covers split views, tabs, panels, canvases, searchable settings,
date/time pickers, toolbars, property inspectors, and image-effect views.

Start with Chapters 1 and 4, then choose a control from the index below.
Each control has a small complete program and steps explaining construction,
creation, signals, and something to try. Supporting snippets extend the
complete programs; they are not separate translation units. Build and
platform details are collected in Chapter 12 rather than repeated in each
lesson. The repository itself builds one demonstration application, `vision`.

## Use an example

1. Copy one complete C++ block containing `int program(int, char **)` into
   your application's source. Do not combine several `program()` definitions.
2. Link the CMake target `native` in a consumer project, following
   [Chapter 12](programming-native/12-BUILDING-AND-DISTRIBUTING.md#native-as-a-cmake-dependency).
3. Build inside the selected Docker SDK; use the remote Apple SDK workflow
   for macOS. The commands below build Vision, not the copied tutorial.
4. Run on that backend's desktop. Follow the lesson's small interaction check.

The [AI entry point](../ai/README.md) routes generation tools to the same
examples, skills, and agent instructions. It is also useful as a compact
checklist for application authors.

## How a native program is organized

A native application has three important pieces:

1. Include the public API with `#include <native.h>`.
2. Define an application window, usually by deriving from
   `native::app_wnd`.
3. Implement `program()`, which creates the window and passes it to
   `native::app::run()`.

The library supplies the operating-system entry point and event loop. Your
code supplies `program()` and reacts to signals emitted by windows and
controls.

## Table of contents

1. [Your first application](programming-native/01-FIRST-APPLICATION.md)
2. [Painting and mouse input](programming-native/02-PAINTING-AND-INPUT.md)
3. [Menus and commands](programming-native/03-MENUS-AND-COMMANDS.md)
4. [Buttons and control lifecycle](programming-native/04-BUTTONS.md)
5. [Configuring controls](programming-native/05-CONFIGURING-CONTROLS.md)
6. [Absolute layout](programming-native/06-ABSOLUTE-LAYOUT.md)
7. [Grid and nested layout](programming-native/07-GRID-LAYOUT.md)
8. [Selection controls](programming-native/08-SELECTION-CONTROLS.md)
9. [Owned windows and file dialogs](programming-native/09-OWNED-WINDOWS-AND-DIALOGS.md)
10. [Graphics, images, fonts, and themes](programming-native/10-GRAPHICS-IMAGES-FONTS-THEMES.md)
11. [Clipboard and text editing](programming-native/11-CLIPBOARD-AND-TEXT-EDITING.md)
12. [Building, linking, and distributing](programming-native/12-BUILDING-AND-DISTRIBUTING.md)
13. [Collection and disclosure controls](programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md)
14. [Advanced table views](programming-native/14-ADVANCED-TABLE-VIEWS.md)
15. [Source editing](programming-native/15-SOURCE-EDITING.md)
16. [Split views and tabs](programming-native/16-SPLIT-VIEWS-AND-TABS.md)
17. [Filesystem resources, input, dialogs, and window chrome](programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md)
18. [Panels and canvases](programming-native/18-PANELS-AND-CANVASES.md)
19. [Background work and services](programming-native/19-BACKGROUND-WORK-AND-SERVICES.md)
20. [Portable programmable image shaders](programming-native/20-IMAGE-SHADERS.md)
21. [Settings, dates, and times](programming-native/21-SETTINGS-AND-PICKERS.md)
22. [Toolbars and property inspectors](programming-native/22-TOOLBARS-AND-PROPERTIES.md)

## Building the repository program

Configure the host control tree and choose a Docker-backed target:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake --target docker-sdl2
```

The application is produced as `build/linux-sdl2/src/vision`. Use the matching
F5 profile or the runtime instructions in
[Chapter 12](programming-native/12-BUILDING-AND-DISTRIBUTING.md) for dependencies,
linking, deployment, and the other backend targets. Motif/CDE, OPEN LOOK, and
Window Maker have dedicated nested desktop launchers; GTK uses Wayland.

## Find every control

The links point directly to a complete program or its construction steps.
Aliases and extension bases are identified so you do not search for widgets
that are not separate controls.

| Need | Public control or object | Step-by-step lesson |
| --- | --- | --- |
| Main window | `app_wnd` | [First application](programming-native/01-FIRST-APPLICATION.md) |
| Menu bar and commands | `main_menu`, `menu_entry` | [Menus](programming-native/03-MENUS-AND-COMMANDS.md) |
| Push button | `button` | [Buttons](programming-native/04-BUTTONS.md) |
| Boolean and exclusive choices | `check`, `radio` | [Selection](programming-native/08-SELECTION-CONTROLS.md#a-complete-selection-example) |
| Text list | `list`, alias `list_box` | [Selection](programming-native/08-SELECTION-CONTROLS.md#a-complete-selection-example), [choices](programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md#build-choices-and-standard-dialogs-step-by-step) |
| Single-line, multiline, read-only text | `text_edit` | [Text editing](programming-native/11-CLIPBOARD-AND-TEXT-EDITING.md#a-complete-editing-example), [read-only help](programming-native/21-SETTINGS-AND-PICKERS.md#step-3-run-the-window) |
| Fixed or editable dropdown | `combo_box` | [Choices](programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md#build-choices-and-standard-dialogs-step-by-step) |
| Image items and hierarchy | `icon_view`, `tree_view` | [Collection browser](programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md#build-a-collection-browser-step-by-step) |
| Collapsible sections | `accordion` | [Collection browser](programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md#build-a-collection-browser-step-by-step) |
| Multiple columns, groups, multiselection | `table_view`, `table_store` | [Small table](programming-native/14-ADVANCED-TABLE-VIEWS.md#build-a-small-table-step-by-step) |
| Source text and completion | `code_edit` | [Source editor](programming-native/15-SOURCE-EDITING.md#build-a-source-editor-step-by-step) |
| Resizable panes and pages | `split_view`, `tab_view` | [Tabbed workspace](programming-native/16-SPLIT-VIEWS-AND-TABS.md#build-a-tabbed-workspace-step-by-step) |
| Structural child host | `panel` | [Drawing panel](programming-native/18-PANELS-AND-CANVASES.md#build-a-drawing-panel-step-by-step) |
| Drawing with scrolling | `canvas` | [Drawing panel](programming-native/18-PANELS-AND-CANVASES.md#build-a-drawing-panel-step-by-step) |
| Settings pages and optional search | `settings_sidebar` | [Settings](programming-native/21-SETTINGS-AND-PICKERS.md) |
| Date and wall-clock fields | `date_picker`, `time_picker` | [Settings and availability](programming-native/21-SETTINGS-AND-PICKERS.md) |
| Edge commands and typed inspector | `toolbar`, `property_grid` | [Inspector](programming-native/22-TOOLBARS-AND-PROPERTIES.md) |
| Custom property popup | `property_drop_down` descriptor | [Custom dropdown](programming-native/22-TOOLBARS-AND-PROPERTIES.md#add-a-custom-property-dropdown) |
| Bottom status and coordinate scales | `status_bar`, `ruler` | [Drawing panel](programming-native/18-PANELS-AND-CANVASES.md#build-a-drawing-panel-step-by-step), [chrome settings](programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md#rulers-and-status-bar) |
| Auxiliary windows and custom dialogs | `modeless_wnd`, `modal_wnd` | [Owned windows](programming-native/09-OWNED-WINDOWS-AND-DIALOGS.md#build-owned-windows-step-by-step) |
| File, save, folder, and message panels | `open_file_dialog`, `save_file_dialog`, `directory_dialog`, `message_box` | [Standard dialogs](programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md#build-choices-and-standard-dialogs-step-by-step) |
| Image-effect display | `shader_view` | [Effect window](programming-native/20-IMAGE-SHADERS.md#build-an-image-effect-window-step-by-step) |
| Positioning | `absolute_layout_manager`, `grid_layout_manager` | [Absolute layout](programming-native/06-ABSOLUTE-LAYOUT.md), [grid layout](programming-native/07-GRID-LAYOUT.md) |

`wnd`, `owned_wnd`, `custom_control`, `collection_view`, `non_client`,
`file_dialog`, `layout_manager`, and `table_model` define common contracts or
extension points. Use the concrete controls above first. `scrollbar_policy`
configures scrollable controls; it is not a standalone scrollbar widget.
There is no separate label class: use a read-only `text_edit` for explanatory
text, with the border policy appropriate to the page.

Images, fonts, clipboard, screens, signals, input, audio, worker delivery, and
child processes support controls rather than adding more widgets. Their
lessons are Chapters 2, 10, 11, and 19; architecture details are in the
[Book of Native](BOOK-OF-NATIVE.md).

## Conventions used in the chapters

- Windows and controls are ordinary C++ objects.
- Parent and child relationships are non-owning; the application retains its
  control objects.
- Signals call connected handlers. Returning `true` stops further signal
  propagation.
- State changes that affect painting call `invalidate()` to request a repaint.
- Paint handlers draw only through the `gpx` object carried by the paint
  event.
- Portable strings are UTF-8. Text editors normalize line endings to `\n`.
- Backend headers and handles never enter application code. Include
  `<native.h>` and link the CMake target `native`.

Continue with [Your first application](programming-native/01-FIRST-APPLICATION.md).


Backend qualification and current review limits are described in
[Building and distributing](programming-native/12-BUILDING-AND-DISTRIBUTING.md)
and [ADJUST.md](../ADJUST.md). Amiga's runtime check is not a supported
application target. The repair record distinguishes passing portable examples,
native runtime regressions and SDK exceptions from exhaustive conformance.
