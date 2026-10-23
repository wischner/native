# Feature Matrix

The 2026-10-21 macOS VM (`mike`) Debug build succeeds with AppleClang 17
and all 19 CTests pass. CMake enables libc++ experimental threading support
when the Apple SDK requires it. LLDB authorization is enabled; interactive breakpoint stops and automatic
session shutdown after inferior exit have been verified on the VM.

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


## Input height and footer ordering (2026-10-22)

All maintained backends compile the shared geometry and footer changes.
Single-line entries, both combo styles and date/time pickers resolve to the
same theme height, with centered text. Taller bounds allocations center the
field; repeated dimension-only changes preserve position. Multiline geometry
is unchanged. Status bars reserve the outermost bottom strip before bottom
toolbars regardless of construction order and reject other docking edges.

Haiku uses one BControlLook dropdown indicator for both combo styles and
calendar triggers, frames its navigation search, and uses a flat panel footer
with a top rule and internal dividers. The date field keeps its native
preferred width instead of stretching its segments across a wide allocation.
Date and editable-combo markers share their editor's paper and single frame,
without inset menu backgrounds. Date separator spacing follows the glyph
width. Narrow arrow buttons center their native glyphs, retain native pressed
feedback while open, and close the popup on a second click. The native
calendar opens immediately below the field and is never narrower; it grows as needed for readable day columns,
with small, equal square month buttons. Materialized tables preserve rows,
Shift anchors and native selection during grid/stripe changes; a plain
click replaces multi-selection afterward.

| Backend | Final validation for this change |
| --- | --- |
| SDL2 | 27/27 dummy/software CTests, including queued control input and renderer cache checks |
| GTK Wayland / elementary | 20/20 CTests each under Weston |
| Haiku | 20/20 native commands on `hotel`; desktop pointer checks verify pressed feedback and second-click dismissal for calendar and both combo styles; visible search/footer inspection; tested Vision deployed to normal run location with matching SHA-256 |
| macOS | 19/19 native CTests on `mike`, including light/dark picker text and accessory pixels |
| Windows | Four focused interactive runs pass on `whiskey`; native text client/caret centering and inspector input/pixels; tested Vision deployed to normal F5 location |
| X11/Athena | 21/21 Xvfb CTests; native text margins leave room for a complete centered line |
| OpenMotif | 24/24 Xvfb CTests; CDE separately passes 23/24, with a table grid-pixel assertion remaining |
| OPEN LOOK | 24/24 olwm CTests; windowless native input items remain visible above canvas copies, including deferred teardown under ASan/UBSan |
| Window Maker | 20/21 CTests; the existing repaint fixture still reports Fontconfig/Pango exit allocations |
| GEMix direct / gemd | 19/20 and 20/21 CTests; input regressions pass with four-row viewports sized from actual field heights; the SDK menu restoration assertion remains |

The new footer/input assertions run through the existing window, settings,
inspector and collection executables. No additional application API for
native handles or platform dimensions is introduced.

## Settings navigation and civil pickers (2026-10-21)

`settings_sidebar`, `date_picker` and `time_picker` are implemented in every
maintained selection. Sidebar builders support stable IDs, sections,
enabled pages, optional shared images and optional search over titles/sections.
Search keeps the active page open and emits only user edits. Date rules combine ranges,
explicit holiday lists, excluded weekdays and application predicates; time
rules combine same-day limits, minute steps and predicates, with optional
seconds. Setters are silent; unavailable user proposals restore the cached
value. Rules can invalidate a cached value without coercing it.

| Backend | Adapter | Current validation |
| --- | --- | --- |
| elementary | GraniteSettingsSidebar, GraniteDatePicker, GraniteTimePicker plus optional GTK seconds | Full headless Weston suite 20/20; native callbacks and page recreation |
| GTK Wayland | GtkListBox navigation, linked native entry/arrow fields with calendar or time-spin popovers | Full headless Weston suite 20/20 |
| Windows | Real WinUI 3 NavigationView, AutoSuggestBox and ImageIcon rows; Win32 date/time common controls | Pinned Docker SDK build; four interactive control/inspector/settings runs pass on `whiskey`; physical navigation, search and normal close verified |
| macOS | Source-list NSOutlineView, sidebar material, optional NSSearchField; compact NSDatePicker and native calendar-button popover | Remote AppleClang Debug build; full suite 19/19 on `mike`; production Dates/Time pages inspected offscreen |
| Haiku | Framed, inset BOutlineListView; standard-height shared-kit DateEdit/TimeEdit, internal calendar trigger and native calendar popup | Docker cross-build; 20 native checks pass on `hotel`, including all three file-panel centers, picker geometry, toolbar icon sizes and status-edge pixels |
| X11/Athena | Native Label/Toggle navigator; editable-combo date field with native-button calendar, and time field with native step buttons | Full Xvfb suite 21/21 |
| OpenMotif | XmContainer and native entry/choice fields | Full Xvfb suite 24/24 |
| OPEN LOOK | Indented native PANEL_LIST with labeled search; editable-combo date calendar and joined time editor with paired buttons | Full Xvfb/olwm suite 24/24; native input and repaint coverage recorded below |
| Window Maker | WMList and native entry/choice fields | New settings tests pass; full suite 20/21, existing repaint fixture reports Fontconfig/Pango allocations at exit |
| SDL2 | Framed themed tree/search; editable-combo date calendar and joined time editor with step arrows | Full dummy/software suite 27/27, including queued SDL control input and frame pixels |
| GEMix direct / gemd | Existing themed tree, entry and choices | New settings tests pass; full suites 19/20 and 20/21, respectively; recorded SDK menu-strip restoration failure remains |

Optional search is exercised in the current Mac, GTK Wayland, elementary and
SDL suites and with physical keyboard input in the Windows VM. The other backend rows record their existing picker/page checks;
native keyboard search interaction was not rerun on those hosts for this change.

`native_settings_model_tests` checks search filtering, retained selection, civil dates, holiday/weekend predicates,
time constraints, signal semantics and stable page identity.
`native_settings_runtime_tests` checks native callbacks on GTK, Granite,
Win32, AppKit and Haiku; it checks the shared acceptance hook on composed
backends. Windows fixture notifications explicitly deliver the same WM_NOTIFY
payload as a user edit, since a programmatic setter need not produce it.
Runtime checks also cover rejected proposals, invalidated cached values,
seconds, page teardown/recreation, clear/re-add and full-range creation.
These assertions are separate from a complete physical-input or visual
walkthrough of every backend. Vision exposes `--settings` for that walkthrough.

Mac stock edge toolbars use flexible-height native flat NSButton controls and NSBox
separators; toolbar primitives use the same native flexible-button appearance.
Workspace file icons use Quartz rasterization with straight-RGBA conversion.
Rounded buttons retain AppKit painting when edge masks change;
borderless read-only help uses transparent native text views. Mac presentation
regressions cover native tool actions/disabled state, icon pixels, native calendar-popover activation and rejection, border-mask
renderer continuity and visible removal of native inset edges for all 16 masks,
sidebar source-list/search widgets and compact pickers.

Mac retained-window regressions also render old AppKit content views after
resource recreation and C++ destruction. Weak resource-generation checks
prevent late paints and delegate/input callbacks from reaching dead owners.


The elementary spacing correction enables the native theme's sidebar row
padding and restores missing icon/title column spacing. The full 20-test
Docker suite passes with elementary's installed stylesheet; selection and
resource recreation still use Granite's widgets. GraniteDatePicker and
GraniteTimePicker remain the standard controls. Optional seconds use an
additional GtkSpinButton because GraniteTimePicker edits hours and minutes.

The GTK image/resize correction displays supplied navigator images in native
GtkImage rows. Its runtime regression checks the real texture pixels and
completes a compositor-backed grow/shrink cycle in both dimensions with a
client-filling sidebar. Fixed overlay children are clipped and excluded from
minimum-size measurement on both GTK and elementary. Full suites remain
20/20 in each selection, including native bottom-row geometry and hit testing.
Restoring the old measured-host setting inside GDB makes the resize fixture
fail, confirming the regression detects the original height lock. The actual
generic GTK Vision gallery was also inspected after 760×477 → 900×800 →
760×477 resizing; its supplied icons are visible.

Native calendar APIs do not universally offer individually disabled day
painting. GTK marks unavailable days; every adapter rejects unavailable
commits, and composed day choices omit them. Native Athena rows and XView/WINGs lists display text only, retaining optional images in the portable
model. Haiku suppresses obsolete row painting and portable latch lookup while
BOutlineListView synchronously removes selected rows during model replacement.

## Windows editor cursors (2026-10-20)

The Windows native-peer regression covers single-line and multiline editor
I-beam selection after root-to-child focus transfer, explicit crosshair
overrides, inactive-root arrow selection and reactivation. The original
focus-transfer assertion reproduced the arrow defect on `whiskey`. The repair
separates top-level activation from stock-child keyboard focus and resumes
native-child hover on forwarded cursor messages. It retains native `EDIT`
controls and their non-client cursor behavior. The Docker build and updated
interactive native-peer fixture pass on `whiskey`; Vision is redeployed.

## GTK Wayland and elementary OS (2026-10-20)

Both toolkit selections build with GCC, warnings and Debug ASan/UBSan in the
GTK 4.14.5/Granite 7.4 Docker SDK. All 18 tests pass for each toolkit under
headless Weston/Pixman. The latest elementary OS 8.1 VM run also passes 18/18
(`ssh echo`, GTK 4.14.5, Granite 7.8.1). The later generic GTK VM run passed
15/18 before three GUI initialization timeouts. All three affected fixtures
(close, widget/theme and collection) pass on subsequent focused reruns after
the compositor recovered. Both VM toolkits pass 96 native-theme pixel comparisons.
GDB located the earlier wait inside `gtk_init_check()`; Gala logged recursive
main-context warnings. Earlier full generic VM runs also passed;
see the [VM runtime note](../../notes/ELEMENTARY-VM-RUNTIME.md) for scoped evidence. The focused regression
asserts GTK/Granite native control types, silent setters, live validation,
native column actions, container lifetimes and reparenting,
independent modeless stacking without GTK transience (modal transience retained),
canonical source updates, lazy million-row tables and destroy/recreate behavior.
The production GLSL retro-terminal smoke verifies Cairo image lifetimes and
posted completion on this path. LeakSanitizer's exit scan is disabled in the
runtime session because GTK/desktop libraries retain process resources.

The shared GTK adapter clips the non-measured fixed child overlay, preserves child
pointer targeting, and routes background controllers at the target phase.
Top-level defaults include header/menu height. Native menus translate portable
mnemonics to GTK underscores while escaping literal underscores. The focused
regression checks allocated client height, bottom-button hit testing and
native action delivery, plus top-level/item mnemonic labels. Elementary's
only F5 profile is **Elementary OS, VM**; generic GTK retains its Docker profile.

The GTK close regression also covers a parent closed synchronously by a child
input-reset callback, and parent destruction during native unmapping. ASan
reproduced a freed child widget before the fix. Peers now hold their own GTK
widget reference and retain the native parent during removal; the regression
asserts that the child reference is released and closes a populated window
through GTK with an editor focused. Both 18-test Docker suites pass; 25
repeated elementary close checks pass under sanitizers, and 10 repeated checks
pass on `echo` with fatal GTK criticals.

Custom theme primitives now use actual GTK/Granite widget snapshots and native
CSS-part functions, not palette-based emulation. Window primitives draw GTK
vector nodes directly through their frame's device transform, offset and clip;
image targets rasterize the same nodes. The fixture passes 96 exact pixel
comparisons against independently captured live controls: 84 image cases for
buttons, checks, radios, entries and lists across light/dark and normal, hover,
press, disabled, focus and inactive states (including unchecked indicators),
plus 12 controlled 2x window cases with offsets/clips. These pass in both
Docker toolkits and on the elementary VM. Primitives express steady state;
intermediate CSS animations and physical HiDPI monitors are not qualified by
these controlled comparisons.

| Area | Current implementation and limits |
| --- | --- |
| Desktop integration | Native GTK 4.14 widgets; elementary adds Granite validated entries, message dialogs and live color preference when the system service is reachable. |
| Display | GDK Wayland only; separate generic and elementary F5/build selections. Top-level placement remains compositor-owned; advertised monitor geometry is also used as work area. |
| Drawing | Frame-owned Cairo, native GTK vector/CSS theme parts, Pango text, and portable TrueType/image rendering. Native theme snapshots also serve image targets. EGL image shaders omit GLX discovery. |
| Collections | Native GTK list/grid/column/outline composition; icon and table item objects are lazy. Stable portable data/selection remain library-owned. |
| Source editor | GtkSourceView 5 with UTF-8 canonical-document and caret/selection synchronization. Native language highlighting is used for stock editing. Application lexers, markers, diagnostics, style runs, completion and derived drawing select the shared Cairo editor path. |
| Advanced tables | Native columns, virtual row binding, selection and activation. Sort, resize and reorder events are forwarded to the portable model. Native header menus implement column visibility. Grouped, alternating, headerless and derived tables use the shared Cairo model painter to retain staged drawing; interactive column operations remain unqualified. |
| Precision pointer | Hidden cursors and ordinary GTK gestures; explicit capture, relative motion and raw/unaccelerated delivery are unavailable. |
| Desktop acceptance | The VM control snapshot and automated hit testing are verified. Physical mouse/keyboard input, accessibility, cross-application clipboard exchange and interactive file portals remain manual acceptance limits. |

The X11-specific inspector-input fixture is not built for these selections;
its shared model tests and the dedicated Wayland widget regression remain built.
The EGL fixture covers EGL preservation/failure rollback; GLX-only scenarios
remain covered by existing X-capable builds.

## Window Maker collection repaint (2026-10-18)

Collection invalidations now coalesce into a deferred buffered repaint,
bypassing WINGs' synchronous frame redisplay that briefly repainted the native
border before the collection image. The new icon/tree regression verifies
one paint per invalidation burst and safe destruction/recreation. The Docker
Debug build and all 19 Window Maker regressions pass (18 existing tests plus
the focused repaint test). The rebuilt gallery also starts in Docker Xephyr.

## Window Maker Docker runtime (2026-10-18)

The **Linux Window Maker, Xephyr** F5 profile builds and runs in
`wischner/gcc-x86_64-linux-window-maker:latest` through the image's
`window-maker-xephyr` helper. The Docker Debug build passes, all 18 CTests
pass in that Xephyr/Window Maker session, and Vision creates its gallery
window. The GDB pipe produces clean MI output. The standalone launcher
retains its existing `ASAN_OPTIONS=detect_leaks=0` setting; this verification
does not establish new leak-checking coverage. Earlier Bookworm guest
results remain historical evidence.

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

The 2026-10-21 GEMix correction accounts for the pinned hosted VDI's reversed
monochrome index mapping and disables inversion for the entire 32-bit Rasta
session. Absolute screenshot checks cover white paper and black borders in
AES menus, title bars, window edges and Native's client. The framebuffer
reader handles packed monochrome and RGBA output without calibrating away
incorrect colors. The real viewer-close regression runs Vision through live
GDB/MI, keeps MI stdin open while closing Rasta, and verifies termination of
the owned debugger/server/inferior session. Command exit status is retained
and viewer/server logs stay off MI stdout. Full suites pass 17/18 direct and
18/19 proxy tests; the final menu-strip restoration assertion still fails
on the pinned SDK, which leaves a blank AES menu strip after detachment.

The 2026-10-21 macOS VM regressions cover text-field I-beams before editing,
split-divider hover cursors, the composed Properties and Toolbars dialog,
ruler marker updates from ordinary window motion, and button/check/radio
theme pixels against live AppKit drawing in light/dark
appearances with selected and disabled states. Child surface clipping keeps
panel backgrounds from covering neighboring toolbar icons and labels.

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
dithered canvas images with unchanged source pixels and all four copied alert bitmaps. X11 collections
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
| Linux Window Maker/WINGs | `docker-wmaker` + Docker Xephyr/Window Maker | Yes (tested) |
| Linux GEMix | `docker-gemix` + local rasta | Yes (tested) |
| Linux GEMix via gemd | `docker-gemix-gemd` + local rasta | Yes (tested) |
| MS Windows (MinGW) | `docker-win` + native Windows 11 VM | Yes (tested) |
| Haiku | `docker-haiku` + SSH deploy/run | Yes (tested) |
| Apple | remote build and runtime checks | Yes (tested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | Linux OPEN LOOK | Linux Window Maker | Linux GEMix | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Per-edge outer border flags | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | WIP |
| Custom property dropdowns and value converters | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Compact typed property grid | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Four-edge toolbars, sticky groups, 16/24/32px icons | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | WIP |
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
| Themed check/radio drawing | Yes (build tested) | Yes (tested) | Yes (build tested) | OLGX (tested) | WINGs (tested) | Yes (tested) | Yes (build tested) | Yes (build tested) | Yes (tested) | WIP |
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
  uses its native Xaw browser with parent-centered modal placement. SDL2 consistently
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
- Motif trees use a native `XmFrame` around a shadowless automatic scroller
  and retained outline container. CDE/Xlib tracing identified whole-scroller
  clears during native disclosure layout; the separate frame isolates the
  border from those clears. Library invalidations target the inner outline.
  The 2026-10-19 native disclosure-activation regression passes in CDE and
  checks all frame exposes, unchanged border pixels, selection repair, retained
  gadgets, explicit border changes and scrolling. The isolated command is
  `native_motif_runtime_tests --tree-repaint`. All 22 CTests pass under Xvfb;
  the full CDE run hit a table-grid pixel assertion and an inspector artifact
  write-permission failure, so it is not a full-suite CDE pass. Real CDE mouse
  clicks were traced through first collapse, expand and repeated collapse;
  the outer frame received no Xlib clear requests and settled screenshots
  showed clean collapsed rows.
- OpenMotif invalidations merge into a peer-owned deferred Xt expose without
  clearing the visible frame first. Accordion refresh no longer requests the
  same repaint twice; split refresh skips identical pane bounds and repeated
  shows of visible panes. The 2026-10-19 Docker/Xvfb run passed all 22
  Motif CTests, including repaint batching, retained pixels, cancellation on
  teardown, recreation, and pane show/geometry counts. Visual CDE flicker
  acceptance remains unverified.
- OpenMotif shell sizing reserves the negotiated menu area when showing or
  programmatically resizing a window, preserving the requested work dimensions.
  The 2026-10-19 Docker/Xvfb Motif regression checks initial menu-bearing
  work size and both dimension and bounds setters; CDE visual acceptance
  remains separate.
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

## Haiku, Windows and elementary control review (2026-10-21)

Stock edge toolbars use native BToolBar on Haiku and the flat toolbar common
control on Windows; their theme primitives use matching native parts.
Accordion headers reuse table-group surfaces without a second beveled separator.
Property editors are native children of a canvas value host, with light dotted
lines across keys and values and rows sized for the native editor height.
Windows button masks preserve native painting; iconless tree rows use a real
transparent image mask; section headings own their page children, so native
indentation remains visible when connector lines are hidden.
Windows native status parts omit competing inset top borders.

GTK clipboard reads remain asynchronous while dispatching their producer;
copy-field/paste now has a native multiline regression. GTK table input keeps
Ctrl toggling and Shift ranges; completion accepts Tab and focuses its keyboard
host. Splitters start with a wide native handle and suppress recursive position
feedback and teardown notifications. GTK theme probes are cached per painter,
and table header backgrounds do not include a second native sort glyph.
Editable GTK combos use the combined native GtkComboBoxText; selection-only
combos retain GtkDropDown. Tabs remain native GtkNotebook controls with the
desktop stylesheet. Granite uses centered, 24-logical-pixel native sidebar images,
a full-width native search entry, initialized editable dates, and native
insensitive styling on unavailable calendar labels.

Native picker fields keep their preferred dimensions and center vertically.
Haiku uses a native calendar popup with month navigation. Modal show sessions
center over the owner; Win32 common dialogs center on activation, and Haiku
alerts center their final native frame. The 2026-10-22 Haiku follow-up below
corrects file-panel placement after native Show. On Wayland, the
compositor places owner-modal transient windows.


Verification: GTK Wayland and elementary each pass 20/20 Docker CTests; SDL2
passes 26/26 and macOS passes 19/19. Haiku runs all 19 registered commands plus
the collection runtime check successfully on `hotel`. Windows passes four
focused native/control, inspector and settings runs on `whiskey`. The installed
elementary desktop passes focused GTK/settings/collection checks, including 96
live-control pixel comparisons. Haiku's native calendar also commits a date
through real pointer input. The elementary desktop locked during the final
visible walkthrough; snapshot/callback coverage is recorded separately.
AppKit message alerts attach as native owner sheets, with a regression for
sheet ownership and three-button return mapping.

The affected Motif adapters pass four focused CDE checks (window API, settings,
inspector and collections); Window Maker passes three (window API, settings and
inspector). CDE startup must finish before physical-input tests. The inspector
scrolls its border-property rows into view before native hit testing so native
font/editor heights do not put the tested rows outside its viewport.


## AppKit control refinement (2026-10-22)

Mac toolbar buttons and their theme primitives use a flexible-height native flat
bezel. Native alignment rectangles follow icon dimensions, including vertical
bars. Removing every stock button border disables the native bezel, avoiding
rounded corner remnants; restoring edges restores native painting.

Mac pickers each have one native enclosing field at the standard NSTextField
intrinsic height, using the adaptive native control color. Dates have a text-only
editor and calendar trigger, with no stepper; time retains its native stepper.
Native baseline and image-cell metrics align the painted text and accessories. The date
calendar remains an AppKit popover subject to the same shared availability rules.
Settings rows show 24-logical-pixel icons in 32-pixel page rows. Vision supplies
embedded Font Awesome Free gear/calendar/clock tiles consistently across
backends; source attribution and regeneration live beside the artwork.


The expanded focused AppKit and settings regressions pass on `mike`, including
16/24/32/48-pixel selected toolbar bounds on both axes, borderless corner pixels,
internal calendar-button hit testing, native holiday rejection and selected
navigation artwork in both appearances. The shared embedded artwork also
builds through the SDL2, elementary, Windows and Haiku Docker targets. Its
regenerator reproduces the checked-in PNG and embedded-byte hashes exactly.

The complete 2026-10-22 macOS suite passes all 19 CTests after these changes.
The GCC/Docker SDL2, elementary, Windows and Haiku builds are warning-free;
`git diff --check` also passes.


The picker sizing follow-up removes the macOS date stepper and keeps its
calendar trigger. Both fields use standard NSTextField intrinsic height,
vertically centered in their requested bounds. The date text and calendar
button, and the time text and compact native stepper, are centered inside that
field. AppKit's adaptive control color supplies the background in both
appearances; no fixed RGB palette or custom stepper drawing is introduced.

After this picker-height/date-stepper change, all 19 macOS CTests pass on
`mike`. The native fixture checks the preferred NSTextField height, centered
editor/accessory alignment, date text-only mode, compact time stepper, native
control background and calendar hit testing/holiday rollback. Light and dark
AppKit captures of the production Dates and Time pages were also inspected.


## Haiku control layout (2026-10-22)

Open, Save and Select Folder panels center after BFilePanel::Show, which
otherwise replaces the earlier owner placement. Native BToolBar minimum sizes
reserve complete buttons for changed icon sizes and orientation; native layout
is applied before input resumes. Docked status chrome retains its top rule
and internal separators without outer left/right dividers.

Date and time fields include accessory space and complete locale segments in
their preferred widths, match BTextControl height and center text by font
metrics. Time arrows have equal geometry and use BControlLook drawing. The
date field replaces its arrows with a native flat calendar trigger inside the
text frame; its BCalendarView popup retains the existing availability policy.
Single-line BTextView controls use their actual line height for centering.
Settings navigation has equal margins around its native framed outline list.

Verification: the warning-free Docker Haiku build passes all 19 registered
commands plus the collection-runtime check on `hotel` (20/20). Settings runtime
assertions cover all three native file-panel centers, picker height/width and
the internal calendar button. Inspector checks cover native buttons at 16, 24
and 32 pixels and presented status-edge pixels. A real desktop pointer check
opens the integrated calendar, selects October 16 and confirms field/status
updates and popup dismissal. Dates, Time, navigation margins and the toolbar
window were inspected in desktop captures. Shared GCC/SDL2 checks pass 26/26;
the inspector check was rerun after clearing old screenshot-file permissions.


## AppKit picker text and accessory alignment (2026-10-22)

Date/time editors use their final native control height and AppKit baseline
metrics to align with a standard NSTextField. The native calendar image cell
keeps its symbol padding outside the bottom bezel inset, so the painted symbol
centers with the field. The native time stepper's alignment edge meets the
field's right edge without the former four-point gutter. The enclosing field
clips native decoration to its bounds. Both controls retain their **H** kinds,
NSDatePicker editing and calendar/availability behavior.

The warning-free remote AppleClang build passes all 19 CTests on `mike`.
Rendered regressions compare actual date/time glyph rows with a standard
NSTextField containing the same locale-formatted value and check the painted
calendar symbol/time arrows in Aqua and Dark Aqua. The native fixture checks
the stepper's right edge, hit testing, calendar activation and holiday rollback.
Production Dates/Time captures in both appearances were inspected. GCC/Docker
SDL2 builds and both settings model/runtime tests also pass.

## Windows and elementary control review (2026-10-22)

Windows now hosts the actual WinUI 3 NavigationView, native search and image
rows through Windows App SDK 1.8. The reproducible GCC/WinUI Docker build
passes; four focused native desktop runs pass on `whiskey` (Windows controls,
inspector, settings model and settings runtime). Physical mouse selection,
keyboard search, page switching and normal application closure are verified;
closure returns zero and leaves no Vision process. Native date/time values
include text padding. Accordion headers omit their extra focus outline.

Both GTK suites pass 20/20, including the real elementary stylesheet. Tests
cover Ctrl/Shift multiselection, group collapse through a complete press and
release, editable-combo caret preservation, fifty repeated split allocations
and orientation changes, pending splitter delivery during destruction, and all
sixteen native-button edge masks. GtkPaned has a real allocated drag strip;
its portable layout callback runs after native allocation. GtkNotebook uses
native theme painting with padded labels. Property-grid right frames sit
outside scrollbar painting. The shared SDL suite passes 26/26.

The deployed elementary desktop also confirms physical Ctrl/Shift selection,
persistent group collapse and repeated splitter dragging. Live typing preserves `hello` in order; native tab labels,
individual preview edges, the fully borderless preview and the property
grid's right frame were also inspected. Test screenshots
are development artifacts under `build/elementary-control-review-*.png`.

## GTK4 and X11 control review (2026-10-22)

Generic GTK Wayland passes 20/20 CTests under headless Weston. Its native
runtime fixture also exits normally on the host Wayland desktop, including
120 native-theme pixel comparisons. Native split close and retained-widget
callbacks, equal-height tree rows, editable-combo synchronization and linked
entry/arrow date and time popovers have regression coverage. Icon selection
leaves scrolling to GTK; toolbar highlights use native button state colors.
The gallery file/dialog buttons have room for their native minimum widths.

X11 passes 21/21 CTests under both Xvfb and monochrome TWM. Coverage includes
Ctrl/Shift table selection, editable text replacement and Enter commits,
window grow/shrink, all three centered native file panels, modal server grabs,
calendar selection and disabled dates, full-width native navigator rows,
monochrome image presentation, and scrollbar thumb/page input. Calendar
painting was inspected under TWM. Source image pixels retain their colors.
These checks cover generic GTK and X11; elementary was not retested in this review.

## SDL2 control review (2026-10-22)

The SDL2 Docker suite passes 27/27 tests. The new control fixture sends SDL
mouse, text and key events through the application loop, covering Ctrl/Shift
table selection, editable combo replacement and popup synchronization,
completion keyboard/mouse acceptance, child lists inside tabs, date exclusions
and popup toggling, and joined time editing/stepping. It compares actual list
pixels with the theme primitive, checks navigation margins and borders, and
verifies that repeated table frames reuse bounded renderer text textures.

SDL date and time fields now extend the existing editable combo rather than
using separate year/month/day or hour/minute choices. The calendar uses the
same theme and disables unavailable dates; the time field has paired arrows
in one button column. Search includes a magnifier, framed navigation has
six-pixel margins and half the former tree indentation, and read-only Vision
help/status fields retain standard borders. Toolbar strip edge rules follow
their horizontal or vertical orientation. Frame and calendar snapshots were
inspected with the software renderer. The same queued-input fixture also
passes through the normal Docker/GDB desktop path with SDL's X11 video driver
and exits normally. This is automated desktop coverage, not a physical mouse
walkthrough.

## OPEN LOOK control review (2026-10-22)

The Open Look Docker build and all 24 Xvfb/olwm CTests pass. The native runtime
fixture sends XTest mouse and keyboard events through XView, covering Ctrl/Shift
table selection, source-editor caret movement, completion arrows/Tab/mouse/Escape,
and editable-combo typing followed by native popup selection synchronization.
Native calendar opening, repeated-arrow dismissal, month navigation, disabled
dates and minute-step buttons are checked, together with visible arrow bitmaps
and non-overlapping time-button geometry. Search insertion/deletion and
restored nested-page frame coordinates are also checked. The native navigation
scrollbar stays inside the sidebar margins, with whole rows fitted using the
actual XView row height.

Portable background updates now merge damage and copy only the affected area,
excluding live windowless Panel items. This prevents editor/selection erasure
while rulers track the pointer. Full off-screen OLGX scene rendering remains;
native XView clear/expose callbacks still restore the background and affected
items. A pixel sentinel verifies that an application redraw preserves the native
editor area. The calendar and closed-control screenshots were inspected on the
composed olwm desktop. Navigation uses native indented list rows, bold inactive
section headings and a labeled Search field. Dates use DD.MM.YYYY; times use
HH:MM with optional seconds. Both fields extend the existing native editable
combo and apply shared availability rules. The same fixture exits normally
through the maintained Docker/GDB Xephyr/olwm launcher; this is automated
desktop input coverage, not a physical mouse walkthrough.

## Window Maker, GEM and CDE control review (2026-10-22)

The three adapters now extend their editable combo for joined dotted date
entry and colon time entry. Native WINGs/Motif buttons form the calendar;
GEM uses the same themed button/editor paths as its ordinary controls.
Availability rules cover both typing and calendar selection, including
excluded holidays. Calendar arrows toggle the popup, and reopening recreates
native children safely. Settings retain page images and indented sections,
with a labeled native search field, equal outer margins and matching paper.
Window Maker read-only help no longer requests a scrollbar; plain multiline
text uses the same native default font as single-line fields. Its theme list
includes the native scrollbar gutter/stipple and selected toolbar buttons
use pressed relief. Motif editors/tabs use the measured control font, icons
wrap within the viewport and selected icons scroll into view. Its custom
property dropdown trigger uses the existing native combo. These font changes
require no replacement CDE Docker image.

GEM editable combos contain real text editors, synchronize typed matches
and track popup hover. Tab children receive their own clicks; hidden ancestor
pages do not paint through the active page. Table modifiers use the shared
selection path, and WINGs/Motif completion takes real native focus and handles
arrows, Enter, Escape and pointer selection. GEM image presentation uses two
monochrome VDI transfers instead of many individual pixel runs; dirty drawing
is culled. Its disclosure triangles, resize cursors, oriented toolbar edges,
dotted property-grid rules and compact navigator follow its existing theme.
The native AES footer/scrollbar order remains unchanged.

WINGs and Motif standard panels center over their owner using desktop client
coordinates after native decoration placement. GEM file/save/folder/message
panels are positioned AES forms, constrained to the desktop, with filesystem
browsing, filters, typed paths, extension handling and overwrite confirmation.
No SDK extension, pin or runtime-image change is needed.

Validation: Docker builds succeed for WINGs, Motif, GEM direct and GEM/gemd.
Motif passes 25/25 CTests. The classic interaction fixture also exits normally
through the maintained Window Maker and CDE Docker/GDB desktop launchers,
including centered open/save/folder/message panels and inspected picker/list/
navigation snapshots. CDE must finish its startup grabs before synthetic
input begins. GEM direct passes 20/21 tests and gemd passes 21/22; their sole
failure remains the existing SDK menu-strip restoration assertion. Real Rasta
packet tests accept and cancel all composed form types and restore owner input.
WINGs passes 20/22 CTests: the repaint fixture and new classic fixture report
Fontconfig/Pango exit allocations; all classic functional assertions pass.
Leak detection remains enabled there. The standalone WINGs-only font test
independently reproduces measurement retention as well as factory retention;
its exceptions match WMWidthOfString/WMCreateFont rather than all Fontconfig
or Native allocations.

The shared private-combo hook also compiles on SDL2; its existing inspector
and SDL control fixtures pass, covering adapters that retain the original
painted property trigger.

## Motif disclosure arrows (2026-10-23)

Accordion headers, table grouping bands and the native tree use the same
13-pixel flat disclosure triangle and native Xlib fill routine. Arrows remain
black when a header is selected. The Motif runtime fixture compares both
collapsed and expanded selected-header arrow masks with the tree pixmaps.
The Docker GCC build and all 25 Motif CTests pass; the same pixel comparison
also passes in the maintained CDE desktop session.

## Motif/CDE picker popup and time buttons (2026-10-23)

Date/time fields now own an XmTextField and their XmArrowButton children
inside one XmForm. The date trigger opens the populated native-button
calendar; there is no underlying empty XmComboBox dropdown. Form attachments
keep the buttons visible and inside the field after resizing or raising it.
Time buttons use the highlighted component or caret: hours change by one,
minutes use the configured step and visible seconds change by one. The
native text selection survives model refresh so repeated clicks retain the
same component. Typed text is committed before stepping, and normal range,
step and validator checks still apply.

Private composition callbacks are copied into the peer before backend
widget creation, allowing the adapter to create the correct native widget
hierarchy directly. Regular Motif combos retain XmComboBox.

The Docker GCC build and all 25 Motif CTests pass. The expanded classic
fixture also passes under the maintained CDE desktop/GDB launcher with real
pointer input: calendar days are visible and selectable, arrows remain
mapped after relayout/raising, and up/down clicks cover selected hours,
minutes and seconds plus caret-only stepping. Calendar and field snapshots
were inspected under both generic Motif and CDE. Shared callback setup also
builds for WINGs, GEM and SDL; their focused picker/property checks pass.


## Application book and generation resources (2026-10-23)

The application book now has 22 chapters and an index covering every concrete
visual control, with aliases and extension bases identified separately.
New complete programs cover owned/modeless/modal windows, collections/accordion,
tables, source completion, split/tab workspaces, choices/standard panels, panel/canvas/ruler/status,
image-effect views, settings/pickers, and toolbar/property inspection.
Chapters 21 and 22 separate settings/availability and toolbar/property lessons
from the general input/chrome chapter.

All 20 complete tutorial programs pass GCC C++20 syntax checks with
`-Wall -Wextra -pedantic -Werror` in the SDL Docker SDK, and all 20 link against
that SDK's existing Native static target and its backend dependency closure.
Five new programs (split/tabs, drawing panel, image effect, settings, inspector)
also pass create/close smoke checks with the SDL dummy driver and
ASan/UBSan; the settings check switches pages repeatedly. The identity shader
package is generated with the maintained packager before its smoke check.
These are startup/lifetime checks, not desktop screenshots or a manual
interaction walkthrough of every example on every backend.

The [documentation checker](../../../scripts/docs/check.py) validates 459 local
links across 79 maintained Markdown files, extracts complete book programs,
and can syntax-check them inside an SDK. Three application-consumption skills
pass the skill frontmatter validator. Provider-neutral agent roles and an AI
request/handoff guide route generators to these same examples and headers;
they do not introduce a second API catalog or install a runner.
