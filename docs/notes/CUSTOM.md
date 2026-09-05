# Native Versus Custom Control Audit

This document records how every public visual control is implemented in each
supported backend. It describes the code as it exists, not merely what the
underlying platform could provide. Its purpose is to make avoidable custom
implementations easy to find.

Audit date: 2026-09-05.

## Legend

| Mark | Meaning |
| --- | --- |
| **N** | A platform/toolkit control owns the control's presentation and main interaction. Portable code adapts its data and events. |
| **H** | Hybrid: native widgets provide the host, scrolling, or part of the interaction, while library code composes or paints a material part. |
| **C** | Library-painted or library-operated control; the platform supplies only a generic window/drawing surface. |
| **E** | External desktop helper, with a library fallback where documented. |

`list_box` is a compatibility alias of `list`, not a second implementation.
Models, layouts, graphics, fonts, clipboard, screen discovery, and signals are
services rather than controls and are outside this control inventory.
The inherited `wnd` mouse-cursor adapter, including its resize shapes and the
documented GEMix/macOS precision-pointer fallbacks, is likewise common window
behavior; adding its system cursor mapping does not change a control's **N**,
**H**, or **C** classification below.

## Applied native replacements

| Backend and control | Result | Portable behavior retained in C++ |
| --- | --- | --- |
| Windows `status_bar` | **N/H**, `STATUSCLASSNAME` with `SB_SETPARTS` and `SB_SETTEXT` | Text, part ordering, fixed/flexible widths, visibility, edge reservation, and fallback for an unsupported part count. The common control owns drawing and the size grip; derived bars retain the painted path so drawing overrides still work. |
| Athena `split_view` | **N**, Xaw `Paned` | Orientation, ratio, minimums, borrowed pane ownership, and ratio-change signal. `XawPanedSetMinMax` and pane constraints drive the native grips. |
| Motif `tree_view` | **N/H**, `XmContainer` outline in both presentation modes | Stable IDs, hierarchy data, icon ownership, and signals. Presentation mode now changes gadget relief instead of changing to a painted `XmDrawingArea`. |
| Motif `icon_view` | **N/H**, spatial `XmContainer` with `XmIconGadget` entries | Item model, stable selection index, images, label mode, disabled state, activation, and scroll offset. The collection is materialized, which matches `icon_view`'s owned-vector contract. |

## Reviewed proposals deliberately retained

| Backend and control | Decision |
| --- | --- |
| Haiku `status_bar` | Keep **C**. `BStatusBar` is a progress indicator, not a window footer. |
| Window Maker `ruler` | Keep **C**. `WMRuler` represents `WMText` margin and tab-stop state, not a generic measuring strip. |
| macOS `ruler` | Keep **C** for the public control. `NSRulerView` requires an `NSScrollView` client, while the portable ruler attaches to any `wnd`. A future text-editor-specific ruler can use it. |
| OPEN LOOK/XView `split_view` | Keep **H**. `OPENWIN_SPLIT` creates another view of one `CANVAS`/`TEXTSW`; it cannot host two arbitrary borrowed controls. |
| OPEN LOOK/XView `status_bar` | Keep **C**. Frame footers live outside and resize the content panel, while `non_client` reserves an in-host edge; combining both would reserve the strip twice. OLIT `FooterPanel` is not linked by this XView backend. |
| Motif `status_bar` | Keep **C**. `XmNmessageWindow` likewise participates in `XmMainWindow` geometry and only directly represents one message, not the portable in-host multi-part strip. |
| Window Maker `main_menu` | Keep **H**. The linked headers expose `WMMenuItem` through `WMPopUpButton`, but no public `WMCreateMenu`/show-at-point API with submenu support. The existing click-persistent popup preserves the menu contract. |
| Windows, macOS, and Haiku buttons/checks/radios | Keep **H**. Native controls own focus and interaction; owner/custom drawing is an explicit theme policy rather than a missed control. |
| Haiku `icon_view` | Keep **C**. Haiku's icon-grid implementation, `BPoseView`, is Tracker-private rather than a reusable public application control; a painted `BView` uses portable scrolling with `BControlLook` scrollbar parts and the system thumb preference. |
| SDL2 file open/save/directory | Keep **C**. SDL2 has no file-panel or desktop-widget API; one themed library browser can still provide consistent modal ownership, special-folder navigation, native-or-generic file icons, and standard-filesystem behavior without an external process. |
| X11/Athena `tab_view` | Keep **C**. Athena has no notebook or tab widget, and `Paned` only divides arbitrary children; painted tab chrome around borrowed page windows is the closest faithful implementation. |
| All accordions | Keep their current implementation. Paned/split widgets do not implement disclosure-stack semantics. |
| All `canvas` scrollbars | Keep **C**. Native scrollbars (`BScrollBar`, `XmScrollBar`, `WMScroller`, `NSScroller`, `SBS_*`) carry a narrower or differently-shaped range than the portable signed 32-bit content contract, which requires both endpoints to stay exactly reachable. The themed portable scrollbar also keeps the ruler/scrollbar and scrollbar/scrollbar corners inside one geometry pass. |
| All `canvas` hosts | Keep **H**. The backend supplies a real child drawing surface and its event routing; only the client, ruler, and scrollbar painting is portable, because the application owns the client pixels by contract. |

There is no stock general-purpose Win32 splitter, accordion, code editor, or
ruler equivalent to the portable contracts. SDL2 and GEM/AES likewise do not
provide a complete reusable child-widget set, so custom controls are expected
in those backends.

## Linux X11/Athena

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | Xt top-level/transient shells; portable ownership and modal-result state wrap Xt grabs and window-manager protocols. |
| `wnd` | **H** | Xaw `Form` child host with library paint/input routing. |
| `main_menu` | **N/H** | Xaw `MenuButton`, `SimpleMenu`, `SmeBSB`, and `SmeLine`; a composed Xaw menu bar maps mnemonics and shortcuts. |
| `button` | **N** | Xaw `Command`. |
| `check` | **N** | Xaw `Toggle`. |
| `radio` | **N** | Xaw `Toggle` with radio grouping. |
| `list` | **N** | Xaw `List`. |
| `combo_box` | **H** | Xaw `AsciiText` plus `MenuButton`/`SimpleMenu`; Athena has no single combo widget. |
| `text_edit` | **N/H** | Xaw `AsciiText`; portable validation and clipboard policy wrap the native editor. |
| `accordion` | **C** | Library-painted collection in an Xaw host. |
| `tab_view` | **C** | Library-painted tabs, framed/strip-only page chrome, and borrowed-page routing in an Xaw host. |
| `icon_view` | **C** | Library-painted wrapping grid in the shared collection host. |
| `tree_view` | **C** | Library-painted hierarchy in the shared collection host. |
| `table_view` | **C** | Model-backed library table painter in the shared collection host. |
| `code_edit` | **C** | Portable document/editor painted in an Xaw host. |
| `split_view` | **N/H** | Xaw `Paned`; portable ratio/minimum state maps to native pane constraints and `XawPanedSetMinMax`. |
| `panel` | **N** | Xaw `Form` child; Athena owns the container background and it is a real Xt parent for every control. |
| `canvas` | **H** | Xaw `Form` drawable host with library paint/input routing; the client, rulers, and themed scrollbars are painted by portable code. |
| `ruler` | **C** | Shared library-painted non-client strip. |
| `status_bar` | **C** | Shared library-painted non-client strip. |
| File open/save/directory | **E/H** | Zenity or KDialog when available; otherwise the library's Xaw file browser. |
| `message_box` | **N/H** | Xaw `Dialog` shell and Xaw buttons, composed to support the portable one-to-three-button contract. |

## Linux SDL2

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `SDL_Window`; portable ownership, modality filtering, positioning, first-frame presentation, focus-click-through with one-shot activation-release reconstruction, renderer-before-window teardown, and results. |
| `wnd` and all child-control hosts | **C** | SDL event routing and renderer-backed library windows; SDL2 supplies no desktop widget set. |
| `main_menu` | **C** | Library-painted menu bar and popup with SDL keyboard/pointer dispatch. |
| `button`, `check`, `radio` | **C** | Library-painted controls using SDL's neutral system-gray emulation palette; live and complete theme check/radio drawing share the same stages and geometry. |
| `list`, `combo_box`, `text_edit` | **C** | Library layout, painting, selection, editing, and conventionally stacked popup behavior; text selection uses the same active/inactive foreground and background palette as collections, and bounds changes reclamp retained text scrolling. Either combo field region opens the popup, its hot row follows pointer motion, it receives hits ahead of covered siblings, and it commits a row on the first press. Combos use a content-colored inset arrow button, and SDL text-input and clipboard services are used. |
| `accordion`, `tab_view` | **C** | Library-painted headers/tabs, including stable optional accordion frames, framed/strip-only tab pages, frame-aligned tab-strip starts, one-pixel selected-page overlap on every edge, and page routing. |
| `icon_view`, `tree_view`, `table_view` | **C** | Shared library collection painting, root-relative hit testing, and scrolling; collection scrollbars share classic arrow/trough/thumb painting and SDL thumb capture, tree frames are optional and default on, and SDL uses compact disclosure arrows without connector lines. Tables finish with complete rows and their complete outer frame. |
| `code_edit` | **C** | Portable document/editor and library painting. |
| `split_view` | **C** | Library pane geometry, registered root-relative divider hit testing, pointer-captured drag handling, and resize cursor. |
| `panel`, `canvas` | **C** | Nested regions of the emulated-control tree; painting, clipping through an SDL viewport, and root-relative hit testing are library-owned. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips; status parts use gray chrome surfaces and highlighted/shadowed edges. |
| File open/save/directory | **C** | One compact resizable themed browser for all three modes, with icon-backed Places and Name/Type/Size tables, icon-only history navigation, a breadcrumb/direct-address area toggled by double click, continuous draggable scrolling without pagination, separate filename entry, filters, validation, and save overwrite handling. |
| `message_box` | **C** | Library-owned themed `modal_wnd` with attributed embedded PNG semantic badges, stock control font, real Native buttons, and SDL event routing. |

## Linux OpenMotif

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | Xt shells with `XmMainWindow`; portable ownership/result state wraps Motif modality. |
| `wnd` | **H** | `XmDrawingArea` child host with library paint/input routing. |
| `main_menu` | **N** | `XmMenuBar`, `XmPulldownMenu`, `XmCascadeButton`, `XmPushButton`, and `XmSeparator`. |
| `button` | **N** | `XmPushButton`. |
| `check`, `radio` | **N** | `XmToggleButton`; portable code manages radio grouping. |
| `list` | **N** | `XmList` in `XmScrolledWindow`. |
| `combo_box` | **N** | `XmDropDownList` or editable `XmComboBox`. |
| `text_edit` | **N/H** | `XmTextField` or `XmText`; portable validation and clipboard policy wrap it. |
| `accordion` | **C** | Library-painted collection in `XmDrawingArea`. |
| `tab_view` | **N/H** | `XmNotebook`; side-tab labels are rotated, page hosts use Motif forms, and strip-only mode uses `XmSeparatorGadget`. |
| `icon_view` | **N/H** | Spatial `XmContainer` and `XmIconGadget` entries in `XmScrolledWindow`; portable code materializes the owned item vector and images. |
| `tree_view` | **N/H** | Both visual modes use `XmContainer` outline and `XmIconGadget`; the mode changes native gadget relief rather than selecting a custom painter. |
| `table_view` | **N/H** | Materialized mode uses `XmContainer` detail view; virtual models use the library-painted collection fallback. |
| `code_edit` | **C** | Portable editor painted in `XmDrawingArea`, with native scrollbars. |
| `split_view` | **N** | `XmPanedWindow`, including native sash behavior and pane minimum resources. |
| `panel` | **N** | `XmForm` child with `XmRESIZE_NONE`; Motif fills it and it is a real Xt parent. |
| `canvas` | **H** | Shared Motif `XmDrawingArea` collection host; the client, rulers, and themed scrollbars are painted by portable code. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips using Motif theme resources. |
| File open/save/directory | **N/H** | `XmFileSelectionDialog`; portable code adapts save confirmation and directory-only behavior. |
| `message_box` | **N** | Motif error, warning, question, or information dialogs. |

## Linux OPEN LOOK/XView

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | XView `Frame`/subframe and notifier; portable owner-busy and result handling. |
| `wnd` | **H** | XView panel/canvas host with OLGX-assisted library drawing. |
| `main_menu` | **N/H** | XView `Menu`/`Menu_item` attached to `PANEL_BUTTON` items in a composed menu panel. |
| `button` | **N** | `PANEL_BUTTON`. |
| `check` | **N** | `PANEL_CHECK_BOX`. |
| `radio` | **N** | `PANEL_CHOICE`. |
| `list` | **N** | `PANEL_LIST`. |
| `combo_box` | **H** | `PANEL_TEXT` plus `PANEL_CHOICE_STACK`; XView has no single combo widget matching both public modes. |
| `text_edit` | **N/H** | `PANEL_TEXT` or `PANEL_MULTILINE_TEXT`; portable validation and clipboard policy wrap it. |
| `accordion`, `tab_view` | **C/H** | Library-painted OLGX collection/tab chrome, including framed/strip-only pages, in XView panel hosts. |
| `icon_view`, `tree_view`, `table_view` | **C/H** | Library-painted collection content with native XView `SCROLLBAR` objects. |
| `code_edit` | **C/H** | Portable editor painted in an XView host with native XView scrollbars. |
| `split_view` | **H** | Two XView child `Panel` panes; the current implementation is not `OPENWIN_SPLIT` and has portable geometry. |
| `panel` | **N** | Borderless XView `PANEL` placed on the frame at the accumulated child offset; XView clears it and it accepts Panel items. |
| `canvas` | **H** | Shared XView collection Panel and paint window; the client, rulers, and themed scrollbars are painted by portable code. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips using the OPEN LOOK theme. |
| File open/save/directory | **N/H** | XView `FILE_CHOOSER`, adapted for directory-only and save behavior. |
| `message_box` | **N** | XView `NOTICE` with one to three buttons. |

## Linux Window Maker/WINGs

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `WMWindow`/`WMPanel`; portable ownership and input gating supplement WINGs. Owned windows destroy portable children before the recursive WINGs view tree and restore owner focus and repaint after either modal or modeless close. |
| `wnd` | **H** | WINGs view/frame host with library paint/input routing. |
| `main_menu` | **H** | WINGs `WMFrame` menu titles and separator plus a library-managed Xlib popup, mnemonics, accelerators, and hover tracking across top-level titles while a menu is open. |
| `button` | **N** | `WMCommandButton`. |
| `check` | **N** | `WMSwitchButton`. |
| `radio` | **N** | `WMRadioButton`. |
| `list` | **N/H** | `WMList`; supported item drawing aligns selection with the collection palette. |
| `combo_box` | **H** | `WMPopUpButton` and optional `WMTextField` inside a `WMFrame`; X Shape exposes only the editable field's inset arrow while the native popup retains the complete combo width, and state-aware overlay painting keeps the disclosure glyph centered through press and release. |
| `text_edit` | **N/H** | `WMTextField` or `WMText`; portable validation and clipboard policy wrap it, including reliable whole-value selection for direct copy/paste commands. |
| `accordion`, `icon_view`, `tree_view`, `table_view` | **C/H** | Library-painted collection content in a `WMFrame`, with native `WMScroller` controls. |
| `tab_view` | **N/H** | Native `WMTabView`/`WMTabViewItem` for framed top tabs; WINGs-matched directional painting and borrowed-page hosts for bottom, side, and strip-only tabs. Selected bottom/right tabs retain the same single joining line as top/left; inactive bottom/right tabs suppress their redundant inner closure and shadow their free edge. |
| `code_edit` | **C/H** | Portable editor painted in a WINGs host with native scrollers. |
| `split_view` | **N** | `WMSplitView` with WINGs subviews and divider behavior; native pane-size notifications synchronize the portable ratio and refit both borrowed children to the exact granted pane sizes, preserving every control edge. |
| `panel` | **N** | `WMFrame` with `WRFlat` relief; WINGs fills it and it is a real parent widget. |
| `canvas` | **H** | Shared WINGs collection frame; the client, rulers, and themed scrollbars are painted by portable code. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips using WINGs colors/fonts. |
| File open/save/directory | **N** | WINGs open/save file panels, including directory-selection mode; owner exposes remain live while their private modal loop moves a panel, and focus returns explicitly when it closes. |
| `message_box` | **N/H** | Native WINGs alert panel, controls, fonts, and modal loop, with the requested frame title and attributed embedded PNG semantic badge. |

## Linux GEMix/AES

The 2026-09-05 local-rasta audit retains the classifications below. AES owns
window stacking and menu tracking; child controls share coalesced damage in
their root peer. VDI clipping cannot be widened past the current AES visible
rectangle. Accordion surfaces precede their children, combo popups paint last,
and framed tab pages suppress the redundant enclosure of their list content.
Stock text on images uses the same GEM bitmap font as VDI window text.
Table sliders use the shared drag geometry with flat GEM-style frames,
stippled tracks, and arrow buttons. The custom radio primitive matches the
standard library GEM radio indicator.
The status height uses the same font-plus-six-pixel metric as AES title/menu
chrome; the gallery requests that theme metric rather than a font-only height.
Status parts retain their top rule and leading dividers, but omit bottom and
right borders already supplied by the surrounding window.
List selection explicitly invalidates its own region, including split panes.
Focused editors handle clipboard shortcuts before menu accelerators, and the
event loop requests both press and release edges. Flat left tabs retain their
page separator beside inactive tabs. AES selector buttons invert until release;
menu teardown occurs before the workstation closes. The sibling GEM runtime
restores complete title strips and publishes restored menu-popup pixels.

The same classifications apply to the verified `libgem -> gemd -> AES/VDI`
transport. It moves AES/VDI execution into the server without replacing any
control with a different implementation. Image scanline emission is bounded
by the visible repaint region before crossing the proxy connection.
The input follow-up retains these classifications: hosted USB navigation codes
and LF/CR Enter are translated in the GEMix adapter, and AES queued messages
preserve pointer state across the proxy. Closing an owned window repaints exposed
Native content inside the same update transaction as teardown.
AES desktop/frame redraws now share client occlusion clipping; opening windows
does not erase covered owner content or redraw hidden frames. This is an AES
implementation repair, with no change to control kinds or public interfaces.
The modal-frame follow-up keeps the **N/H** classification: AES owns an
untitled, gadget-free host and Native draws its four-pixel `1011` enclosure.
Both direct and proxy pixel tests check every edge and the client inset.

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | AES `wind_create` windows; library dispatcher supplies ownership/modality and the untitled modal `1011` frame. |
| `wnd` | **C/H** | AES work-area registration with VDI library painting and dispatch. |
| `main_menu` | **N/H** | The library builds an AES `OBJECT` tree and installs it with `menu_bar`; AES owns normal menu interaction while portable code maps commands and shortcuts. |
| `button`, `check`, `radio` | **C** | GEM-themed library painting and AES event handling. |
| `list`, `combo_box`, `text_edit` | **C** | Library painting, editing, popup/list, scrolling, and selection over AES/VDI. |
| `accordion`, `tab_view` | **C** | Library-painted headers/tabs, including framed/strip-only pages, and page routing. |
| `icon_view`, `tree_view`, `table_view` | **C** | Library-painted AES/VDI collection controls and virtual scrolling. |
| `code_edit` | **C** | Portable document/editor painted through VDI. |
| `split_view` | **C** | Portable pane geometry and splitter dispatch. |
| `panel`, `canvas` | **C** | Nested regions of the emulated-control tree; painting through an offset context and root-relative hit testing are library-owned. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips through VDI. |
| File open/save/directory | **N/H** | AES `fsel_input`; portable code adapts mode, path, and overwrite confirmation. |
| `message_box` | **N** | AES `form_alert`. |

## Microsoft Windows

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | Win32 top-level `HWND`; portable ownership/result state wraps owner enablement and modal dispatch. |
| `wnd` | **H** | Child `HWND` registered by the library for portable paint/input routing. |
| `main_menu` | **N** | `HMENU`, menu separators, mnemonic labels, and `HACCEL` accelerators. |
| `button`, `check`, `radio` | **H** | Win32 `BUTTON` windows with `BS_OWNERDRAW`; Win32 owns focus/messages while the theme paints them. |
| `list` | **N** | Win32 `LISTBOX`. |
| `combo_box` | **N** | Win32 `COMBOBOX`. |
| `text_edit` | **N/H** | Win32 `EDIT`; portable validation and clipboard policy subclass it. |
| `accordion` | **C** | Library child-window class and theme painting. |
| `tab_view` | **N/H** | Common-controls `WC_TABCONTROL`; portable borrowed-page routing and a post-paint separator for strip-only pages. |
| `icon_view` | **N/H** | Common-controls `WC_LISTVIEW` in icon mode with portable images/model mapping. |
| `tree_view` | **N/H** | Common-controls `WC_TREEVIEW` with custom draw for portable theme/details. |
| `table_view` | **N/H** | Report `WC_LISTVIEW`, owner-data virtualization, groups, and custom draw. |
| `code_edit` | **C** | Library child-window class and portable document/editor painter. |
| `split_view` | **C** | Library child-window class; Win32 has no stock splitter control. |
| `panel` | **N** | Child window of a Native class whose background brush is `COLOR_BTNFACE`; Win32 fills it and it is a real parent HWND. |
| `canvas` | **H** | Child window of the shared Native class; `WM_PAINT` routes to the portable paint path, which draws the client, rulers, and themed scrollbars. |
| `ruler` | **C** | Shared library-painted non-client strip; Win32 has no stock ruler peer. |
| `status_bar` | **N/H** | Common-controls `STATUSCLASSNAME`; portable parts map to `SB_SETPARTS`/`SB_SETTEXT`, with the library retaining edge reservation and model state. |
| File open/save/directory | **N** | Common Item Dialog (`IFileOpenDialog`/`IFileSaveDialog`, with folder-pick mode). |
| `message_box` | **N** | Win32 `MessageBoxW`. |

## Haiku

Rechecked 2026-09-05 in the Haiku VM: selection/resize repainting, collection
disclosure, table headings/groups/grid gutters, native four-edge tabs, and
the composed combo controls.
Basic native button/check/radio/list hosts remain hidden until their peer
registration is complete, preventing early custom-draw callbacks.

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `BWindow`; portable owner graph and result contract supplement native subset/modal behavior. |
| `wnd` | **H** | `BView` child host with portable paint/input routing. |
| `main_menu` | **N** | `BMenuBar`, `BMenu`, `BMenuItem`, and native separators/shortcuts. |
| `button` | **H** | `BButton` subclass with a library theme-drawing override. |
| `check` | **H** | `BCheckBox` subclass with Haiku-only drawing stages; indicator and label use the same `BControlLook` primitives as custom themed checks. No SDL2/default checkbox painter is linked into Haiku. |
| `radio` | **H** | `BRadioButton` subclass with a library theme-drawing override. |
| `list` | **N/H** | `BListView`/`BStringItem`; selection invalidates one shared paint path, and resizing repaints the entire list to erase old borders. |
| `combo_box` | **H** | Native buttons and `BTextControl` with `BControlLook` frames; editable arrow is a child inside the shared text border. Both native `BPopUpMenu` lists match the field width and open below it. Child visibility uses local hide counts, independent of hidden ancestors. |
| `text_edit` | **N/H** | `BTextView` and optional `BScrollView`; subclass supplies portable validation. |
| `accordion` | **C** | Library-painted collection `BView`, with background clearing on every invalid region; native-themed headers omit the generic blue focus frame. |
| `tab_view` | **N/H** | Native `BTabView`/`BTab` on all four sides, including native rotated text and label-sized side tabs; `B_NO_BORDER` plus one separator in strip-only mode. |
| `icon_view` | **C** | Library-painted collection `BView`; portable scrollbar geometry uses native `BControlLook` parts and system thumb style. |
| `tree_view` | **N/H** | `BOutlineListView`, `BStringItem`, and native `BScrollView`; custom item drawing adds portable icons and always repaints native latches. |
| `table_view` | **N/H** | `BColumnListView`, `BTitledColumn`, and `BRow`; group fields span all columns, grid lines extend through the latch gutter, and last-column fitting excludes native chrome. Virtual mode uses matching native headers and actual `BScrollBar` children. Both modes honor row-background settings and retain the same native row pitch. |
| `code_edit` | **C** | Portable document/editor painted in a `BView`. |
| `split_view` | **N** | `BSplitView`. |
| `panel` | **N** | Child `BView` with the panel background view color and no `B_WILL_DRAW`; the app_server fills it and it is a real parent view. |
| `canvas` | **H** | Shared collection host `BView`; the client, rulers, and themed scrollbars are painted by portable code. Native `BScrollBar` is not used because the portable scroll range is signed 32-bit content, not view pixels. |
| `ruler` | **C** | Shared library-painted non-client strip. |
| `status_bar` | **C** | Shared library-painted non-client strip. `BStatusBar` is intentionally not used because it is a progress indicator rather than a footer. |
| File open/save/directory | **N** | `BFilePanel`, configured for files, save, or directories. |
| `message_box` | **N** | `BAlert`. |

## Apple macOS

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `NSWindow`; portable ownership/result state wraps sheets and modal sessions. |
| `wnd` | **H** | `NSView` child host with portable paint/input routing. |
| `main_menu` | **N** | `NSMenu`/`NSMenuItem`, separators, mnemonics, and key equivalents. |
| `button`, `check`, `radio` | **H** | `NSButton` subclasses retain native control behavior but override drawing through the portable theme. |
| `list` | **N/H** | One-column `NSTableView` in `NSScrollView`, adapted to the portable list model. |
| `combo_box` | **N** | `NSComboBox`. |
| `text_edit` | **N/H** | `NSTextField` or `NSTextView` in `NSScrollView`; delegates enforce portable validation. |
| `accordion` | **N/H** | `NSStackView` with disclosure-style `NSButton` headers and portable page routing. |
| `tab_view` | **N/H** | `NSTabView`/`NSTabViewItem` with portable borrowed-page hosts; strip-only mode disables the page background and draws one separator. |
| `icon_view` | **N/H** | `NSCollectionView` in `NSScrollView`, adapted to the portable model. |
| `tree_view` | **N/H** | `NSOutlineView` in `NSScrollView`, with portable data source and images. |
| `table_view` | **N/H** | Data-source-driven `NSTableView` in `NSScrollView`, retaining native headers, reuse, grids, and stripes. |
| `code_edit` | **C** | Portable document/editor painted in a custom `NSView`. |
| `split_view` | **N** | `NSSplitView`. |
| `panel` | **N** | Child `NSView` filling `windowBackgroundColor`; a real AppKit parent for every control. |
| `canvas` | **H** | Child `NSView` whose `drawRect:` routes to the portable paint path; the client, rulers, and themed scrollbars are painted by portable code. |
| `ruler` | **C** | Shared library-painted non-client strip; `NSRulerView` is not currently used. |
| `status_bar` | **C** | Shared library-painted non-client strip. AppKit has no direct window-status-bar peer to the portable control. |
| File open/save/directory | **N** | `NSOpenPanel`/`NSSavePanel`, including directory-selection mode. |
| `message_box` | **N** | `NSAlert`. |

## Deliberately portable state

Even an **N** control retains portable C++ state for consistent API behavior:
items and stable IDs, callbacks/signals, validation, model access, owned-window
lifetime, and dialog results. That shared state does not make a control custom;
the classification is based on who owns visible presentation and primary user
interaction.

Conversely, placing a library painter inside a native generic view is **C**, not
**N**. Using a real native scrollbar or container around custom content is
**H**. This distinction is intentional so future reviews do not mistake a
native handle for a native control implementation.
