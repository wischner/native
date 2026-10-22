# macOS VM Remote Runtime (mike)

This note records the current remote macOS workflow used from Linux.

## Target

- Host: `mike` (connect directly with password-free `ssh mike`)
- User: SSH-configured user (`tomaz` on the VM)
- Remote project path: `/Users/tomaz/Projects/native`

The scripts are parameterized through environment variables:

- `MAC_REMOTE_HOST` (default `mike`)
- `MAC_REMOTE_USER` (default empty; honors the SSH alias user)
- `MAC_REMOTE_BASE` (default `/Users/tomaz/Projects`, or `/Users/<user>/Projects` with a user override)
- `MAC_REMOTE_PROJECT` (default local repo basename)

The VS Code launch profile is **macOS, VM**. Historical
verification results below refer to the former host `leia`; they do not
establish test coverage on `mike`. The 2026-10-21 Debug build on `mike`
succeeds with AppleClang 17 and all 19 CTests pass. Password-free SSH, Clang,
LLDB and rsync were verified on `mike`. Apple Command Line Tools are now
installed, together with checksum-verified CMake 4.4.4 in
`~/Applications/CMake.app`. Developer Tools authorization is enabled; interactive breakpoint and
automatic debugger-exit checks pass.
The build helper checks compiler/CMake availability before syncing and finds
the user-local CMake bundle. CMake probes C++20 cooperative threads and enables
`-fexperimental-library` for Apple SDKs that require it.

## Scripts

- `scripts/macos/remote/sync.sh`
  - rsync source tree to macOS target
- `scripts/macos/remote/build.sh`
  - sync + configure + build the `Debug` application bundle on macOS
    (`build/macos-debug/src/vision.app`)
- `scripts/macos/remote/run.sh`
  - run Vision on macOS over SSH
- `scripts/macos/remote/debug.sh`
  - verify Developer Tools authorization and run Vision under remote `lldb`
- `scripts/macos/remote/smoke-test.sh`
  - build on macOS, verify binaries, clear/check quarantine xattr,
    ad-hoc sign, and launch/kill Vision for a short smoke test

## Why this avoids Gatekeeper "internet download" rejection

The application bundle is compiled on the Mac itself from synced source. It is
not copied as a prebuilt internet artifact. The smoke test also verifies that
no `com.apple.quarantine` xattr remains on the bundle.

## VS Code tasks and launch

- Tasks now use the script-backed labels:
  - `Build Vision Debug (macOS, VM)`
  - `Smoke Test Vision Debug (macOS, VM)`
- The launch entry builds and starts the remote LLDB session through SSH.

## Native-control regression checks

The settings/toolbar follow-up uses a dedicated AppKit source-list sidebar,
sidebar material and optional NSSearchField. Stable IDs, repeated section
headings, filtering without page teardown and search callbacks are covered by
`native_settings_runtime_tests`. Date/time editors use native intrinsic sizes;
the date's explicit calendar button opens a native popover whose selections
pass the same date/holiday validation as the editor. The test activates that
button and rejects a holiday through the popover's actual NSDatePicker.

Stock edge toolbars host flexible-height native flat NSButtons and NSBox separators;
configuration creates their children, never painting. Native actions, disabled
and exclusive state, visible icon pixels and native workspace file-icon alpha
have regression checks. AppKit cell measurements prevent truncated labels.
Retained AppKit content views/delegates check weak resource-generation tokens;
late callbacks cannot reach a destroyed C++ dialog or a replacement resource.
The regression explicitly renders a retained view after recreation and deletion.
Changing preview border sides preserves stock rounded-button drawing. Read-only
borderless help uses transparent native text on the page background.

Toolbar primitives use an explicit native appearance host to avoid snapshot
text dimming. Appearance checks compare light/dark and selected/disabled
states, composing image targets through AppKit before color comparison.
Accessory probes allow a one-device-pixel baseline phase between native
hierarchies; color/state tolerances remain strict. Production General, Dates,
Time and Properties pages are inspected through AppKit view captures because
system desktop captures remain subject to the permissions noted below.


Mac rounded-button edge masks use the intrinsic native control height and
AppKit alignment rectangle rather than trimming the outer view margin.
The same clip applies to native painting and layer composition, so disabling
an edge removes the inset bezel while retaining AppKit's rounded drawing.

The 2026-10-21 VM follow-up covers I-beams before text editing, split-divider
resize cursors before clicking, panel/canvas clipping in the composed
Properties and Toolbars dialog, and themed check/radio pixels compared with
real AppKit drawing in light/dark, selected and disabled states. Native
button/check/radio primitives use native drawing with the real destination
window state; image targets use a native bitmap context. The cached-display
path is excluded because it changes indicator appearance. Window motion
updates ruler markers without pressing a mouse button.

The LLDB helper launches to the entry stop, imports `lldb_autoquit.py`, then
continues. The helper listens for the inferior's final state and ends the
owned debugger process with the inferior's exit status after termination.
Breakpoint and crash stops remain interactive; an exited inferior no longer
leaves LLDB and the SSH terminal running in VS Code.

The 2026-09-05 AppKit audit passes all six CTests on `leia`. The dedicated
`native_mac_runtime_tests` renders controls into AppKit bitmap targets and
verifies that stock controls do not invoke the portable painter. It checks
native text/image cells, group disclosure, icon selection, accordion scrolling
hosts, and the retained derived-button drawing extension. Follow-up checks
cover upright image text with straight-alpha blending, single-open accordion
scrollbar ownership after section changes and resizing, and table grid/stripe
contrast in both light and dark appearances. The general collection
lifecycle test is also registered on macOS. All six tests also
pass in a separate AddressSanitizer/UndefinedBehaviorSanitizer build (leak
detection disabled for process-lifetime AppKit resources).

### Reproduce the automated checks

From the Linux checkout, sync and build using the existing remote script,
then run CTest on the Mac (these commands use the default target above):

```bash
bash scripts/macos/remote/build.sh
ssh mike 'export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH; cd /Users/tomaz/Projects/native && ctest --test-dir build/macos-debug --output-on-failure'
```

For the separate sanitizer audit, SSH into `mike` and run:

```bash
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
cd /Users/tomaz/Projects/native
cmake -S . -B build/macos-native-audit -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -pedantic -fsanitize=address,undefined' \
  -DCMAKE_OBJCXX_FLAGS='-Wall -Wextra -pedantic -fsanitize=address,undefined' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/macos-native-audit -j8
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/macos-native-audit --output-on-failure
```

The explicit flags matter: the top-level automatic sanitizer configuration
targets GCC, not AppleClang. This audit tree is separate from the F5 Debug
bundle and does not change the launch configuration.

### Desktop permissions and visual coverage

The SSH login can build and run GUI tests in the logged-in desktop session.
After enabling Screen Recording for the SSH session, `screencapture` can
capture the desktop; the updated gallery has been inspected there. macOS may
also present an explicit `com.apple.sshd-session` permission prompt that the
console user must approve. Accessibility is a separate permission: remote
System Events input is now permitted after enabling the SSH session under
Privacy & Security → Accessibility (`/usr/libexec/sshd-session` on this Mac).
The console must remain logged in and unlocked: a locked session returns
black captures and hides application windows from Accessibility. The live
walkthrough found empty splitter panes and oversized tab content; those now
have dedicated geometry regressions, alongside native virtual-grid scrolling
and grid-style checks. The session locked before that complete-gallery
walkthrough finished. After unlocking, targeted desktop checks verified the
image-text, duplicate accordion scrollbar and table-contrast fixes; this does
not constitute a completed mouse-driven walkthrough of every gallery feature.
`NATIVE_MAC_TEST_SNAPSHOT` optionally names a PNG output path for
the regression window's own offscreen rendering, without desktop capture.

The targeted desktop checks confirmed upright text in the gallery image,
one scrollbar in the expanded single-open accordion, and clearer grids and
alternating rows in both table examples. Light/dark color contrast is covered
by automated AppKit color checks; this is not a claim of a full visual
walkthrough in both appearances. Screen Recording and Accessibility are
needed for remote desktop inspection and input, not for the test's own
offscreen bitmap rendering.


## GLSL context and shader verification (2026-10-14)

The current Debug build on `leia` passes after correcting the CGL profile
constant to the SDK's `kCGLOGLPVersion_GL4_Core` (OpenGL 4.1). The four focused
CTests pass: `native_desktop_glsl_context_tests`, `native_mouse_shader_tests`,
`native_terminal_screen_tests` and `native_infinity_tests`. The CGL test runs
against real contexts without skipping and checks repeated handoff, nesting
rejection, idle leave and destruction while entered. `vision --retro-terminal
--terminal-smoke` also exits zero after original GLSL output becomes active
and the host-shell echo command completes. This is functional shader evidence;
it does not add a visual walkthrough, performance claim or full-suite result.

After the configured sync/build, reproduce these checks on `leia`:

```bash
cd /Users/tomaz/Projects/native
ctest --test-dir build/macos-debug --output-on-failure -V \
  -R 'native_(desktop_glsl_context|mouse_shader|terminal_screen|infinity)_tests'
build/macos-debug/src/vision.app/Contents/MacOS/vision \
  --retro-terminal --terminal-smoke
```

## Non-Amiga repair verification (2026-10-15)

The authorized sync/build and full CTest suite pass 17/17 on leia, including
real CGL handoff, GLSL history/live installation, callback/creation rollback,
independent worker shutdown and native performClose cancellation/acceptance.
Run ctest --test-dir build/macos-debug --output-on-failure after the configured
build script. These are functional regressions; the earlier full-gallery visual
walkthrough remains incomplete. AppleClang with the installed Apple SDK is the
explicit platform exception to Docker-backed GCC verification.


The 2026-10-21 settings/picker addition passes all 19 CTests on `mike`,
including real NSDatePicker action delivery, rejection rollback and page
recreation. The production `vision --settings` also creates its navigation
and pages. This session's System Events request reports assistive access
unavailable; desktop captures hide the application windows. Those captures
are not visual acceptance evidence. Direct offscreen captures from the
running production gallery were inspected for both Dates and Time; this
verifies their layout, not a physical-input walkthrough.


The final 2026-10-21 control review passes all 19 CTests on `mike`. Visible-owner
message boxes now use native NSAlert sheets attached to that owner, matching
the existing file-panel attachment. The presentation regression verifies sheet
ownership and the synchronous three-button result. A hidden owner retains
AppKit's application-modal alert behavior.


## Integrated controls and navigation (2026-10-22)

Toolbar feedback uses flexible-height native buttons and alignment rectangles
so its bezel grows with 16–48 pixel icons on horizontal and vertical strips.
The matching theme primitive uses the same native style. All-edge removal
switches off the stock rounded button bezel; partial masks retain native
clipping and restoring an edge restores the bezel.

Each picker has one native surrounding text-field bezel at the standard
NSTextField intrinsic height and uses the adaptive native control color. Dates
contain a text-only editor and calendar trigger without a stepper; times retain
their native stepper. Native baseline and image-cell metrics align the text and
accessories; the time stepper meets the field's right edge.
The existing native popover and shared date/time validation remain in use.
The second implicit calendar overlay is disabled in favor of that one trigger.

Vision embeds Font Awesome Free gear/calendar/clock tiles, documented with
attribution in `src/assets/navigation/README.md`. AppKit source-list rows use
24-pixel images and 32-pixel page heights so full-color selected icons remain
visible in both Aqua appearances.

The source list uses explicit native row metrics: AppKit's system row-size
styles otherwise overwrite image-view sizes when reusing a selected cell.
Custom row-size metrics retain the native source-list renderer and its
selection, with 24-pixel artwork on every page.


The expanded native regressions pass on `mike`: every 16–48 pixel toolbar icon
size on horizontal and vertical bars, whole-bezel removal with no corner
fragments, native picker accessory containment and hit testing, calendar
activation/rejection, and all three selected Vision navigation images in light
and dark appearances. Test-owned AppKit captures show the integrated Dates and
Time pages and 48-pixel selected toolbar buttons. Desktop capture permissions
are unnecessary for these captures of the test's own views.

Flexible toolbar theme comparisons paint a real reference NSButton into a
sibling canvas, using the same destination context as the primitive. AppKit
changes flexible bezels' fill tone and idle text when caching a button directly;
matching those contexts preserves the strict color/state tolerances.

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


## Picker baseline and symbol alignment (2026-10-22)

A follow-up replaces geometric editor centering with baseline alignment after
setting the native editor height. NSDatePicker is unflipped inside the flipped
field, and its native ornament/typography padding is not the visible text
center. Calendar image-cell padding is handled separately; the time stepper
meets the right edge without a gutter. NSTextField clips native decoration.
The widgets and event/availability paths remain AppKit-owned.

The completed AppleClang build passes all 19 CTests on `mike`. The AppKit
regression compares rendered glyph rows with an NSTextField containing the
same locale-formatted value, and checks painted calendar/arrow centering in
both appearances. The existing native settings test still verifies calendar
hit testing, popup activation and rejection rollback, and now checks the flush
right edge. The final Dates/Time production captures were inspected in Aqua
and Dark Aqua. These are test-owned view captures, not a physical-input
walkthrough. GCC/Docker SDL2 builds and its two settings regressions pass.

The regression writes `picker-date-{light,dark}.png`,
`picker-time-{light,dark}.png`, and `picker-reference-{light,dark}.png` beside
its existing `native-mac-settings-*` production-page captures in
`build/macos-debug/tests/`. Render the full page when inspecting Dark Aqua: a
cached native field can contain transparent background pixels.

## Shared input and footer geometry (2026-10-22)

After GCC/Docker validation, the documented AppleClang build on `mike` passes
all 19 CTests. Single-line controls resolve to NSTextField's intrinsic height;
NSComboBox alignment rectangles account for native outer margins during
creation, movement and resizing. Picker baselines remain native; the calendar
button fills the field's height so its image cell centers the symbol without
relying on a taller host's fractional origin. Light/dark raster checks compare
picker glyph rows with a stock text field and verify accessory centering.
Status bars share the bottom-priority reservation tested by window API checks.
