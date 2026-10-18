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

## OPEN LOOK Docker runtime (2026-10-18)

The maintained OPEN LOOK F5 profile builds and runs in
`wischner/gcc-x86_64-linux-openlook:latest`, using the image's
`openlook-xephyr` helper and olwm. The Docker Debug build passes, all 22 CTests
pass in that nested session, and Vision creates its feature-gallery window.
The GDB/MI pipe also starts successfully with clean protocol output. This run
uses the standalone launcher's existing `ASAN_OPTIONS=detect_leaks=0` setting;
it does not establish new leak-checking coverage. Earlier Tribblix results
below remain historical evidence.

## Current standards review (2026-10-15)

All nine existing Docker build targets pass. Current full-suite results are
SDL2 24/24, X11 19/19, OpenMotif 22/22, OPEN LOOK 22/22, Window Maker 18/18
and GEMix direct/gemd 18/18 each: 141/141 Linux CTests pass. macOS on leia
passes 17/17 including real CGL and GLSL history tests. The final Windows VM
run passes 16 tests and skips two OpenGL tests because no suitable desktop
OpenGL context is available. The final Haiku VM run passes all 18 regression
executables, including real GLSL pixels/history and context ownership, native
inspector input/pixels/recreation, lifecycle/close and two-window stress.
The original eleven-pass GLSL terminal smoke also passes with Mesa softpipe. Both deployments were user-approved and all deployed test
hashes match the final Docker artifacts.

The [adjustment record](../../ADJUST.md) documents the A01–A18 repairs and
verification. Native callback continuations now observe object/resource
lifetimes, EGL displays have shared leases, GLSL installation/history is
transactional, geometry widens before calculation, and creation rolls back all
shared acquisition. These regressions do not certify every execution path.

Toolkit-only controls independently reproduce retained Motif selection/notebook,
XView drop-interest and WINGs font-pattern allocations. Affected runtime tests
use narrow SDK allocation-path exceptions with leak checking enabled; Native's
XView menu-item ownership leak is fixed. The real GLSL test keeps Mesa driver
metadata resident without leak suppressions. See the build chapter and
ADJUST.md for exact policies; historical test-specific sanitizer settings are
not a whole-suite leak-free claim.

`docker-amiga-check` currently fails C++20 runtime qualification. It is not an
Amiga backend, Native library build or deployment result.

## Mouse and programmable image shaders (2026-10-07)

The shared `native-image-1` executor runs on the CPU and draws through each
backend's existing canvas/image compositor. This CPU profile has no GPU
acceleration. The separate `native-glsl-1` profile has private OpenGL adapters
on Linux, Windows, macOS and Haiku; its live execution/capabilities and runtime
requirements are recorded separately. Haiku uses OSMesa software rendering. No GPU performance acceptance is claimed.

| Backend | Hidden client cursor | Public drag capture | Relative capture | Image profile / history | New adapter evidence |
| --- | --- | --- | --- | --- | --- |
| X11/Athena | Yes | Yes | Unsupported | Yes | Docker build; shared pixel/input tests |
| SDL2 | Yes | Yes | Device-count deltas | Yes | Docker build; full regression suite including native canvas cancellation |
| OpenMotif | Yes | Yes | Unsupported | Yes | Docker build; shared pixel/input tests |
| OPEN LOOK | Yes | Yes | Unsupported | Yes | Docker build; shared pixel/input tests |
| Window Maker | Yes | Yes | Unsupported | Yes | Docker build; shared pixel/input tests |
| GEMix, direct and gemd | Yes | Unsupported | Unsupported | Yes | Both Docker builds; shared pixel/input tests |
| Windows | Yes | Yes | Unsupported | Yes | Docker cross-build; native terminal pixels/shell/hidden-cursor shutdown |
| Haiku | Yes | Unsupported | Unsupported | Yes | Docker cross-build; 18 native regression executables; real OSMesa context/pixels and original-GLSL terminal smoke with softpipe |
| macOS | Yes | Unsupported | Unsupported | Yes | Debug build; full 17-test suite and real CGL context test pass; original-GLSL terminal smoke also passes |

No backend guarantees unaccelerated relative input. Unsupported acquisition
returns an explicit diagnostic. Desktop outside restoration, native occlusion,
menu/modal takeover and actual capture loss still need interactive acceptance;
portable event assertions and prior gallery checks do not establish those new
behaviors on every host. `vision --mouse-shaders` provides the acceptance view
beside a native editor, with cursor policy, color changes and Escape release.

The historical 2026-10-07 CPU-profile validation passed all 84 CTests: X11 12, SDL2 16, Motif 12,
OPEN LOOK 12, Window Maker 10, GEMix direct 11 and gemd 11. The new
`native_mouse_shader_tests` checks ordered boundaries, disabled-child occlusion,
re-entry deltas, focus-transfer cancellation, generation-safe callbacks, actual identity RGBA
pixels, multipass/history, invalid package dependencies and budgets, source
ownership, transactional replacement and recreation. Six Python compiler
tests also pass for reproducible packages and rejected executable expressions.
Windows and Haiku were cross-built in that mouse validation pass; subsequent terminal coverage is recorded below and does not establish mouse-capture acceptance.

The optimized SDL Docker build also passes all sixteen CTests. A single first
CRT executor tick in that Release build measured 22.1 ms at 256×192 and
740.5 ms at 1920×1080 on this host, including input linearization and output
conversion. These are executor measurements, not end-to-end display latency,
steady-state averages, or a frame-rate guarantee. The Debug build measured
587 ms and 24.4 seconds respectively while other builds were running; use
optimized builds for CPU effect performance. Reproduce with
`build/linux-sdl2/tests/native_mouse_shader_tests --benchmark` inside the image.

Effects execute synchronously on the UI thread. The CPU profile is appropriate
for bounded image processing, not a promise of real-time full-HD CRT rendering.
See [Image shader patterns](PATTERNS-IMAGE-SHADERS.md) for resource limits and
[the programming chapter](../programming-native/20-IMAGE-SHADERS.md) for the
portable format and authoring tool.

## CRT command terminal

`vision --retro-terminal` currently loads the original cool-retro-term and Qt
blur GLSL pass bodies from `terminal-glsl.nshader`. Its eleven-pass graph uses
packaged noise and retained burn-in textures in the private OpenGL renderer.
It no longer uses the earlier eight-pass CPU approximation as the displayed
terminal effect. Historical `terminal.json`/`terminal.nshader` remain CPU
fixtures for parser/font/material/pixel tests; passing those fixtures does not
establish original-GLSL behavior.

The demo renders a bounded ASCII/ANSI screen using the attributed portable
TrueType font and runs line-oriented commands through bounded `process`
capture. POSIX hosts use `/bin/sh -c`; Windows uses PowerShell `-Command`.
There is no PTY, persistent shell session, Unicode cell layout or full-screen
program support. Asset provenance and licenses remain beside the executable.

The original GLSL terminal smoke passes in the Linux Docker selections,
on macOS leia and on the Haiku VM with Mesa softpipe. Windows has current
compile coverage only for this GLSL pipeline; its VM lacks desktop OpenGL.
Older Windows/Haiku terminal evidence refers to the preceding CPU demo.
Functional smoke success requires active shader output and the expected
completed shell command; it is not a frame-rate guarantee or proof of exact
upstream visual parity. The user confirmed the visible Haiku demo works.
Shared display ownership, live installation and history repairs are recorded
in ADJUST.md.

OpenMotif, Window Maker and OPEN LOOK wake their production loops through
nonblocking pipes when workers post results. OPEN LOOK synchronizes pipe
publication/write/close and checks nonblocking/close-on-exec setup.

## Infinity services (2026-10-06)

This table records the newly added service adapters separately from older
control/gallery runtime evidence. The diagnostic application is
`vision --infinity-test`; its acceptance checklist is in
[Background Work and Services](../programming-native/19-BACKGROUND-WORK-AND-SERVICES.md).

| Backend | Physical input | PCM output | Scoped UI delivery / child process |
| --- | --- | --- | --- |
| SDL2 | Yes (tested): actual event dispatch, pairing, focus reset and editor isolation | Yes (tested with dummy audio); audible playback untested | Yes (tested): shared queue/lifetime tests and real POSIX child |
| X11/Athena | Yes (build tested): XKB positions | Yes (build tested): ALSA adapter; audible playback untested | Yes (tested): shared tests and real POSIX child |
| OpenMotif | Yes (build tested): XKB positions | Yes (build tested): ALSA adapter; audible playback untested | Yes (tested): shared tests and real POSIX child |
| OPEN LOOK | Yes (build tested): XKB positions | Yes (build tested): ALSA adapter; audible playback untested | Yes (tested): shared tests and real POSIX child |
| Window Maker | Yes (build tested): XKB positions | Yes (build tested): ALSA adapter; audible playback untested | Yes (tested): shared tests and real POSIX child |
| GEMix direct / gemd | WIP: SDK/viewer lacks complete raw release/focus integration; capability reports false | Yes (build tested): ALSA adapter; audible playback untested | Yes (tested) on both transports: shared tests and real POSIX child |
| Windows | Yes (build tested): Win32 physical input | Yes (build tested): WASAPI; audible playback untested | Yes (build tested): shared tests and Win32 process adapter |
| Haiku | Yes (build tested): raw positions and modifier state | Yes (build tested): BSoundPlayer; audible playback untested | Yes (build tested): shared tests and POSIX process adapter |
| macOS | Yes (build tested): AppKit adapter; physical held-key acceptance pending | Yes (build tested): AudioQueue adapter; audible playback untested | Yes (tested): shared queue/lifetime, PCM consumer and real process tests on leia |

All nine Docker selections build the service tests and Vision. The shared
PCM test uses a deterministic private consumer and verifies bounded copying,
order, stereo frame arithmetic, hardware occupancy, silence, failure, concurrent
producer/consumer access and reopening. Scope tests cover concurrent close,
reentrant destruction and resource recreation. Process tests launch a real
child, verify arguments/environment/directory, drain both full pipes under
capture limits, and cancel/reap, including POSIX timeout escalation.

The original 2026-10-06 validation had 15 SDL2, 11 X11, 9 WINGs and 10
tests per GEM transport; Motif/XView behavior passed with leak detection
disabled and reported allocations when enabled. That historical result is
superseded by the current repair results and narrow independently reproduced
SDK exceptions at the top of this chapter. macOS source sync was subsequently
authorized and its service suite now runs on leia. Do not infer real-device
sound or physical-key acceptance from deterministic consumer/model tests.

## Backend status

Every backend routes native window-close requests through the portable
`app_wnd::request_close()` hook. SDL2 also routes application quit through it.
Its default destroys the resource; overrides can keep the event loop alive
during asynchronous save prompts. Real native cancellation/acceptance tests
cover the adapter routes. Explicit destroy() forces resource teardown.
SDL2 also composes panels, tabs, split views, accordions, and canvases in
parent-depth order, preserving canvas pixels inside nested containers.
Viewport changes clear the previous renderer clip so accelerated rendering
preserves trailing tab strips. The 2026-10-06 four-edge pixel regression passes
with software and desktop renderers, and the input-chrome gallery visually
confirms the right-side labels and borders.
All 11 SDL2 CTests pass under the Docker toolchain after this correction.
Canvas input retains left/middle/right identity and captures drags through
release, including motion outside the original surface. Dedicated SDL
composition and input tests cover these paths. Software-renderer requests
bypass accelerated initialization, and loop shutdown releases video while preserving independently owned audio
peers.
The 2026-09-06 SDL2 follow-up passes all nine CTests under the Docker
toolchain, including the renderer-pixel, real SDL input, and close-policy
regressions.

The 2026-09-05 macOS native-control audit replaces default portable drawing
inside AppKit button/check/radio, table, outline, icon and accordion hosts.
`native_mac_runtime_tests` verifies native rendering without entering the
portable painter, retained derived-button drawing, native cell subviews and
disclosure/selection actions. All six CTests pass on `leia`, including the
now-registered collection runtime test, both normally and with address and
undefined-behavior sanitizers. The gallery has been inspected in a desktop
screenshot after enabling Screen Recording. Accessibility input now works;
the live walkthrough reproduced an empty split view and overflowing tab pages.
The fixes use native pane allocation and `NSTabViewItem` page hosts. Expanded
regressions cover both split orientations, all tab placements and resizing,
native grid toggling, virtual million-row scrolling and row-height reset.
Tables use full-width AppKit grids and native row views with stronger dynamic
system-derived grid/stripe colors; native scroll state updates the portable
viewport cache. Follow-up regressions cover upright image glyph pixels and
straight-alpha text blending, single-open accordion scrollbar ownership and
light/dark grid contrast. These three reported issues received targeted
desktop checks; the earlier complete-gallery walkthrough remains incomplete.

The 2026-09-05 OPEN LOOK follow-up exercises native radio exclusion,
intrinsic button height, partial-exposure theme pixels, coalesced invalidation,
editor keyboard/completion routing, chrome construction, resized list panes,
and repeated modal/modeless teardown in `native_openlook_runtime_tests`.
Tab regressions cover all four placements, page switching and several heights:
native list borders stay inside the host, bottom pages meet the tab strip,
and spare page space does not retain provisional list borders. Pixel checks
verify separate splitter grip ribs in both orientations. The Tribblix
visual checks also cover bottom/side joins and live dragging with the ribbed,
unboxed splitter grip beside native scrollbars.
`native_collection_runtime_tests` is also registered for OPEN LOOK. The tests
run in the native Tribblix VM and the Docker XView build with ASan/UBSan.
All six CTests pass in both environments; the focused OPEN LOOK regression
also passes ten consecutive runs after the final tab-page repaint fix.
Gallery walkthroughs use the VM and retain native left-aligned, fixed-height
OPEN LOOK buttons; theme primitives use matching OLGX button/choice geometry.
Explicit pane cleanup and deferred, callback-detached window destruction
prevent stale XView handles during subsequent window openings. A centered
painted grip marks the captured splitter, and both native lists reserve their
scrollbars when resized. Editable combo fields use XView's display-width
attribute so their underline does not extend beneath neighboring controls.
Tab page switches repaint the new native items after old-page cleanup, with
a pixel regression preventing blank content after switching in either direction.

The 2026-09-05 X11 follow-up passes six CTests under a private Xvfb server.
`native_x11_runtime_tests` checks pre-show graphics/theme lookup, full-row
list selection before/after resize, full accordion geometry, idle repaint
counts, collection/table borders, scrollbar separators, bottom-row coverage,
growing/shrinking grid children, combo sizing, full-width popup placement,
single-click native selection/text synchronization, pointer-hover highlighting,
centered message buttons and parent-centered shells (including moved/off-screen
and hidden-owner cases), all four open tab joins, individual menu borders
without a full-width bar rule, and server-delivered two-pane separator movement with preserved
child borders, vertical/zero-ratio native backing, monochrome collection pixels,
unaltered canvas images and all four copied alert bitmaps. X11 collections
display monochrome icons and white alternating rows; alerts use copied local
monochrome artwork. `native_x11_scrollbar_tests` verifies actual Xaw widgets
for tables, icon grids, trees and canvases, native middle-button dragging,
horizontal ranges, million-row endpoints, signed canvas origins and hiding.
Gallery checks run under monochrome TWM inside Xephyr and cover combo hover,
table/accordion dragging, centered message buttons, the splitter grip and
menu borders. Combo field text is vertically centred and unclipped at 1x and
2x (checked 2026-10-18 in the input/chrome window); the backend works around
libXaw's stale effective top margin after `SetValues`. Seven SDL2 CTests
passed in the preceding review after the shared tab-text contrast correction.

| Backend | Build path | Runtime status |
| --- | --- | --- |
| Linux X11 | `docker-x11` | Yes (tested) |
| Linux SDL2 | `docker-sdl2` | Yes (tested) |
| Linux OpenMotif | `docker-openmotif` | Yes (tested) |
| Linux OPEN LOOK/XView | `docker-openlook` + Docker Xephyr/olwm | Yes (tested) |
| Linux Window Maker/WINGs | `docker-wmaker` + Bookworm VM native build | Yes (tested) |
| Linux GEMix | `docker-gemix` + local rasta | Yes (tested) |
| Linux GEMix via gemd | `docker-gemix-gemd` + local rasta | Yes (tested) |
| MS Windows (MinGW) | `docker-win` + native Windows 11 VM | Yes (tested) |
| Haiku | `docker-haiku` + SSH deploy/run | Yes (tested) |
| Apple | remote build and runtime checks | Yes (tested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Per-edge outer border flags | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Custom property dropdowns and value converters | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Compact typed property grid | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Four-edge toolbars, sticky groups, 16/24/32px icons | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Build through the current platform workflow | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | WIP |
| `app::run` startup path | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | WIP |
| Screen detection | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Exact-size file/directory PNG icons | Freedesktop + fallback (build tested) | Freedesktop + fallback (tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Freedesktop + fallback (build tested) | Shell + fallback (tested) | Tracker + fallback (build tested) | AppKit + fallback (tested) | WIP |
| Typed special-directory detection | XDG/C++ (build tested) | XDG/C++ (tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | XDG/C++ (build tested) | Known Folders (tested) | `find_directory` (build tested) | Foundation (tested) | WIP |
| Main window create/show | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | WIP |
| Central non-const lifecycle and visible-state contract | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (untested) | WIP |
| Independent owned `modeless_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (untested) | WIP |
| Owner-blocking, focus-taking `modal_wnd` | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Nested modal stack and `dialog_result` | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Standard `open_file_dialog` | Desktop/Xaw (tested) | Themed C++ browser (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (tested) | Common Item Dialog (tested) | BFilePanel (build tested) | NSOpenPanel (untested) | WIP |
| Standard `save_file_dialog` | Desktop/Xaw (tested) | Themed C++ browser (tested) | Motif (build tested) | XView (tested) | WINGs (tested) | AES (tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSSavePanel (untested) | WIP |
| Standard `directory_dialog` | Desktop/Xaw (build tested) | Themed C++ browser (tested) | Motif (build tested) | XView (build tested) | WINGs (build tested) | AES (build tested) | Common Item Dialog (build tested) | BFilePanel (build tested) | NSOpenPanel (untested) | WIP |
| Standard one-to-three-button `message_box` | Athena (build tested) | Themed modal with embedded PNG badges (tested) | Motif (build tested) | XView (build tested) | WINGs alert with embedded PNG badge and title (tested) | AES (build tested) | Win32 (build tested) | BAlert (build tested) | NSAlert (untested) | WIP |
| Paint event (`on_wnd_paint`) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse move | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse button press/release | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse wheel | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Arrow, I-beam, crosshair, and four resize mouse cursors | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Resize crosshair fallback (build tested) | Yes (build tested) | Yes (build tested) | Diagonal crosshair fallback (build tested) | WIP |
| Native/emulated `check` control | Yes (build tested) | Yes (tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | NSButton (tested) | WIP |
| Native/emulated sibling-exclusive `radio` control | Yes (build tested) | Yes (tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | NSButton (tested) | WIP |
| Native/emulated single-selection `list` control | Yes (build tested) | Yes (build tested) | Yes (build tested) | XView (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| `list_box` compatibility name | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Selection-only/editable `combo_box` | Athena (build tested) | Themed (tested) | XmComboBox (build tested) | XView composite (build tested) | WINGs centered inset-arrow composite (tested) | Themed (tested) | COMBOBOX (build tested) | Native-control composite, below-field popup (tested) | NSComboBox (untested) | WIP |
| Non-client rulers and status bars | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Structural `panel` container | Xaw `Form` child (build tested) | Emulated nested region (tested) | `XmForm` child (build tested) | XView Panel (build tested) | WINGs flat frame (build tested) | GEM themed region (build tested) | Child HWND, control-host brush (build tested) | Child `BView` container (build tested) | Child `NSView` container (build tested) | WIP |
| Paintable `canvas` surface | Athena drawable host (build tested) | Emulated nested region (tested) | Motif `XmDrawingArea` (build tested) | XView Panel with paint window (build tested) | WINGs flat frame (build tested) | GEM offset region (build tested) | Child HWND (build tested) | Child `BView` (build tested) | Child `NSView` (build tested) | WIP |
| Canvas 32-bit content bounds, clamping, and themed arrow/trough/thumb scrollbars | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (tested) | Portable (build tested) | WIP |
| Single/multiple-mode `accordion` | Athena themed host (tested) | Emulated themed host (tested) | Motif themed host (tested) | XView/OLGX host (tested) | WINGs themed host (tested) | GEM themed host (tested) | Composite HWND (tested) | Native-look BView (tested) | NSStackView + disclosures (tested) | WIP |
| Borrowed-page four-edge `tab_view`, framed or strip-only | Athena directional host (build tested) | Edge-aligned emulated directional host (tested) | `XmNotebook`, rotated side labels, native separator (build tested) | XView/OLGX directional host (build tested) | `WMTabView` framed top, WINGs-matched directional/strip-only fallback (tested) | GEM directional host (tested) | Win32 classic tab control (four-edge pages tested; strip-only build tested) | `BTabView` on all four sides with native rotated labels (tested) | `NSTabView` + `NSTabViewItem` (four-edge pages tested; strip-only build tested) | WIP |
| Wrapping, scrolling `icon_view` | Athena themed grid (tested) | Emulated themed grid (tested) | Spatial XmContainer (tested) | XView/OLGX grid (tested) | WINGs themed grid (tested) | GEM themed grid (tested) | WC_LISTVIEW (tested) | Native-look BView grid (tested) | NSCollectionView (tested) | WIP |
| Classic hierarchical `tree_view` | Athena themed tree (build tested) | Emulated themed tree (tested) | XmContainer outline for both presentations (tested) | XView/OLGX tree (build tested) | WINGs themed tree (build tested) | GEM themed tree (tested) | WC_TREEVIEW (build tested) | BOutlineListView (tested) | NSOutlineView (native cells tested) | WIP |
| Virtual multi-column `table_view` | Athena themed table (tested) | Emulated themed table (tested) | Unified Motif-themed viewport and native scrollbars (tested) | XView/OLGX table (tested) | WINGs themed table (tested) | GEM themed table (tested) | Report ListView/owner data (tested) | BColumnListView plus virtual fallback (tested) | NSTableView (tested) | WIP |
| UTF-8 source `code_edit` with gutter and overlays | Athena themed host (tested) | Emulated themed host (tested) | Motif themed host (tested) | XView/OLGX host (tested) | WINGs themed host (tested) | GEM themed host (build tested) | Custom themed HWND (tested) | Native-look BView (tested) | Native-look NSView (tested) | WIP |
| Two-pane `split_view` | Xaw `Paned` (tested) | Emulated host (tested) | `XmPanedWindow` (tested) | XView pane host (tested) | `WMSplitView` with pane notifications (tested) | GEM host (tested) | Win32 splitter host (build tested) | `BSplitView` (tested) | `NSSplitView` (geometry and native divider notifications tested) | WIP |
| Typed UTF-8 text clipboard | Yes (build tested) | Yes (tested) | Yes (build tested) | XView Selection (tested) | WINGs Selection (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Lossless RGBA image clipboard | PNG selection (build tested) | X11 PNG (build tested), fallback (tested) | PNG selection (build tested) | PNG selection (build tested) | PNG selection (build tested) | AES PNG plus standard IMG scrap (build tested) | PNG/DIBV5 (build tested) | PNG MIME (build tested) | PNG/TIFF (untested) | WIP |
| Native/emulated single-line `text_edit` | Athena (build tested) | Shared selection palette and bounds-clamped scrolling (tested) | Motif (build tested) | XView Panel (tested) | WMTextField (tested) | Yes (tested) | EDIT (build tested) | BTextView (build tested) | NSTextField (untested) | WIP |
| Native/emulated multiline `text_edit` | Athena (build tested) | Yes (build tested) | Motif (build tested) | XView Panel (tested) | WMText (build tested) | Yes (tested) | EDIT (build tested) | BTextView (build tested) | NSTextView (untested) | WIP |
| Live complete-value validation | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Direct and keyboard copy/cut/paste | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| `vision` build | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (build tested) | WIP |
| `vision` runtime | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No (not run) |

## Drawing, text, and image matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `gpx` ink/paper/pen/font state and clipping | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: complete-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Scoped `gpx` state restoration | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Ellipse, polyline, and polygon drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Bounded aligned/ellipsized UTF-8 text | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Cropped/scaled nearest or linear RGBA images | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| `gpx_img`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (upright glyph and partial-alpha pixels tested) | WIP |
| `gpx_img`: image-to-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `img`: owned RGBA storage and direct pixel access | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | WIP |
| Six semantic stock-font roles | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | WINGs (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Installed-font enumeration and portable descriptions | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Portable font creation from file or copied memory | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
| Font, text-run, and Unicode-character measurement | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| PNG file load/save and memory decode/encode | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | libpng (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| JPEG file load/save and memory decode/encode | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| `theme` factory, metrics, palette, and state preservation | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed button drawing | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed check/radio drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Staged control, window, panel, and splitter owner drawing | Yes (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (untested) | WIP |
| Themed menu bar/title/item/popup drawing | Yes (tested) | Yes (tested) | Yes (tested) | OLGX (build tested) | WINGs (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed complete-list and list-item drawing | Yes (build tested) | Yes (build tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| Themed editable-text frame drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (build tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (untested) | WIP |
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
  OpenMotif uses the same Motif-themed viewport with native scrollbars for
  both data modes; `XmContainer` remains the native tree/icon peer.
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
- SDL2 follows the desktop scale factor. Under X11 it reads `Xft.dpi`, and
  under SDL's own high-DPI drivers it uses SDL's factor; it never applies
  both. Window, screen, and pointer geometry stays in logical pixels, and
  stock text is rasterized at device resolution. Tested 2026-10-18 in Docker
  Xvfb at `Xft.dpi` 96, 144, and 192: vision's top-level window measured
  exactly 1×, 1.5×, and 2× with identical layout, and XTest clicks at
  device coordinates hit the expected list row, menu, and modal-dialog item.
  SDL Wayland high-DPI is untested.
- X11/Athena follows `Xft.dpi` the same way: geometry, pointer input, and
  screens stay logical, while widgets, drawing, Athena spacing, and library
  bitmaps are scaled to device pixels. Strokes and widget borders stay one
  device pixel. Window text uses the nearest installed bitmap size, for
  example Lucida Typewriter 26 for the 13-pixel control font at 2x, from the
  image's `xfonts-75dpi`, `xfonts-100dpi` and `xfonts-terminus`. Tested
  2026-10-18 in Docker Xvfb at `Xft.dpi` 96, 144, and 192: vision's
  top-level window measured exactly 1x, 1.5x, and 2x. All 19 CTests pass
  unscaled. At 192 the X11 scrollbar, window, lifecycle, and close suites
  pass, including XTest pointer drags. `native_x11_runtime_tests` and
  `native_inspector_runtime_tests` assume 1x and fail there: they expect
  32-pixel alert bitmaps, and they read captures in logical coordinates.
  The gallery was checked visually under 2x TWM in Xephyr.
- Portable file/memory fonts use the same vendored TrueType rasterizer on all
  backends. Installed-font enumeration remains machine-specific and returns
  file paths and collection face indices for explicit selection.
- Windows runtime checks use the native Windows 11 VM, with MinGW runtime
  DLLs beside the executable. The 2026-09-05 walkthrough checked native
  button/check/radio and theme samples, multiline typing, collection icons
  and trees, native table headers and selection, splitter dragging and pane
  selection, layout maximize/restore, combo selection, tab switching,
  folder selection and a native message box. Five test executables pass:
  core, window API, table model, code document, and Windows native peers.
  The peer regression runs in the interactive desktop session and covers
  native styles, retained derived drawing, list row metrics, keyboard and
  clipboard, splitter capture, paint clipping/GDI lifetime and modal siblings.
  Additional regressions verify grouped and owner-data table grid pixels,
  horizontal/vertical grid toggling and virtual-row scrolling,
  all four native classic tab orientations, page insets and compact vertical
  label padding, status-strip
  sibling clipping and hit-testing through repeated resizes, and independent
  modeless stacking without losing modal exclusion.
  The Windows F5 profile uses
  an interactive native GDBserver task and an SSH tunnel; see
  [Windows VM Runtime](../../notes/WINDOWS-VM-RUNTIME.md) for setup and checks.
  Source breakpoints work; interruption of the running GUI remains an open
  debugger issue, so the Windows workflow is not fully debugger-validated.
- OpenMotif uses widget resource colors when they are available through Motif/Xt.
  Its theme also uses Motif `Xme` shadow primitives for native-window targets.
  If resource lookup or native drawing is unavailable, the backend supplies a
  Motif-specific emulation.
  The 2026-09-05 CDE VM walkthrough checked list-font/palette and round-radio
  primitives, the accordion's visible scrollbar, centered tree icons, both
  table modes with grids and a jump to virtual row 900,001, text-field focus,
  splitter movement, and creation of Input and Window Chrome. Motif peer
  regressions run both in Docker/Xvfb and in the native CDE session; they
  cover early graphics, native editor input isolation, native scrollbars,
  table grid pixels, bounded repainting, code contrast and native sashes.
  The collection lifecycle suite also runs under Motif and guards against
  freeing an Xt parent before its portable child peers detach native gadgets.
  The follow-up also covers whole-image tree scaling, raised native label
  baselines, matching accordion/tree disclosures, visible alternating rows
  and contrast-sensitive grids, first-selection notebook layout, modal
  palette/size, and menu-colored ruler paper/marks including the shared corner. The ruler
  surface roles retain existing header/panel defaults on other backends.
- Window Maker uses WINGs screen resources, including stipple and shadow
  colors, system and bold fonts, button relief, native switch/radio indicator
  pixmaps, and actual WINGs scrollers for collection/table hosts. Its reference
  session aligns WINGs panel gray with the desktop inactive-title gray and
  reserves white content for text/document editing.
- Windows stock controls use native Win32 painting and Common Controls v6
  visual styles; derived controls retain the custom-draw extension path.
  Tabs alone use the native classic renderer consistently across all four
  edges because visual styles do not support bottom or vertical tabs.
  Theme samples use matching UxTheme parts and native list metrics. Haiku
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

The 2026-09-06 GEM v1.0.0 compatibility pass builds both transports from
upstream commit `0df463d9ff4b4cbbdd0612ce700c9e11e5e33661` using the published
`wischner/gcc-x86_64-gemix:latest` image, which ships that SDK. All six
Native CTests pass in each Debug build, including actual Rasta input packets
and framebuffer regressions. Native retains ASan/UBSan; the release SDK is
built in Release mode. No C++ backend API adaptation was required. The proxy
executable's GEM dependency is `libgem`, with no direct AES/VDI dependency.
This pass is automated runtime coverage; it does not repeat the earlier
visual walkthrough.

GEMix's 2026-09-05 rasta pass uses the updated sibling GEM runtime as well
as Native. Six CTest executables pass, including the GEM-specific framebuffer
regressions, including the untouched desktop checker on a fresh launch.
The visual pass covers the initial desktop, pressed buttons, collection icons and
borders, table-thumb and splitter dragging, overlapping/modeless and modal
windows, file-selector movement, open/save followed by copy/paste, and the
input/chrome gallery. Untested cells above remain unclaimed; this is not a
claim of complete Atari hardware coverage.
The interaction follow-up also checks focused-editor keyboard copy/paste,
selector press/release feedback, split-pane selection without a resize,
inactive left-tab separators, full title restoration, and menu/popup removal
on Exit. Local rasta does not forward wheel events; wheel scrolling is not
claimed by this pass.

The separate libgem/gemd build passes the same six Native tests. The GEM
repository's real-server RPC regression also passes, including fragmented and
malformed requests, output arrays, reconnects, menu replacement and abrupt
menu-client exit. Visual proxy checks cover editing/clipboard, file and
directory selectors (including saving/reopening a PNG), modal/modeless
windows, layouts, collections, tables, combo choices, message boxes,
splitter dragging and menu teardown. The client binary links libgem without
direct AES/VDI dependencies. This is coverage of Native's GEM call set on the
hosted ABI, not all AES/VDI functions or cross-architecture serialization.
The input follow-up also verifies typing through the actual Rasta viewer into
single-line and multiline fields, Copy field/Paste text, and source-editor
drawing, Enter and arrow navigation. Automated editor checks cover selection
and clipboard shortcuts. The RPC regression preserves held-button state across
queued redraws and checks modifier bursts. A framebuffer regression checks that
window close publishes restored content only at the outermost update boundary.
Window-opening framebuffer checks cover modal/modeless windows wholly above the
owner and partly above the desktop. They inspect the intermediate screen before
client painting: covered content remains intact instead of being erased to the
desktop checker. The regression fails against the preceding runtime.
The GEMix status-height regression also matches the theme metric to the actual
AES title strip; the gallery uses that metric to keep text below its top rule.
GEM status-part painting omits the redundant bottom/right enclosure while
retaining the top separator and leading part dividers.
Pixel regressions check all four status-part edges independently.

The sibling GEM security pass retains six passing Native CTests in each
transport and adds five passing hosted GEM tests. Three real Native processes
share one local-Rasta gemd desktop; automated RPC checks verify separate
window ownership, VDI attributes/clips and focused keyboard queues, plus
menu-owner teardown and stalled-client recovery. Standard AES alerts/selectors
service background clients while retaining classic shared input modality.
Viewer checks include a selector, an alert, editing/copy/paste and a modeless
window. Public AES/VDI interfaces are unchanged. These are resilience checks
for a same-user service, not a claim of isolation from hostile same-UID code.

The modal-frame follow-up checks gadget-free modal hosts, four-pixel client
insets and `1011` pixels on every edge in both transports. AES message-box
edges have an additional real-server pixel regression. The runtime now
returns actual flags from `wind_get(WF_KIND)`, not work-area coordinates.
`native_gemix_input_tests` drives real Rasta packets through `app::run`,
covering rapid clicks, both text modes, clipboard buttons/shortcuts and input
after modal, modeless and file-selector closure. These cases pass in both
transports; the renewed report of dead fields/copy-paste did not reproduce in
fresh viewer sessions, and is not claimed as a separately diagnosed fix.

### GEMix canvas composition (2026-09-07)

Structural surfaces now paint in ancestry order across panels, tabs,
accordions, split views, and canvases. Tab backgrounds no longer erase a
nested canvas or its rulers and scrollbars. Secondary AES mouse-button
transitions reach canvases with right/middle identity intact. The GEMix
runtime regression checks actual nested-canvas framebuffer pixels and
secondary-button dispatch.

The GEM scrollbar arrow correction uses the original outlined stock-font
glyphs with shafts for up, down, left, and right. Canvas and collection
scrollbars share this theme rendering, including pressed inversion and
clipping inside small button frames. The original four bitmap symbols also
serve as the fallback when the installed font cannot be loaded. Both Docker
Debug transports pass all six CTests; the arrow regression compares theme
pixels against raw VDI system-font output in all four directions, released
and pressed, with full/partial clipping and regular/tiny bounds.

The collection-input correction connects GEM icon-grid and tree scrollbars
to the same geometry used for painting. Arrows step on press, troughs page,
and thumbs retain capture outside their modeless window until release.
Scrollbar gestures preserve item selection and activation, and destruction
clears capture even after reparenting. Shared tree painting also preserves
manual scrolling when row metrics are unchanged, so repainting does not
return the viewport to the selected item. The real-Rasta input regression
checks these gestures for an icon grid nested in an accordion and a tree in
a modeless window, including endpoint dragging beyond the window and capture
cleanup after destruction/reparenting. All six CTests pass in each GEM Debug
transport; all nine SDL2 CTests also pass with the shared geometry and tree
repaint changes.

2026-09-07: Shared **C** ruler painting now spaces labels by measured text
width, retaining all ticks at dense scales, and clips to its own strip.
The window API regression checks both orientations and graphics-state
restoration. Maestro uses the standard Native ruler with no custom painter.

The inspector additions use `native_inspector_tests` for all sixteen border
masks, typed-value validation, programmatic silence, sticky groups, multiple
edge reservations, and exact 16x16/24x24/32x32 image pixels. Runtime coverage
for new native edge adapters is distinct from earlier backend-wide runtime
coverage. The Vision Properties and toolbars window is available through
its Window menu and the `--properties` command-line option.

The GEM input regression also exercises nested property text/number typing,
checkbox changes, combo popups outside the grid viewport, grid scrolling,
toolbar commands and exclusive selection, cancelled outside releases, and
composite destroy/recreate. Compact rows were inspected in the GEM gallery.

The 2026-09-07 inspector follow-up builds all nine Docker targets without
compiler warnings. All 55 registered Linux test executions pass: GEM direct
7/7, GEM via gemd 7/7, SDL2 11/11, X11 8/8, Motif 8/8, OPEN LOOK 8/8, and
Window Maker 6/6. Linux Debug builds include address/undefined behavior
sanitizers; the X-based suites run in private Xvfb displays with toolkit
lifetime leak reporting disabled.

`native_inspector_runtime_tests` also passes on the native Windows 11 and
Haiku VM desktops. It checks presented icon/value pixels, native hit-tested
checkbox clicks, text and numeric typing, repeated toolbar activation with
status updates, sticky selection, bottom-edge input, scrolling, and owner
recreation. The application consumes its public create signal deliberately;
internal toolbar resources must still be created. Vision's inspector was
visually checked on X11, Motif, OPEN LOOK, and WINGs. Values and labels share
control paper, and icons preserve alpha and align with toolbar text. The
Win32 property combo also passes native popup selection and a check that its
selected-value paper matches adjacent cells. The
macOS edge adapter and inspector controls remain unbuilt and untested on
macOS in this validation pass.

The sizing/dropdown follow-up adds native post-creation grid resizing, live
32→24→16 toolbar geometry and neighboring-bar hit tests, arbitrary values
committed from native popup content, and popup cancellation/stale-callback
coverage. SDL and Haiku inspect presented pixels for all sixteen button
border masks. GEM compares VDI text to the stock-font image raster so text
and toolbar images share their vertical origin. OPEN LOOK combo positions
and command-button menu marks, and WINGs text centering, are visually
inspected in the Properties and toolbars gallery. Windows and Haiku popup
commits use the production posted-work dispatch path. GEM’s direct and gemd
input suites also open a custom canvas popup and
commit an arbitrary value through real Rasta/AES input. Custom dropdown
runtime coverage on macOS is not included in this pass.

Verification of that sizing/dropdown pass built all nine Docker targets without
compiler warnings and passed all 55 Linux test executions. The native
Windows 11 and Haiku VM inspector checks cover the final popup commit/Cancel
lifecycle and rejection of callbacks retained from a previous popup session.

The 2026-09-07 popup repair also verifies titleless GEMix canvas hosts: all
four `1011` frame edges remain intact, with the requested client size and a
separate inset paint/input origin. X11/Athena now wakes and drains posted
work in the production Xt loop; its inspector test no longer supplies a
queue-draining timer. Server-delivered clicks on native buttons and painted
colour swatches both commit values and close the dropdown. The X11, direct
GEMix, and gemd Docker builds pass without warnings, and all 22 test executions
across those three builds pass (8 + 7 + 7).

The Motif border follow-up verifies all sixteen preview masks through actual
property-grid checkbox clicks and screen pixels. A separate native-widget
pixel test varies focus margins and default-button reservations through all
sixteen masks. The Motif Docker build has no compiler warnings and all eight
tests pass. Hidden edges include the complete inset native relief; restoring
an edge restores its original Motif appearance.

Haiku's keyboard follow-up sends test keystrokes through the window's preferred
target, exercising native focus routing. Real VM keystrokes also edit property
text and update the preview after adding a USB keyboard to the `Haiku` domain.
The VM's PS/2 keyboard had delivered no input even to Terminal; the editor
implementation required no change. The keyboard setup is recorded in the
[Haiku runtime note](../../notes/HAIKU-REMOTE-RUNTIME.md).

The full-monitor follow-up checks actual shader pixels for the bezel, glass
recess, reflection, green power indicator and stable housing under screen
curvature changes. Its new visual presentation has SDL2 acceptance coverage;
the preceding native Windows/Haiku evidence refers to the earlier four-pass
terminal image. These checks establish executor behavior only; the presentation was rejected.


### Linux GLSL context handoff (2026-10-07)

The EGL adapter preserves toolkit EGL/GLX context ownership. An accelerated
SDL/X11 window previously caused `eglMakeCurrent` to return `EGL_BAD_ACCESS`
while a GLX context remained current. Native now releases and restores that
context around image passes. `native_glsl_context_tests` covers deterministic
handoff, repeated activation, error rollback and teardown without a GPU.
A desktop run on Intel Iris Xe reports active original GLSL shaders with the
ordinary accelerated SDL renderer; the shader package and stock fonts are
unchanged. This evidence applies to Linux context coexistence, not other OS
adapters.

Native's Debug SDL2 Docker build passes all 19 CTests, including the new
context fixture tests. Its Release build passes the desktop terminal smoke test
with the normal accelerated SDL renderer. The rebuilt consumer also reports
active shaders under GDB without graphics environment overrides. Debug desktop
execution requires the build image's matching sanitizer runtime; the Release
binary has no sanitizer runtime dependency.


### Shader execution status (2026-10-08)

Shader peers start pending; source-less effect installation
also remains pending. Active requires a successful output.
The shared mouse/shader regression checks status after
creation and source-less replacement before its first paint.
This contract applies independently of fallback pixels and
prevents consumers from reporting premature shader success.

The SDL2 Docker Release suite passes all nineteen CTests with this status
correction. The mouse/shader regression also passes in the Docker Debug
ASan/UBSan build. Infinity's consumer UI gate passes normal, Mesa-override
and explicit unavailable-EGL runs, including no automatic retry and continued
input/audio behavior. The Release archive retains SDL2_ttf and stock fonts.


### Cross-platform shader handoff review (2026-10-14)

The pending-status, bounded latest delivery and shader pacing implementations
are common to all backends. Linux toolkits share the EGL/GLX handoff adapter.
WGL/CGL now reject re-entry, check restoration and restore entered contexts at
teardown; the shared renderer avoids GL cleanup in a caller context after a
failed restoration. Incidental exposes do not retry failed continuous effects;
a source/property update permits retry. At this 2026-10-14 checkpoint Haiku
had an unavailable GLSL factory and CPU-profile execution; the later OSMesa
implementation below supersedes that missing adapter. Control kind marks remain unchanged.

`native_desktop_glsl_context_tests` checks real WGL/CGL handoff and teardown;
unavailable desktop OpenGL is a skip. This test does not establish original
CRT pipeline acceptance or measured frame performance on those desktops.

Verification for this review: 29 focused CTests pass across SDL2, X11,
OpenMotif, OPEN LOOK, Window Maker, GEMix and GEMix/gemd. Each runs the
mouse/shader, terminal-screen, UI-delivery and deterministic EGL/GLX context
regressions; SDL2 also checks renderer presentation startup. The SDL2 CTest
uses software rendering and does not verify accelerated refresh behavior.
Windows and Haiku cross-compilation pass, including the new WGL test
executable and Haiku's explicit unavailable GLSL adapter. These checks do not
claim Windows or Haiku desktop execution.
The macOS Debug build on `leia` passes after selecting the SDK's
`kCGLOGLPVersion_GL4_Core` profile constant. All four focused CTests pass:
UI delivery, mouse/shader lifecycle, terminal-screen pixels and real CGL
context preservation (no skip). The original GLSL CRT terminal smoke also
exits successfully after active output and its host-shell command complete.
This is functional execution evidence, not a visual/performance acceptance
claim or a full macOS suite run.

### Haiku GLSL execution (2026-10-15)

Haiku now loads optional OSMesa dynamically, requests OpenGL 3.3 core and
composes its software-rendered GLSL output through the existing canvas.
All 18 deployed regression executables pass without skips under
GALLIUM_DRIVER=softpipe, including real history/reset/resize/failure pixels,
transactional live installation and the new native context test. Context tests
cover re-entry, binding order, viewport preservation, entered/retired teardown
and rejection of a foreign Mesa context without modifying its binding.
Foreign GL clients must release their current context before Native activation.

The configured Mesa 22.0.5 llvmpipe crashes inside its vertex JIT. The adapter
rejects that known combination before drawing, with a diagnostic naming the
supported softpipe launch setting. The library does not mutate the process
renderer selection. The Haiku GLSL CTests and F5 profile supply the setting;
there is no GPU acceleration or passthrough requirement for this software path.
The original eleven-pass CRT/blur terminal smoke exits zero with active shader
output and completed shell capture. A visible demo was left open and the user
confirmed it works. This does not establish exact upstream pixel parity or a
performance release gate. The remote runtime note records reproducible commands.
