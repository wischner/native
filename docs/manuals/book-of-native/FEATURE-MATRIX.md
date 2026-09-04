# Feature Matrix

This chapter records what is implemented now and what is currently exercised in
runtime checks. For the contracts and examples behind the drawing rows, see
[Drawing Primitives](DRAWING-PRIMITIVES.md).

Legend:

- `Yes (tested)` = implemented and exercised in current workflow
- `Yes (build tested)` = implemented and compiled in the current workflow,
  without a runtime assertion for that behavior
- `Yes (untested)` = implemented, not yet exercised in current workflow
- `No (not run)` = not exercised by the current runtime workflow
- `WIP` = still under development

## Backend status

| Backend | Build path | Runtime status |
| --- | --- | --- |
| Linux X11 | `docker-x11` | Yes (tested) |
| Linux SDL2 | `docker-sdl2` | Yes (tested) |
| Linux OpenMotif | `docker-openmotif` | Yes (tested) |
| Linux OPEN LOOK/XView | `docker-openlook` + Tribblix VM native build | Yes (tested) |
| Linux Window Maker/WINGs | `docker-wmaker` + Bookworm VM native build | Yes (tested) |
| Linux GEMix | `docker-gemix` | Yes (untested) |
| MS Windows (MinGW) | `docker-win` + Wine | Yes (tested) |
| Haiku | `docker-haiku` + SSH deploy/run | Yes (tested) |
| Apple | remote build and runtime checks | Yes (tested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Build through the current platform workflow | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | WIP |
| `app::run` startup path | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Screen detection | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Exact-size file/directory PNG icons | Freedesktop + fallback (build tested) | Freedesktop + fallback (tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Shell + fallback (tested) | Tracker + fallback (build tested) | AppKit + fallback (tested) | WIP |
| Typed special-directory detection | XDG/C++ (build tested) | XDG/C++ (tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | Known Folders (tested) | `find_directory` (build tested) | Foundation (tested) | WIP |
| Main window create/show | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Central non-const lifecycle and visible-state contract | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (untested) | WIP |
| Independent owned `modeless_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Owner-blocking, focus-taking `modal_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Nested modal stack and `dialog_result` | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Standard `open_file_dialog` | Desktop/Xaw (tested) | Themed C++ browser (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (build tested) | Common Item Dialog (tested) | BFilePanel (build tested) | NSOpenPanel (untested) | WIP |
| Standard `save_file_dialog` | Desktop/Xaw (tested) | Themed C++ browser (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (build tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSSavePanel (untested) | WIP |
| Standard `directory_dialog` | Desktop/Xaw (build tested) | Themed C++ browser (tested) | Motif (build tested) | XView (build tested) | WINGs (build tested) | AES (build tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSOpenPanel (untested) | WIP |
| Standard one-to-three-button `message_box` | Athena (build tested) | Themed modal with embedded PNG badges (tested) | Motif (build tested) | XView (build tested) | WINGs alert with embedded PNG badge and title (tested) | AES (build tested) | Win32 (build tested) | BAlert (build tested) | NSAlert (untested) | WIP |
| Paint event (`on_wnd_paint`) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse move | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse button press/release | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse wheel | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Arrow, I-beam, crosshair, and four resize mouse cursors | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Resize crosshair fallback (build tested) | Yes (build tested) | Yes (build tested) | Diagonal crosshair fallback (build tested) | WIP |
| Native/emulated `check` control | Yes (build tested) | Yes (tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Native/emulated sibling-exclusive `radio` control | Yes (build tested) | Yes (tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Native/emulated single-selection `list` control | Yes (build tested) | Yes (build tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| `list_box` compatibility name | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Selection-only/editable `combo_box` | Athena (build tested) | Themed (tested) | XmComboBox (build tested) | XView composite (build tested) | WINGs centered inset-arrow composite (tested) | Themed (build tested) | COMBOBOX (build tested) | Native-control composite, below-field popup (tested) | NSComboBox (untested) | WIP |
| Non-client rulers and status bars | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Structural `panel` container | Xaw `Form` child (build tested) | Emulated nested region (tested) | `XmForm` child (build tested) | XView Panel (build tested) | WINGs flat frame (build tested) | GEM themed region (build tested) | Child HWND, control-host brush (build tested) | Child `BView` container (build tested) | Child `NSView` container (build tested) | WIP |
| Paintable `canvas` surface | Athena drawable host (build tested) | Emulated nested region (tested) | Motif `XmDrawingArea` (build tested) | XView Panel with paint window (build tested) | WINGs flat frame (build tested) | GEM offset region (build tested) | Child HWND (build tested) | Child `BView` (build tested) | Child `NSView` (build tested) | WIP |
| Canvas 32-bit content bounds, clamping, and themed scrollbars | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (build tested) | WIP |
| Single/multiple-mode `accordion` | Athena themed host (tested) | Emulated themed host (tested) | Motif themed host (tested) | XView/OLGX host (tested) | WINGs themed host (tested) | GEM themed host (tested) | Composite HWND (tested) | Native-look BView (tested) | NSStackView + disclosures (tested) | WIP |
| Borrowed-page four-edge `tab_view`, framed or strip-only | Athena directional host (build tested) | Edge-aligned emulated directional host (tested) | `XmNotebook`, rotated side labels, native separator (build tested) | XView/OLGX directional host (build tested) | `WMTabView` framed top, WINGs-matched directional/strip-only fallback (tested) | GEM directional host (build tested) | Win32 tab control and separator (build tested) | `BTabView` on all four sides with native rotated labels (tested) | `NSTabView` + `NSTabViewItem` (build tested) | WIP |
| Wrapping, scrolling `icon_view` | Athena themed grid (tested) | Emulated themed grid (tested) | Spatial XmContainer (tested) | XView/OLGX grid (tested) | WINGs themed grid (tested) | GEM themed grid (tested) | WC_LISTVIEW (tested) | Native-look BView grid (tested) | NSCollectionView (tested) | WIP |
| Classic hierarchical `tree_view` | Athena themed tree (build tested) | Emulated themed tree (tested) | XmContainer outline for both presentations (tested) | XView/OLGX tree (build tested) | WINGs themed tree (build tested) | GEM themed tree (build tested) | WC_TREEVIEW (build tested) | BOutlineListView (tested) | NSOutlineView (build tested) | WIP |
| Virtual multi-column `table_view` | Athena themed table (tested) | Emulated themed table (tested) | XmContainer plus virtual fallback (tested) | XView/OLGX table (tested) | WINGs themed table (tested) | GEM themed table (build tested) | Report ListView/owner data (tested) | BColumnListView plus virtual fallback (tested) | NSTableView (tested) | WIP |
| UTF-8 source `code_edit` with gutter and overlays | Athena themed host (tested) | Emulated themed host (tested) | Motif themed host (tested) | XView/OLGX host (tested) | WINGs themed host (tested) | GEM themed host (build tested) | Custom themed HWND (tested) | Native-look BView (tested) | Native-look NSView (tested) | WIP |
| Two-pane `split_view` | Xaw `Paned` (tested) | Emulated host (tested) | `XmPanedWindow` (tested) | XView pane host (tested) | `WMSplitView` with pane notifications (tested) | GEM host (build tested) | Win32 splitter host (build tested) | `BSplitView` (tested) | `NSSplitView` (build tested) | WIP |
| Typed UTF-8 text clipboard | Yes (build tested) | Yes (tested) | Yes (build tested) | XView Selection (tested) | WINGs Selection (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Lossless RGBA image clipboard | PNG selection (build tested) | X11 PNG (build tested), fallback (tested) | PNG selection (build tested) | PNG selection (build tested) | PNG selection (build tested) | AES PNG plus standard IMG scrap (build tested) | PNG/DIBV5 (build tested) | PNG MIME (build tested) | PNG/TIFF (untested) | WIP |
| Native/emulated single-line `text_edit` | Athena (build tested) | Shared selection palette and bounds-clamped scrolling (tested) | Motif (build tested) | XView Panel (tested) | WMTextField (tested) | Yes (build tested) | EDIT (build tested) | BTextView (build tested) | NSTextField (untested) | WIP |
| Native/emulated multiline `text_edit` | Athena (build tested) | Yes (build tested) | Motif (build tested) | XView Panel (tested) | WMText (build tested) | Yes (build tested) | EDIT (build tested) | BTextView (build tested) | NSTextView (untested) | WIP |
| Live complete-value validation | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Direct and keyboard copy/cut/paste | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| `vision` build | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | WIP |
| `vision` runtime | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No (not run) | Yes (tested) | Yes (tested) | Yes (tested) | No (not run) |

## Drawing, text, and image matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `gpx` ink/paper/pen/font state and clipping | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: complete-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Scoped `gpx` state restoration | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Ellipse, polyline, and polygon drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Bounded aligned/ellipsized UTF-8 text | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Cropped/scaled nearest or linear RGBA images | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| `gpx_img`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: image-to-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `img`: owned RGBA storage and direct pixel access | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Six semantic stock-font roles | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Installed-font enumeration and portable descriptions | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Portable font creation from file or copied memory | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Font, text-run, and Unicode-character measurement | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| PNG file load/save and memory decode/encode | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| JPEG file load/save and memory decode/encode | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| `theme` factory, metrics, palette, and state preservation | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed button drawing | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed check/radio drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Staged control, window, panel, and splitter owner drawing | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (untested) | WIP |
| Themed menu bar/title/item/popup drawing | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (build tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed complete-list and list-item drawing | Yes (build tested) | Yes (build tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Themed editable-text frame drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (build tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Semantic surfaces, selection, focus, disclosure, separators, and scrollbar parts | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (build tested) | WINGs (build tested) | Yes (build tested) | Yes (build tested) | BControlLook/palette (build tested) | AppKit/palette (build tested) | WIP |

## Notes

- Linux X11 uses Xt for its event loop and Athena `Command`, `MenuButton`,
  `SimpleMenu`, `SmeBSB`, `Toggle`, and `List` widgets for buttons, menus, and
  selection controls.
- Linux OPEN LOOK uses the XView notifier, Frame and Panel windows, Panel
  controls, OpenMenu menus, `File_chooser`, and Selection objects. Custom
  native-window primitives use OLGX with the active Panel resources.
- Linux Window Maker uses the WINGs event dispatcher, windows,
  command/switch/radio buttons, lists, text widgets, file panels, scrollers,
  and selection handlers. Its application menu is a click-persistent
  Window Maker context-style popup with mnemonics, accelerators, and standard
  hover switching between top-level titles after one menu is open, rather than
  the press-drag WINGs selector control. Custom native-window primitives use
  WINGs relief, screen colors and fonts, and the same indicator pixmaps as
  native controls. Selected bottom/right tabs retain the same single joining
  line as top/left tabs; inactive ones suppress the redundant closure and
  shadow their free edge.
- Modal windows use Win32 owners, Xt transient shells and exclusive grabs,
  XView subframes and busy owners, Haiku modal subsets, and AppKit sheets.
  WINGs uses transient panels plus portable owner input gating. SDL2 uses its
  modal-parent API plus portable input filtering; GEMix enforces modality in
  its multi-window AES event dispatcher. Modeless lifetime always follows the
  portable owner graph; backends avoid native grouping that would force
  modeless windows permanently above their owners.
- `open_file_dialog`, `save_file_dialog`, and `directory_dialog` adapt every native chooser to the
  same `modal_wnd` session and `dialog_result` contract. Windows, AppKit,
  Haiku, Motif, XView, WINGs, and GEM use their standard panels. Athena
  prefers Zenity or KDialog and falls back to an Xaw browser. SDL2 consistently
  uses its compact library-owned themed C++ filesystem browser for open, save,
  and folder selection. It provides icon-backed Places and Name/Type/Size
  tables, icon-only history navigation, a breadcrumb that toggles direct path
  editing on double click, and a continuously draggable scrollbar without pagination.
  Its combo popups retain pointer
  priority when they overlap another input control, open from either field
  region, and track a hot row as the pointer moves.
- `message_box` maps typed button sets and semantic icons to each backend's
  standard alert presentation. SDL2 uses a library-themed modal alert with
  its attributed embedded PNG badge set; Window Maker uses those semantic
  badges inside the native WINGs alert and retains its controls and fonts while
  supplying the frame title.
  The other backends use their toolkit alert APIs. Rulers and status bars are host-owned non-client strips
  that consume `wnd::get_client_bounds()`. Windows maps status parts to
  `STATUSCLASSNAME`; other current backends paint their active theme roles.
- The Xaw fallback, SDL2 browser, Motif, XView, WINGs, and GEM file selection
  are single-path.
  AppKit and Haiku preserve their native overwrite safeguards. These older or
  mandatory native behaviors are conservative reductions of optional public
  settings.
- OpenMotif uses `XmToggleButton` and `XmList`; Windows uses BUTTON and LISTBOX;
  Haiku uses `BCheckBox`, `BRadioButton`, and `BListView`; macOS uses `NSButton`
  and `NSTableView`.
- Windows keeps the text-only `list` on LISTBOX but maps `icon_view` to the
  common-control `WC_LISTVIEW`. macOS similarly keeps `list` on `NSTableView`
  and maps `icon_view` to `NSCollectionView`.
- `table_view` remains model-backed on every backend. Windows uses report-mode
  ListView with owner data for virtual models, macOS uses `NSTableView`, and
  OpenMotif uses `XmContainer` detail view for explicit materialized mode.
  Other toolkit ports request only viewport rows through their native-look
  table hosts. Every adapter fills unused width with the final visible column
  by default; library-painted hosts also fill the header corner above the
  vertical scrollbar, normally fit complete final-page rows, and give arrow
  buttons and gripped thumbs matching raised relief.
- `code_edit` keeps UTF-8 source, line indexes, undo records, style runs,
  diagnostics, and markers in the portable document. Every backend hosts the
  library-painted gutter and text presentation with its own theme palette and
  routes native focus, pointer, keyboard, text-input, and clipboard events
  back to that document. Lexers, completion providers, debuggers, and optional
  session sidecars remain application-owned.
- Accordion and custom icon-grid backends compose reusable theme surfaces,
  selection, focus, compact disclosure, separator, and classic scrollbar
  parts. Painted collection scrollbars use the same arrows, trough, and
  gripped thumb as painted tables. They do not copy Windows or macOS styling
  onto another toolkit. Accordion and classic tree outer borders default on
  and can be disabled explicitly.
- Window Maker uses WINGs `WBTToggle`/`WBTRadio` buttons and `WMList`. The
  list's supported user-draw hook applies the same dark selection role as
  Window Maker collection and table controls instead of WINGs' stock white
  selected row.
- GEMix owns the event handling and native-look emulation for its `check`,
  `radio`, and `list` windows.
- Text editors use Athena `AsciiText`, Motif `XmTextField`/`XmText`, Win32
  `EDIT`, XView `PANEL_TEXT`/`PANEL_MULTILINE_TEXT`, Haiku `BTextView`, and
  AppKit `NSTextField`/`NSTextView`. Window Maker uses `WMTextField`/`WMText`.
  SDL2 and GEMix keep their editor state in their own event and paint
  backends.
- SDL2 supplements SDL's text clipboard with an X11 `CLIPBOARD` selection
  provider for Unicode text and PNG. Its non-X11 fallback retains PNG only for
  the current process when the active SDL platform exposes no image format.
- SDL2 uses `SDL2_ttf` for native stock faces when it is available and a
  built-in bitmap stock-font fallback otherwise. Portable file/memory fonts
  always use the shared byte-backed rasterizer.
- Portable file/memory fonts use the same vendored TrueType rasterizer on all
  backends. Installed-font enumeration remains machine-specific and returns
  file paths and collection face indices for explicit selection.
- Windows runtime checks in this workflow use Wine and require MinGW runtime
  DLLs for MinGW-built executables.
- OpenMotif uses widget resource colors when they are available through Motif/Xt.
  Its theme also uses Motif `Xme` shadow primitives for native-window targets.
  If resource lookup or native drawing is unavailable, the backend supplies a
  Motif-specific emulation.
- Window Maker uses WINGs screen resources, including stipple and shadow
  colors, system and bold fonts, button relief, native switch/radio indicator
  pixmaps, and actual WINGs scrollers for collection/table hosts. Its reference
  session aligns WINGs panel gray with the desktop inactive-title gray and
  reserves white content for text/document editing.
- Windows themes use native GDI control primitives and system colors. Haiku
  themes use `BControlLook`, and macOS themes use AppKit cells and colors.
- X11/Athena, SDL2, and GEMix keep their native-look emulation in their own
  toolkit backends because they do not expose a suitable painter for every
  graphics target.
- Haiku runtime checks in this workflow use Docker for the build, then copy the
  binaries to a Haiku machine and launch them there over SSH.
- The 2026-09-04 Haiku VM walkthrough checked repeated Gallery selection,
  accordion cycling, always-visible tree disclosure, splitter resizing,
  materialized table Size headings and values while scrolling, full-width
  group backgrounds and grid gutters, matching native header/scrollbar parts,
  all four native tab orientations, and combo typing and popup selection.
  Haiku graphics isolate native view state around each primitive; painted
  scrollbars use the system thumb preference rather than a fixed grip.
- The 2026-09-05 Haiku follow-up corrected checkbox release repainting,
  alternating rows in both table modes, accordion focus outlines, and combo
  composition. Both tables now own real native scrollbars and preserve the
  same row pitch. Combo popups match the field width and open below it;
  the editable arrow shares the text field's outer frame.
- A tested drawing row means the implementation is covered by the current
  build/runtime workflow. It does not imply pixel-perfect visual comparison of
  every state on every desktop theme.

## Why this matrix exists

This matrix is the contract for documentation quality:

- if code is implemented but not tested, we mark it untested
- if code is still moving, we mark it WIP
- when runtime checks expand, this table should be updated immediately
