# Backend Open Issues

The 2026-10-21 GEMix correction matches portable drawing to the pinned
hosted SDK's physical palette and disables inversion across the 32-bit Rasta
desktop. Full viewer screenshots verify AES menus/chrome and Native content
together. Closing the viewer terminates the owned GDB/Vision/gemd session
with MI stdin still open; command status and clean MI stdout are verified.
The full suites pass 19/20 direct and 20/21 proxy tests. Their final
menu-strip restoration assertion remains failing: a captured framebuffer
shows that the pinned SDK removes application commands but retains a blank
AES menu strip instead of restoring the desktop checker there. This remains
a runtime qualification limit; the assertion has not been weakened.

The 2026-10-21 macOS VM (`mike`) Debug build succeeds with AppleClang 17
and all 19 CTests pass. CMake enables libc++ experimental threading support
when the Apple SDK requires it. LLDB authorization is enabled; interactive breakpoint stops and automatic
session shutdown after inferior exit have been verified on the VM.

This note lists backend-level open issues that are real today.

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


## Athena input/allocation follow-up (2026-10-23)

The reported blank single-line and search fields are corrected: their Xaw sink
line height and final border-adjusted size now leave a displayed line. Real
copy/paste and glyph-pixel checks pass. Editable popup checkmarks follow exact
typed matches. Native `Search:` labeling, stable typing, six-pixel margins,
table group bottom rules, synchronous pre-map combo sizing, and joined picker
buttons have focused regressions. Date typing and calendar selection share
holiday rejection; time arrows retain the selected component.

Native-owned Form allocation queries no longer propose an enlarged cached size
when asked to shrink. The seven-control nested-layout test reverses resizing
and checks its child geometry. The Docker X11 build succeeds and all 21 CTests pass under both Xvfb and
the maintained monochrome Xephyr/TWM session, whose controls snapshot was
inspected. These checks extend the older
X11 review below without changing other backends.

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

## Windows/elementary control review (2026-10-22)

Windows settings now uses the actual WinUI 3 NavigationView with native
search, hosted through Windows App SDK 1.8. The pinned GCC/WinUI SDK builds,
four focused Windows desktop runs pass, and physical navigation/search and
normal zero-status closure are verified on `whiskey`. Both GTK suites pass
20/20 after table release routing, native combo caret, splitter allocation
and callback, tab spacing and border fixes. The shared SDL suite passes
26/26. These checks do not change the unrelated qualification limits below.

## Settings and pickers (2026-10-21)

The new settings/date/time controls pass their focused tests on all Linux
selections and on native macOS, Windows and Haiku desktops. See the feature
matrix for adapter kinds and exact suite coverage. Native calendars without
a per-day disable API reject unavailable selections; GTK marks excluded days
and applies native insensitive styling to their day labels.
Athena, XView and WINGs native lists show text rather than optional images.
These are explicit native-widget limits, not a claim of identical visuals.

The GTK navigator now presents optional images in native GtkImage rows. The
settings runtime fixture checks texture pixels and a real grow/shrink cycle
in both dimensions. Re-enabling the former measured-child-host behavior only
in GDB makes that resize regression fail; no source or compositor setting is
changed for the negative control. Both corrected Docker suites pass 20/20.
The production generic GTK gallery was rendered and inspected after growing
from 760×477 to 900×800 and returning to 760×477. Its images are visible.

The existing Window Maker repaint fixture reports Fontconfig/Pango exit-time
allocations, including on a rerun with full unwinding and its existing
WMCreateFont exception. The new settings fixtures pass. No additional leak
suppression was added. GEM's existing menu-strip restoration failure remains.
Mac desktop capture/Accessibility permissions currently limit an interactive
settings walkthrough; native callback assertions pass independently.

## Input/footer verification limits (2026-10-22)

The current input/footer pass builds every maintained selection and passes
all native Haiku, macOS, GTK/elementary, SDL2, Athena and OPEN LOOK regressions;
Windows passes the four focused interactive control/inspector/settings runs.
Motif passes all 24 tests under Xvfb. The separate CDE suite passes 23/24:
`native_motif_runtime_tests` fails its table grid-edge pixel comparison there,
while that same executable passes on bare Xvfb. This is a desktop-specific
qualification limit, not a successful CDE grid acceptance claim. The existing
Window Maker Fontconfig/Pango exit allocations and GEM menu-strip restoration
failure remain as recorded above. No assertion or sanitizer rule was disabled.

## Motif/CDE control follow-up (2026-10-23)

All 25 Motif CTests pass on Xvfb; the shared table-focus and embedded-dropdown
changes also pass all 27 SDL2 tests. CDE native/custom property-combo pixels,
checkbox indicator pixels, plain-value foregrounds, settings spacing and
indentation, checkbox alignment and continuous table selection pass. The
classic fixture passes real date typing/Enter, calendar toggling/outside-click
closing/day selection and square selected-component time stepping. Native
inspector editing and custom-popup interactions pass on the CDE desktop.
The older grid-edge comparison sampled nominal row heights even when rows
were fitted to the viewport; it now checks the complete painted body.

The complete CDE suite passes 23/25 tests. Its Motif-runtime and
classic-control fixtures nevertheless fail
LeakSanitizer at shutdown: graphics contexts allocated by native IconGadget
font updates remain retained. The same suites are clean on Xvfb. This is an
open CDE qualification issue; no suppression was added and a successful
functional assertion run does not imply a clean sanitizer exit. CDE display
blanking initially produced an all-black inspector capture; disabling
blanking and waking the isolated display makes that fixture pass.

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
see the [VM runtime note](ELEMENTARY-VM-RUNTIME.md) for scoped evidence. The focused regression
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

## Runtime status

- Runtime-tested in the current workflow:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif in the `Tribblix-CDE` KVM guest
  - Linux OPEN LOOK/XView in Docker with Xephyr and olwm
  - Linux Window Maker/WINGs in Docker with Xephyr and Window Maker
  - Linux GEMix in Docker with local rasta
  - MS Windows binaries built with MinGW and run natively in the Windows 11 VM
  - Haiku binaries built through Docker, copied to a Haiku machine, and run
    there
  - Apple
- Still in progress:
  - other toolkit ports not listed above as runtime-tested

The 2026-09-06 compatibility check uses upstream GEM `v1.0.0`
(`0df463d9ff4b4cbbdd0612ce700c9e11e5e33661`). Both transports build and pass
all six Native CTests against the published `wischner/gcc-x86_64-gemix:latest`
image, which ships that SDK; Native no longer builds a local SDK image.
This is now the default dependency; the review-image tags below describe
the earlier diagnostic sessions. No C++ API changes were needed, and this
check adds no new visual-coverage claims. The normal release build workflow
is documented in [Build System](../manuals/book-of-native/BUILD-SYSTEM.md).

## CRT terminal validation (2026-10-07)

All nine Docker selections build the CRT command terminal. All 98 Linux
CTest executions pass, and native Windows 11 passes the real-shell and CRT
paint smoke. Haiku also passes the pixel and native terminal smoke after
fixing peer ownership of cursor resources through app-server teardown. OpenMotif and WINGs worker delivery now uses a
nonblocking pipe
to wake the production loop; the terminal needs no test-only polling timer.
The remote macOS host `leia` cannot be resolved in this pass, so these changes
have not been compiled or run on macOS. See the
[feature matrix](../manuals/book-of-native/FEATURE-MATRIX.md) and
[Haiku runtime note](HAIKU-REMOTE-RUNTIME.md) for exact coverage.

## Current open issues

- The Infinity physical-key extension cannot yet deliver held keys on either
  GEMix transport: the pinned SDK converts physical positions and AES drops
  key releases. A supported SDK/viewer raw-input and focus hook is required;
  `app::get_physical_keyboard_supported()` returns false. See
  [INFINITY.md](INFINITY.md). Audio, scoped UI delivery and child processes
  are available independently.
- The macOS verification gap from 2026-10-06 was superseded after source sync
  was explicitly authorized: the 2026-10-15 build and all 17 registered tests
  pass on `leia`, including the real CGL test. This does not establish
  interactive acceptance of every physical-key or audio device path. See
  [the current feature matrix](../manuals/book-of-native/FEATURE-MATRIX.md).
- The shared creation rollback defect from this review is repaired: peer,
  lifetime and input allocations and partial native acquisition all unwind,
  leaving the object retryable. The fault-injection regression is recorded in
  [ADJUST.md](../ADJUST.md), A18.


- The 2026-09-06 SDL2 follow-up passes all nine CTests. Nested
  tab/split/accordion canvas pixels now survive whole-window composition;
  actual SDL event regressions cover button identity and capture. Close
  policy can defer teardown for unsaved edits. Software rendering also
  needs `SDL_FRAMEBUFFER_ACCELERATION=0` to avoid implicit X11 framebuffer
  GPU probing. `SDL_Quit()` releases the runtime, while libdbus retains
  its known process-global allocation covered by the existing narrow
  test suppression.

- The Windows runtime workflow now targets the `Windows` libvirt guest,
  not Wine. The 2026-09-05 console check verified the native feature gallery
  and button activation. See [Windows VM Runtime](WINDOWS-VM-RUNTIME.md) for
  the SSH and interactive desktop debugging setup.
  Five native Windows tests pass (including interactive native-peer
  regressions), and VS Code source breakpoints work. Stock Win32 controls
  now keep native painting and visual styles; the gallery walkthrough covers
  collections, tables, input/dialogs, splitter dragging and layout resizing.
  Pause/Stop while the GUI runs remains unverified as working: remote
  interrupts do not stop it with GDBserver 10.2 or 17.2. Closing Vision's
  window ends the session normally. The maintained deployment keeps 10.2
  from the Docker image.

- The X11 follow-up (2026-09-05) reproduces the Input/Window Chrome exception
  from graphics lookup before realization. Created-but-unshown graphics now
  work, also synchronizing initial table/collection metrics with painting.
  A private Athena Form specialization prevents content-driven shrinkage and
  competing layout. Full-row lists, complete borders, scrollbar separators,
  filled table viewports and live background-colored splitters have regression
  coverage. Six X11 CTests pass under Xvfb; visual checks now run in monochrome
  TWM inside Xephyr. The installed Xaw library is unchanged. The X11 F5
  profile now starts that nested monochrome session automatically; other
  backend profiles are unchanged. Seven SDL2 CTests pass after the shared
  tab-label contrast fix.
  The second X11 pass adds full-width, bordered combo menus with single-click
  selection and text synchronization, private splitter pane hosts preserving
  borders, an explicit divider grab including its leading pixel, monochrome
  collection imagery, accordion top rules, white table rows, open selected-tab
  joins and copied alert artwork. Menu titles/popups retain their own borders
  without an additional full-width bar rule. The current pass adds ordinary
  combo hover highlighting, centered native message buttons, a small splitter
  grip and actual Xaw scrollbars for tables, icon grids, trees and canvases.
  Native scrollbar tests cover exact endpoints, real middle-button dragging
  and visibility changes. The server-input
  regression also covers menu teardown before Xt destroys the callback owner.

- GEMix's local-rasta review exercises button feedback, check/radio parity,
  text editing and copy/paste after file open/save, overlapping modeless
  windows, modal close, extreme layout resize, collection disclosure/icons,
  table borders and thumb dragging, splitter dragging, combo popups, rotated
  stock text, four-edge tabs, and status chrome. Six CTest executables pass,
  including `native_gemix_runtime_tests`. These checks use the matching GEM
  runtime fixes in the sibling GEM source tree: staged rasta presentation,
  clipped AES pattern drawing, bitmap-copy damage tracking, and synchronous
  file-selector owner restoration. Rebuilding Native alone against an older
  GEM SDK does not include those runtime fixes. AES/VDI interfaces are unchanged.
  The reported Copy-field crash did not reproduce in the reviewed runtime;
  file open/save followed by copy/paste completed without a sanitizer report.
  On this review host, the repaired runtime and resources are installed in
  the local `wischner/gcc-x86_64-gemix:input-dialog-review-20260905` image;
  `latest` now resolves to the published SDK image instead. This includes the
  follow-up fix that paints the desktop on AES workstation startup; its
  fresh-launch framebuffer regression failed before the fix and passes
  afterward. The preceding review image is
  retained under `security-review-20260905`, `open-review-20260905`, `desktop-review-20260905` and
  `native-review-20260905`, and the
  original image as
  `wischner/gcc-x86_64-gemix:before-native-review-20260905`. No image was
  pushed to a registry. The separate gemd launch profile preserves the original
  direct launch/task configuration.
  Those runtime fixes are in upstream GEM `v1.0.0`, so the published
  `wischner/gcc-x86_64-gemix:latest` image carries them: the 2026-09-06 pass
  runs the same framebuffer regressions against it and they pass.

- The GEMix interaction follow-up verifies typing, button and keyboard
  copy/paste, selector button inversion/release, immediate split-pane list
  selection, inactive left-tab separators, complete title restoration, and
  removing the menu and popup on Exit. The ordinary fields worked with
  Editing enabled; the read-only state observed during interleaved manual
  input intentionally rejects typing/paste. Focused editor clipboard commands
  now precede menu accelerators. Wheel scrolling remains unavailable in local
  rasta: its input broadcaster does not forward SDL wheel events. No AES/VDI
  extension was introduced to work around that missing input.

- The libgem/gemd transport is also runtime-tested with local rasta.
  Its matching sibling GEM runtime fixes missing query/selector wrappers,
  malformed-request checks and menu lifetime on client disconnect. The
  local Docker `open-review-20260905` image contains these changes; a registry
  image is not evidence of the same runtime. This verifies Native's call set
  using the same-host RPC ABI, not every AES/VDI entry point or a network ABI.
  The input follow-up fixes zero mouse state on queued redraw replies, hosted
  source-editor navigation codes and LF Enter, and intermediate desktop
  presentation during window close. Viewer-driven typing, both text modes,
  clipboard buttons and source-editor rendering/navigation pass. Five Native
  CTests pass in each transport; all five hosted GEM tests pass, including the
  held-button/redraw and keyboard-modifier RPC regression. A wholly blank source
  editor did not reproduce in the restarted session; its initial/reopen drawing
  was checked without claiming a separate diagnosed blank-editor defect.
  The opening follow-up fixes AES desktop/frame occlusion routing. Modal and
  modeless windows, fully or partly overlapping their owner, preserve covered
  pixels until client painting. The intermediate-frame regression fails with
  `input-review-20260905` and passes with `open-review-20260905`.

- The sibling GEM security follow-up (2026-09-05) adds peer/ownership checks,
  nonblocking bounded RPC transport, per-client VDI state and visible-region
  clipping, menu graph validation, lock/disconnect cleanup and cooperative
  standard-panel waits. Three separate Native processes were displayed in
  local Rasta; closing one left the others running. Real-server regressions
  additionally check three-client keyboard focus, menu/desktop-owner exit,
  malformed/stalled peers, and hostile font/framebuffer inputs. All five
  Native CTests pass in both direct and proxy modes against the local
  `security-review-20260905` runtime. AES/VDI public interfaces and F5 profiles
  are unchanged. This is a same-user shared desktop, not a sandbox: scrap,
  cursor and standard-panel input remain shared, and the private transport
  is not a network ABI. The sibling GEM `docs/SECURITY.md` records limits and
  remaining trust assumptions; the local image has not been registry-pushed.

- The renewed GEMix dead-field/copy-paste report did not reproduce in fresh
  direct or proxy viewer sessions. The new `native_gemix_input_tests` sends
  actual Rasta packets through the full application loop, including rapid
  focus clicks, both text modes, clipboard buttons and Ctrl+A/C/V, and input
  after modal/modeless and file-selector closure. All six Native tests pass
  in each transport; exact failing launch/interaction steps remain needed.
  The modal-frame changes are verified separately: untitled hosts reserve
  four client-excluded pixels for the `1011` enclosure, and AES alerts use
  the same edge pattern. The matching GEM runtime fixes `WF_KIND` queries
  that incorrectly returned work coordinates. Public AES/VDI APIs and F5
  configurations are unchanged.

- Other toolkit ports are still work in progress.
  They should stay out of the normal user workflow until they are built and
  exercised.
- `panel` and `canvas` are implemented on every enabled backend, but their
  verification depth differs:
  - SDL2 has runtime assertions through `native_surface_runtime_tests`,
    covering layout, nesting, scrollbar thresholds, scrolling, pointer
    routing, and a destroy/recreate cycle under ASan/UBSan.
  - X11, OpenMotif, OPEN LOOK, Window Maker, GEMix, Windows, and Haiku are
    compiled through their Docker targets and share the portable geometry,
    clamping, and painting code the SDL2 run exercises. Their own hosts have
    not been driven interactively.
  - OpenMotif and OPEN LOOK split hosts were driven interactively in their
    Tribblix guests, including divider dragging. Haiku input/chrome controls
    were driven interactively after deployment, including both combo modes.
  - Windows table and native file-open hosts were driven through Wine, and
    Window Maker's alert panel was driven in its Bookworm guest. The same
    guest was used under GDB for direct text copy/paste, both combo modes,
    native file-panel movement, modal and modeless close, collection teardown,
    repeated extreme layout resize, native splitter dragging with complete
    pane borders, full-width editable-combo popup presentation with a centered
    inset arrow, semantic alert
    badges, menu-title hover switching, and top, bottom, and right tab
    placements, including seam-free selected tabs, single inactive
    bottom/right page edges; the owner
    remained responsive and exited normally afterward.
  - The macOS implementation, including
    `lib/native/platforms/mac/surfaces.mm`, compiles and launches through the
    remote `leia` workflow. Screen Recording permission now allows desktop
    screenshots, and the updated gallery has been inspected. Remote
    Accessibility input now works and exposed splitter/tab geometry bugs,
    covered by native regression tests. A subsequent unlocked-session check
    verified upright image text, single-open accordion scrollbar ownership,
    and clearer native table grid/stripe colors. The earlier complete-gallery
    interactive walkthrough remains incomplete.
    All six Mac CTests pass normally and with ASan/UBSan.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.

- GEMix nested-canvas regression (2026-09-07): tabs formerly repainted
  their paper after child canvases, hiding their drawing and chrome.
  Structural containers now paint before descendants. Right/middle canvas
  button transitions also retain their identity for secondary drawing inks.

The inspector follow-up fixes live toolbar size/layout changes, native checkbox
label resizing, OPEN LOOK combo origins and command menu marks, WINGs text
centering, and GEM font-cell alignment. Haiku text rectangles follow resized
cells, and button edge changes erase the previous border. Windows and Haiku
now service posted UI work after startup, including custom popup commits.
The 2026-09-07 popup repair also fixes X11/Athena posted-work dispatch through
a thread-safe Xt input pipe; its colour-canvas regression runs without the
test timer that previously masked the missing dispatch. GEMix titleless
modeless popups now reserve and paint all four dialog-frame edges, preventing
client clearing from erasing the top and left. Direct and gemd pixel/input
regressions pass.
Custom property dropdowns currently close through content commit, Cancel, a
second click on the value, source-row destruction, or owner closure.

## Mouse and image-shader verification (2026-10-07)

The portable image profile is a CPU implementation on every backend. A
separate GLSL profile now has private OpenGL adapters on Linux, Windows and
macOS and Haiku. Haiku requires the optional OSMesa runtime and its verified
software driver configuration (see the remote note). Packaged Mesa 22.0.5
llvmpipe faults in generated vertex code on the test VM; Native rejects this
combination and the launcher selects `GALLIUM_DRIVER=softpipe`. Foreign Mesa
contexts must be released before activating a Native image context.
CPU effects and GLSL readback execute on
the UI thread and can delay input. Functional shader execution does not fulfill
the original Infinity proposal's GPU performance acceptance.

SDL2 exposes relative capture with device-count deltas, without a guarantee of
unaccelerated input. X11-family backends and Windows expose explicit drag
capture; macOS, Haiku and GEMix reject explicit capture. Their existing private
control gestures are independent of the public capability result. No backend
claims raw pointer input. New desktop hiding/restoration and capture behavior
still needs interactive acceptance, separately from portable pixel tests.

The 2026-10-14 macOS follow-up builds the current AppKit adapter on `leia`
and passes the shared mouse/shader regression. This does not replace
interactive pointer acceptance.

The full-monitor shader follow-up adds portable glass/housing/vent/indicator
passes. Earlier native Windows/Haiku terminal smoke evidence covers the prior
four-pass image. Updated package validation is recorded in the feature matrix;
the 2026-10-14 macOS follow-up now has original-GLSL terminal smoke coverage.

CRT visual acceptance remains incomplete. The earlier CPU approximation and
independently authored housing/glass were rejected. The current terminal demo
executes original upstream GLSL pass bodies, including `terminal_frame.frag`,
dynamic noise/raster/burn-in processing, bloom and the static compositor,
through the separate `native-glsl-1` profile. Native's CPU expression profile
does not execute GLSL. Conformance, lifecycle and smoke passes must not be
reported as upstream visual acceptance.


Linux GLSL context coexistence is fixed in the EGL adapter: it preserves and
restores the surrounding EGL/GLX toolkit context, including accelerated SDL
renderers. Context failures now include the operation and EGL error code.
The original GLSL pipeline was verified on an Intel Iris Xe desktop.


The 2026-10-14 shader review adds guarded/checkable context restoration to
Windows WGL and macOS CGL and prevents repeated failed continuous execution
on incidental repaint in the shared view. At this 2026-10-14 checkpoint Haiku
reported GLSL unavailable; the subsequent OSMesa implementation and target
verification supersede that limitation. These changes do not
establish visual or performance acceptance on other desktops. The real
WGL/CGL context regression skips when desktop OpenGL is unavailable; on
`leia` it passes without a skip. The macOS Debug build, all four focused
shader/delivery tests and original-GLSL terminal smoke pass on 2026-10-14.
At that 2026-10-14 checkpoint Windows was compile-tested and Haiku retained
CPU-profile support. The 2026-10-15 final native VM results below supersede
that runtime gap; Windows desktop GLSL still skips on this VM.


## Repository standards repair (2026-10-15)

[ADJUST.md](../ADJUST.md) records the completed A01–A18 non-Amiga repairs,
regressions and remaining acceptance limits. Callback and drawer lifetime,
creation rollback, shared EGL ownership, geometry, transactional GLSL/history,
peer-owned caches, tree selection, nonblocking worker shutdown and portable
native-close policy are repaired. OPEN LOOK wake teardown is synchronized.
Haiku's UI gate serializes native window and application loopers; native Quit
releases the gate before BeAPI exits its looper thread. Button peers publish
their completed native content before attachment can invoke drawing.
The final Windows VM run has 16 passes and two desktop-OpenGL skips; Haiku has
18 passing regression executables with Mesa softpipe, including GLSL pixels and
context ownership without a skip. Haiku inspector and multiwindow stress pass. Deployed test hashes match final Docker artifacts on both VMs.

All nine Docker backends build. Current Linux/macOS/VM results are maintained
in [the feature matrix](../manuals/book-of-native/FEATURE-MATRIX.md).
Native's XView menu/combo ownership leak is fixed. Remaining Motif selection/
notebook, XView drop-interest and WINGs font-pattern allocations were reproduced
without Native and have narrow test exceptions. They are SDK limitations;
they are not new Native leak findings or a blanket leak-free claim.
The Amiga diagnostic remains excluded from this pass and adds no UI backend.

### Classic control verification — 2026-10-22

Window Maker's new classic input checks pass under its managed desktop,
but its sanitizer fixture still reports Pango worker FcFontRenderPrepare
exit allocations; the existing repaint fixture also retains Fontconfig
allocations. Full CTest result is 20/22. Only the separately reproduced
WMCreateFont/WMWidthOfString patterns are excepted, with no blanket leak
suppression. Motif passes 25/25 headless CTests and the CDE desktop fixture
passes after startup completes. Its native font alignment is fixed in the
adapter; the Docker image does not need a font change. GEM direct/gemd pass
20/21 and 21/22, retaining the previously recorded menu-strip restoration
failure. Their SDK extended selectors and standard alerts own panel placement.
Packet-driven checks exercise Native controls alongside these native dialogs.

The 2026-10-23 GEM follow-up retains the same 20/21 direct and 21/22 gemd
results: the remaining failure is `native_gemix_runtime_tests`' menu-strip
restoration assertion. Both builds pass the extended real-input fixture for
SDK panels/alerts, first-use combo/tab-list input, calendar activation,
caret-based square time buttons, shared borders, navigator spacing, final
property rules, normal modal-owner text with blocked input and actual splitter cursor-bitmap
restoration. Vision's modal message text also fits the measured stock font.

### GTK / OPEN LOOK / WINGs follow-up — 2026-10-23

GTK Wayland and elementary each pass 20/20 tests with actual editable-popup row focus and native-button
foreground checks. The headless popup test uses the real GTK popup without an
autohide grab because Weston has no input-grab serial; normal application
posting retains GTK's native autohide behavior. OPEN LOOK passes 25/25 tests,
including native multiline font parity and pickers created before their page
maps. A toolkit-only PANEL_MULTILINE_TEXT creation/destruction probe reproduces
its 1025-byte TextSW input record and 35-byte extras-menu filename retention.
The corresponding allocation symbols join the existing narrow XView exception;
no blanket leak suppression was added. Window Maker's separate Pango/Fontconfig
shutdown allocation limitation remains; its native list/navigation, first-open
editor, cursor-image and calendar checks are covered by the classic fixture.
The full Window Maker suite passes 20/22: its repaint and classic-control
executables complete their functional checks before reporting those shutdown
allocations. No new WINGs leak exception was added.


### Picker geometry / native style follow-up — 2026-10-23

X11 passes 21/21 tests with text-fitting picker widths, half-character text
margins, square time arrows and a real arrow-cursor image over its calendar.
GTK Wayland and elementary each pass 20/20, including native CSS pixel parity
for narrow vertical-toolbar frames in light/dark interaction states.
Motif passes 25/25 under Xvfb. CDE functional checks verify pre-map native
text I-beams, matching native checkbox/radio fonts, a native calendar border
and unchanged native property dropdown styling. Its existing IconGadget
font-update shutdown allocations remain visible under LeakSanitizer.
GEM direct/gemd pass 20/21 and 21/22; the real-input fixture verifies compact
fields and normal parent button ink with modal input blocked. The existing
SDK menu-strip restoration assertion remains the only failure. No new leak
suppression was added.


### OPEN LOOK / WINGs / CDE picker verification — 2026-10-23

OPEN LOOK passes 25/25 with symmetric native search/list frame geometry,
compact date/time values with character margins, and repeated selected-component
stepping. Its private SDK text/list-state use is confined to the adapter; native
painting and scrolling remain in use. WINGs compact-time checks pass, while its
full suite remains 20/22 due to the existing Fontconfig/Pango exit allocations.

Generic Motif passes 25/25. On CDE, the date calendar's own native shell border,
compact fields, equal visible up/down glyph bounds and black uniform property
values all pass. CDE's two remaining failures are existing IconGadget font-update
shutdown allocations: the runtime fixture reports 1120 bytes and the classic
fixture 640 bytes after completing their functional assertions (23/25 total).
The actual rebuilt Vision gallery preserves allocated 330-pixel editor widths
and real I-beams at their right edges. Shared nested inspector-host recognition
also passes GTK's 20/20 and three focused GEM checks. No new leak exception was
added.
