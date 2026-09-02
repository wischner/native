# Native Versus Custom Control Audit

This document records how every public visual control is implemented in each
supported backend. It describes the code as it exists, not merely what the
underlying platform could provide. Its purpose is to make avoidable custom
implementations easy to find.

Audit date: 2026-09-02.

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

## Native-replacement review

These are the current custom or hybrid implementations most worth reviewing:

| Priority | Backend and control | Current implementation | Native facility to assess |
| --- | --- | --- | --- |
| High | Haiku `status_bar` | **C**, shared painted non-client strip | `BStatusBar`. The current source does not instantiate it. |
| High | Windows `status_bar` | **C**, shared painted non-client strip | The common-controls status bar (`STATUSCLASSNAME`/`CreateStatusWindow`). |
| High | Window Maker `ruler` | **C**, shared painted non-client strip | WINGs `WMRuler`. |
| High | macOS `ruler` | **C**, shared painted non-client strip | AppKit `NSRulerView`. Its client-view association needs adapting to the portable non-client contract. |
| High | Athena `split_view` | **C**, two child hosts and library splitter handling | Xaw `Paned` where its orientation and child-size rules satisfy the API. |
| Medium | Motif `icon_view` | **C/H**, custom collection painter with native scrollbars | `XmContainer` spatial/large-icon layout, if its model and virtualization limits are acceptable. |
| Medium | OPEN LOOK/XView `split_view` | **H**, composed XView `Panel` panes | XView `OPENWIN_SPLIT` is native for splitting one text/canvas view, but is not a general two-arbitrary-child container. OLIT `RubberTile` would apply only to a separate OLIT backend; this backend links XView, not OLIT. |
| Medium | OPEN LOOK/XView `status_bar` | **C**, shared painted strip | XView frame left/right footers can replace simple footer cases. OLIT `FooterPanel` is unavailable in this XView build. |
| Medium | Window Maker `main_menu` | **H**, WINGs frames plus a custom click-persistent Xlib popup | Recheck current WINGs menu-capable widgets. The present implementation deliberately avoids the press-drag selector behavior. |
| Medium | Motif `tree_view` flat/native-look mode | **C**, `XmDrawingArea` collection host | `XmContainer` already backs the alternate 3-D outline mode and may be usable for both appearances. |
| Low | Windows, macOS, and Haiku `button`, `check`, `radio` | **H**, native controls use owner/custom drawing | Pure native drawing is available, but would give up the library's explicit cross-theme appearance controls. |
| Low | Motif `status_bar` | **C**, shared painted strip | An `XmLabel` in the `XmMainWindow` message-window region could cover a simple single-part status line; it does not directly match the full portable multi-part contract. |

There is no stock general-purpose Win32 splitter, accordion, code editor, or
ruler equivalent to the portable contracts. Custom implementations there are
not automatically architectural mistakes. SDL2 and GEM/AES likewise do not
provide a complete modern widget set, so custom controls are expected in those
backends.

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
| `tab_view` | **C** | Library-painted tabs and borrowed-page routing in an Xaw host. |
| `icon_view` | **C** | Library-painted wrapping grid in the shared collection host. |
| `tree_view` | **C** | Library-painted hierarchy in the shared collection host. |
| `table_view` | **C** | Model-backed library table painter in the shared collection host. |
| `code_edit` | **C** | Portable document/editor painted in an Xaw host. |
| `split_view` | **C** | Library-managed pane geometry and splitter in an Xaw `Form` host. |
| `ruler` | **C** | Shared library-painted non-client strip. |
| `status_bar` | **C** | Shared library-painted non-client strip. |
| File open/save/directory | **E/H** | Zenity or KDialog when available; otherwise the library's Xaw file browser. |
| `message_box` | **N/H** | Xaw `Dialog` shell and Xaw buttons, composed to support the portable one-to-three-button contract. |

## Linux SDL2

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `SDL_Window`; portable ownership, modality filtering, positioning, and results. |
| `wnd` and all child-control hosts | **C** | SDL event routing and renderer-backed library windows; SDL2 supplies no desktop widget set. |
| `main_menu` | **C** | Library-painted menu bar and popup with SDL keyboard/pointer dispatch. |
| `button`, `check`, `radio` | **C** | Library-painted themed controls. |
| `list`, `combo_box`, `text_edit` | **C** | Library layout, painting, selection, editing, and popup behavior; SDL text-input and clipboard services are used. |
| `accordion`, `tab_view` | **C** | Library-painted headers/tabs and page routing. |
| `icon_view`, `tree_view`, `table_view` | **C** | Shared library collection painting, hit testing, and scrolling. |
| `code_edit` | **C** | Portable document/editor and library painting. |
| `split_view` | **C** | Library pane geometry, hit testing, and drag handling. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips. |
| File open/save/directory | **E** | Zenity or KDialog desktop helper; returns cancel/unavailable when neither exists. |
| `message_box` | **N** | `SDL_ShowMessageBox`. |

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
| `tab_view` | **N/H** | `XmNotebook`; portable page-host plumbing uses Motif forms/buttons. |
| `icon_view` | **C/H** | Library-painted grid in `XmDrawingArea` with native `XmScrollBar`. |
| `tree_view` | **N/H** | 3-D mode uses `XmContainer` outline; flat native-look mode uses the custom collection host. |
| `table_view` | **N/H** | Materialized mode uses `XmContainer` detail view; virtual models use the library-painted collection fallback. |
| `code_edit` | **C** | Portable editor painted in `XmDrawingArea`, with native scrollbars. |
| `split_view` | **N** | `XmPanedWindow`, including native sash behavior and pane minimum resources. |
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
| `accordion`, `tab_view` | **C/H** | Library-painted OLGX collection/tab chrome in XView panel hosts. |
| `icon_view`, `tree_view`, `table_view` | **C/H** | Library-painted collection content with native XView `SCROLLBAR` objects. |
| `code_edit` | **C/H** | Portable editor painted in an XView host with native XView scrollbars. |
| `split_view` | **H** | Two XView child `Panel` panes; the current implementation is not `OPENWIN_SPLIT` and has portable geometry. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips using the OPEN LOOK theme. |
| File open/save/directory | **N/H** | XView `FILE_CHOOSER`, adapted for directory-only and save behavior. |
| `message_box` | **N** | XView `NOTICE` with one to three buttons. |

## Linux Window Maker/WINGs

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `WMWindow`/`WMPanel`; portable ownership and input gating supplement WINGs. |
| `wnd` | **H** | WINGs view/frame host with library paint/input routing. |
| `main_menu` | **H** | WINGs `WMFrame` menu titles and separator plus a library-managed Xlib popup, mnemonics, and accelerators. |
| `button` | **N** | `WMCommandButton`. |
| `check` | **N** | `WMSwitchButton`. |
| `radio` | **N** | `WMRadioButton`. |
| `list` | **N/H** | `WMList`; supported item drawing aligns selection with the collection palette. |
| `combo_box` | **H** | `WMPopUpButton` and optional `WMTextField` inside a `WMFrame`. |
| `text_edit` | **N/H** | `WMTextField` or `WMText`; portable validation and clipboard policy wrap it. |
| `accordion`, `icon_view`, `tree_view`, `table_view` | **C/H** | Library-painted collection content in a `WMFrame`, with native `WMScroller` controls. |
| `tab_view` | **N/H** | `WMTabView` and `WMTabViewItem`; portable borrowed-page hosts. |
| `code_edit` | **C/H** | Portable editor painted in a WINGs host with native scrollers. |
| `split_view` | **N** | `WMSplitView` with WINGs subviews and divider behavior. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips using WINGs colors/fonts. |
| File open/save/directory | **N** | WINGs open/save file panels, including directory-selection mode. |
| `message_box` | **N** | `WMRunAlertPanel`. |

## Linux GEMix/AES

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | AES `wind_create` windows; library event dispatcher supplies ownership and modality. |
| `wnd` | **C/H** | AES work-area registration with VDI library painting and dispatch. |
| `main_menu` | **N/H** | The library builds an AES `OBJECT` tree and installs it with `menu_bar`; AES owns normal menu interaction while portable code maps commands and shortcuts. |
| `button`, `check`, `radio` | **C** | GEM-themed library painting and AES event handling. |
| `list`, `combo_box`, `text_edit` | **C** | Library painting, editing, popup/list, scrolling, and selection over AES/VDI. |
| `accordion`, `tab_view` | **C** | Library-painted headers/tabs and page routing. |
| `icon_view`, `tree_view`, `table_view` | **C** | Library-painted AES/VDI collection controls and virtual scrolling. |
| `code_edit` | **C** | Portable document/editor painted through VDI. |
| `split_view` | **C** | Portable pane geometry and splitter dispatch. |
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
| `tab_view` | **N/H** | Common-controls `WC_TABCONTROL`; portable borrowed-page host routing. |
| `icon_view` | **N/H** | Common-controls `WC_LISTVIEW` in icon mode with portable images/model mapping. |
| `tree_view` | **N/H** | Common-controls `WC_TREEVIEW` with custom draw for portable theme/details. |
| `table_view` | **N/H** | Report `WC_LISTVIEW`, owner-data virtualization, groups, and custom draw. |
| `code_edit` | **C** | Library child-window class and portable document/editor painter. |
| `split_view` | **C** | Library child-window class; Win32 has no stock splitter control. |
| `ruler`, `status_bar` | **C** | Shared library-painted non-client strips; the Win32 status-bar common control is not used. |
| File open/save/directory | **N** | Common Item Dialog (`IFileOpenDialog`/`IFileSaveDialog`, with folder-pick mode). |
| `message_box` | **N** | Win32 `MessageBoxW`. |

## Haiku

| Public control | Kind | Current implementation |
| --- | --- | --- |
| `app_wnd`, `modeless_wnd`, `modal_wnd` | **N/H** | `BWindow`; portable owner graph and result contract supplement native subset/modal behavior. |
| `wnd` | **H** | `BView` child host with portable paint/input routing. |
| `main_menu` | **N** | `BMenuBar`, `BMenu`, `BMenuItem`, and native separators/shortcuts. |
| `button` | **H** | `BButton` subclass with a library theme-drawing override. |
| `check` | **H** | `BCheckBox` subclass with a library theme-drawing override. |
| `radio` | **H** | `BRadioButton` subclass with a library theme-drawing override. |
| `list` | **N/H** | `BListView`/`BStringItem`; a subclass adjusts background and selection presentation. |
| `combo_box` | **N/H** | Selection-only uses `BOptionPopUp`; editable mode composes `BTextControl` and `BOptionPopUp`. |
| `text_edit` | **N/H** | `BTextView` and optional `BScrollView`; subclass supplies portable validation. |
| `accordion` | **C** | Library-painted collection `BView`. |
| `tab_view` | **N/H** | `BTabView` and `BTab`; portable borrowed-page hosts. |
| `icon_view` | **C** | Library-painted collection `BView` with native scrollbar objects in its host. |
| `tree_view` | **N/H** | `BOutlineListView`, `BStringItem`, and `BScrollView`; custom item drawing adds portable icons. |
| `table_view` | **N/H** | `BColumnListView`, `BTitledColumn`, and `BRow`; virtual mode retains a custom collection fallback. |
| `code_edit` | **C** | Portable document/editor painted in a `BView`. |
| `split_view` | **N** | `BSplitView`. |
| `ruler` | **C** | Shared library-painted non-client strip. |
| `status_bar` | **C** | Shared library-painted non-client strip; `BStatusBar` is not currently used. |
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
| `tab_view` | **N/H** | `NSTabView`/`NSTabViewItem` with portable borrowed-page hosts. |
| `icon_view` | **N/H** | `NSCollectionView` in `NSScrollView`, adapted to the portable model. |
| `tree_view` | **N/H** | `NSOutlineView` in `NSScrollView`, with portable data source and images. |
| `table_view` | **N/H** | Data-source-driven `NSTableView` in `NSScrollView`, retaining native headers, reuse, grids, and stripes. |
| `code_edit` | **C** | Portable document/editor painted in a custom `NSView`. |
| `split_view` | **N** | `NSSplitView`. |
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
