# Feature Matrix

This chapter records what is implemented now and what is currently exercised in
runtime checks. For the contracts and examples behind the drawing rows, see
[Drawing Primitives](drawing-primitives.md).

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
| Linux OPEN LOOK/XView | `docker-openlook` | Yes (tested) |
| Linux Window Maker/WINGs | `docker-wmaker` | Yes (tested) |
| Linux GEMix | `docker-gemix` | Yes (untested) |
| MS Windows (MinGW) | `docker-win` + Wine | Yes (tested) |
| Haiku | `docker-haiku` + SSH deploy/run | Yes (tested) |
| Apple | platform code present | Yes (untested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Build through current Docker workflow | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No | WIP |
| `app::run` startup path | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Screen detection | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Main window create/show | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Independent owned `modeless_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Owner-blocking, focus-taking `modal_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Nested modal stack and `dialog_result` | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Standard `open_file_dialog` | Desktop/Xaw (tested) | Desktop helper (build tested), unavailable cancel (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (build tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSOpenPanel (untested) | WIP |
| Standard `save_file_dialog` | Desktop/Xaw (tested) | Desktop helper (build tested), unavailable cancel (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (build tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSSavePanel (untested) | WIP |
| Paint event (`on_wnd_paint`) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse move | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse button press/release | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse wheel | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Native/emulated `check` control | Yes (build tested) | Yes (build tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Native/emulated sibling-exclusive `radio` control | Yes (build tested) | Yes (build tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Native/emulated single-selection `list` control | Yes (build tested) | Yes (build tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Typed UTF-8 text clipboard | Yes (build tested) | Yes (tested) | Yes (build tested) | XView Selection (tested) | WINGs Selection (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Lossless RGBA image clipboard | PNG selection (build tested) | X11 PNG (build tested), fallback (tested) | PNG selection (build tested) | PNG selection (build tested) | PNG selection (build tested) | AES PNG plus standard IMG scrap (build tested) | PNG/DIBV5 (build tested) | PNG MIME (build tested) | PNG/TIFF (untested) | WIP |
| Native/emulated single-line `text_edit` | Athena (build tested) | Yes (tested) | Motif (build tested) | XView Panel (tested) | WMTextField (tested) | Yes (build tested) | EDIT (build tested) | BTextView (build tested) | NSTextField (untested) | WIP |
| Native/emulated multiline `text_edit` | Athena (build tested) | Yes (build tested) | Motif (build tested) | XView Panel (tested) | WMText (build tested) | Yes (build tested) | EDIT (build tested) | BTextView (build tested) | NSTextView (untested) | WIP |
| Live complete-value validation | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Direct and keyboard copy/cut/paste | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| `vision` build | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | WIP |
| `vision` runtime | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No (not run) | Yes (tested) | Yes (tested) | No (not run) | No (not run) |

## Drawing, text, and image matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `gpx` ink/paper/pen/font state and clipping | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: complete-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
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
| Themed check/radio drawing | Yes (build tested) | Yes (build tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Themed menu bar/title/item/popup drawing | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (build tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed complete-list and list-item drawing | Yes (build tested) | Yes (build tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Themed editable-text frame drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (build tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |

## Notes

- Linux X11 uses Xt for its event loop and Athena `Command`, `MenuButton`,
  `SimpleMenu`, `SmeBSB`, `Toggle`, and `List` widgets for buttons, menus, and
  selection controls.
- Linux OPEN LOOK uses the XView notifier, Frame and Panel windows, Panel
  controls, OpenMenu menus, `File_chooser`, and Selection objects. Custom
  native-window primitives use OLGX with the active Panel resources.
- Linux Window Maker uses the WINGs event dispatcher, windows, pull-down
  buttons, command/switch/radio buttons, lists, text widgets, file panels, and
  selection handlers. Custom native-window primitives use WINGs relief,
  screen colors and fonts, and the same indicator pixmaps as native controls.
- Modal windows use Win32 owners, Xt transient shells and exclusive grabs,
  XView subframes and busy owners, Haiku modal subsets, and AppKit sheets.
  WINGs uses transient panels plus portable owner input gating. SDL2 uses its
  modal-parent API plus portable input filtering; GEMix enforces modality in
  its multi-window AES event dispatcher. Modeless lifetime always follows the
  portable owner graph; backends avoid native grouping that would force
  modeless windows permanently above their owners.
- `open_file_dialog` and `save_file_dialog` adapt every native chooser to the
  same `modal_wnd` session and `dialog_result` contract. Windows, AppKit,
  Haiku, Motif, XView, WINGs, and GEM use their standard panels. Athena
  prefers Zenity or KDialog and falls back to an Xaw browser. SDL2 delegates
  to the same desktop helpers and completes as cancelled when neither is
  installed.
- The Xaw fallback, Motif, XView, WINGs, and GEM file selection are
  single-path.
  AppKit and Haiku preserve their native overwrite safeguards. These older or
  mandatory native behaviors are conservative reductions of optional public
  settings.
- OpenMotif uses `XmToggleButton` and `XmList`; Windows uses BUTTON and LISTBOX;
  Haiku uses `BCheckBox`, `BRadioButton`, and `BListView`; macOS uses `NSButton`
  and `NSTableView`.
- Window Maker uses WINGs `WBTToggle`/`WBTRadio` buttons and `WMList`.
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
- Window Maker uses active WINGs screen resources, including its stipple and
  shadow colors, system and bold fonts, button relief painter, and native
  switch/radio indicator pixmaps.
- Windows themes use native GDI control primitives and system colors. Haiku
  themes use `BControlLook`, and macOS themes use AppKit cells and colors.
- X11/Athena, SDL2, and GEMix keep their native-look emulation in their own
  toolkit backends because they do not expose a suitable painter for every
  graphics target.
- Haiku runtime checks in this workflow use Docker for the build, then copy the
  binaries to a Haiku machine and launch them there over SSH.
- A tested drawing row means the implementation is covered by the current
  build/runtime workflow. It does not imply pixel-perfect visual comparison of
  every state on every desktop theme.

## Why this matrix exists

This matrix is the contract for documentation quality:

- if code is implemented but not tested, we mark it untested
- if code is still moving, we mark it WIP
- when runtime checks expand, this table should be updated immediately
