# Haiku Remote Runtime

This note records the current Haiku workflow that is actually exercised.

## Current workflow

- Build locally through Docker:
  - `cmake -S . -B build/cmake`
  - `cmake --build build/cmake --target docker-haiku`
- Copy the produced binaries to the Haiku machine over `scp -O` when the
  target does not provide the SFTP subsystem required by newer SCP clients.
- Debug the binaries on the Haiku machine through GDB over `ssh`.

The VS Code launch entry is **Haiku, VM**. Its VM readiness check, remote
directory preparation, `scp -O -r` deployment and SSH/GDB transport all use the
SSH alias `hotel`. The host SSH configuration supplies the user, address and
identity; a plain `ssh hotel` must open the guest shell without a password.
The libvirt domain remains `Haiku`.

On 2026-10-21, password-free `ssh hotel` reports the hostname `hotel`, and
GDB is available at the configured `/boot/system/bin/gdb`. The F5 readiness,
directory preparation, deployment and debugger connection all use this alias;
the libvirt domain name remains `Haiku`.

## VM keyboard input

The `Haiku` libvirt domain has a USB keyboard in its live and saved device
configuration. In the 2026-09-07 keyboard investigation, the VM's PS/2 keyboard
delivered no events to either Vision or Terminal. Pointer input and native
window-message input still worked. Adding the USB keyboard restored physical
typing in property-grid text and number cells without an editor implementation
change. The device remains present across VM restarts.

The required libvirt device is:

```xml
<input type='keyboard' bus='usb'/>
```

When diagnosing missing typing, check Terminal as well as Vision before
treating the failure as a text-control problem. The inspector regression sends
keys to the window's preferred target so `BWindow` resolves the focused editor;
it does not address the text view directly. Real VM keyboard checks complement
that test because a window message does not exercise the virtual keyboard or
Haiku's input driver.

The Docker cross-build and the inspector regression pass with window-routed
text and number input. VM keystrokes also change the Name and Width properties
and their live preview with the persistent USB device installed.

## What is verified

- `vision` builds in `build/haiku/src/vision`.
- The binary is copied to `/boot/home/Projects/native/run/vision`.
- The binary is launched under `/boot/system/bin/gdb` over SSH.
- The 2026-09-04 control walkthrough ran in the Haiku VM, with real pointer
  and keyboard input and screenshots from Haiku's `screenshot -s` utility.
  It covered Gallery selection, accordion/tree disclosure, table scrolling
  and grid lines, splitter dragging, four-edge tabs, and both combo styles.
  Build-local captures are kept under `build/haiku-review/`.
- The 2026-09-05 follow-up checked alternating rows on/off, the unchecked
  indicator, equal native/virtual row pitch, accordion headers without the
  blue focus line, real native table scrollbar dragging, and both below-field
  combo popups. The editable combo uses one border enclosing its arrow.

`native_window_api_tests` constructs its own `BApplication` on Haiku even
though it creates no control windows: font/theme queries still require an
app-server connection. The collection runtime test also inspects local combo
visibility, text-message targets, and preservation of native drawing state.
It additionally checks the inset arrow's native parent/geometry and real
virtual-table scrollbar endpoints. Model tests cover partially visible rows
and complete final-page reveal with native row pitch.

The five Haiku test executables passed again after the 2026-09-05 follow-up;
the collection runtime test also passed ten consecutive runs. The SDL2 Docker build and all
seven SDL2 CTest tests passed with its original checkbox renderer retained.

The 2026-09-07 inspector follow-up passes `native_inspector_runtime_tests`
on the Haiku VM desktop. It checks native checkbox/text/number input,
presented toolbar-icon and value-text pixels, repeated commands and status
updates, sticky selection, bottom-edge input, scrolling, and destroy/recreate.
The desktop must be awake for the screen-pixel assertions. The fixture sends
view-relative pointer coordinates through Haiku's native message dispatcher
and uses the production application messenger to dispatch posted assertions
and finish after programmatic closure. The follow-up additionally covers
post-creation grid resizing, all sixteen live button-border masks, toolbar
growth/shrinkage and neighboring-bar hits, and arbitrary custom values
committed from a native control inside a dropdown popup. Commit, Cancel,
reopening, and callbacks retained from a closed popup are checked.

## Why this note exists

This is an environment-specific constraint for a remote target. It belongs in
`docs/notes/` rather than in the normal build book.

The Haiku deployment uses `scp -O -r` for Vision and its sibling
`retro-terminal/` shader/font assets, including license notices. The acceptance
command is `./vision --retro-terminal --terminal-smoke`. A cross-built pixel
fixture can be run with `./native_terminal_screen_tests ./retro-terminal` so it
does not depend on the Linux build host's absolute source path.

The 2026-10-07 CRT acceptance passes the cross-built ANSI/font/CRT pixel test
and the native terminal smoke: a real `/bin/sh` command, processed paint,
scoped result delivery, and clean application shutdown. Cursor resources now
belong to the window peer; process-static `BCursor` destructors had trapped
after the app-server connection closed.

Normal window closure posts the application quit message to its looper,
avoiding an unlocked `BApplication::Quit()` call from the window thread.

## UI serialization repair (2026-10-15)

Application messages, startup and native window callbacks now share one recursive
UI gate. Window callbacks temporarily release native looper locks before taking
the gate, then reacquire them in a consistent order. This serializes portable
state across different BWindow threads; it does not promise one physical thread.
Native destruction retires the window asynchronously. The retired quit path
releases the gate before BeAPI can exit the looper without C++ stack unwinding.
Queued button commands recover live controls by non-reused IDs rather than a
raw C++ pointer.

The initial repair deployment passed 16 regression executables and skipped
GLSL before the offscreen adapter existed. The later GLSL deployment below
supersedes that skip: all 18 regression executables now pass with softpipe. The native
inspector regression covers actual button drawing, all sixteen live border
masks, editor/toolbar input and owner recreation. Adopting button state before
allocation requires publishing its finished BButton content pointer before
native attachment; otherwise first drawing cannot resolve its window.

The multiwindow dispatch test receives actual paints on both windows while
posted updates/resizes run and closes cleanly. Lifecycle, worker-shutdown and
native close-policy tests also pass. Keep the desktop awake for paint acceptance:
the VM's sleeping desktop produced zero paints, independently of message dispatch. Mouse/shader fixtures can be deployed as tests/data and run
with native_mouse_shader_tests --data data; portable terminal pixels take
native_terminal_screen_tests retro-terminal. Do not run host CTest files with
Linux absolute executable/fixture paths on Haiku.

## Leaving a visible application for inspection

Regression executables open temporary windows and close them automatically;
a completed suite does not leave Vision running. For a persistent manual view,
run `./vision --properties` from `/boot/home/Projects/native/run/` in a Haiku
Terminal. The gallery and Properties and Toolbars windows remain open until
closed by the user. Deploy the current Docker-built Vision separately from the
test executables; copying tests alone does not update the interactive program.

The 2026-10-15 follow-up deployed the final Vision, verified its SHA256 against
`build/haiku/src/vision`, and captured both visible windows in
`build/adjust-haiku-vision-screen.png`. This confirms visible startup; it does
not add an exhaustive interactive walkthrough. GLSL verification follows.

## Original GLSL shaders through OSMesa (2026-10-15)

The VM already has Mesa's libOSMesa.so.8 and Software Pipe add-on; no package
installation was needed. Native loads OSMesa dynamically and validates an
OpenGL 3.3 core context. It returns shader pixels through the normal Haiku
canvas, without a BGLView or GPU passthrough. Rendering is on the CPU.

The installed Mesa 22.0.5 llvmpipe fails in llvm_middle_end_linear_run at its
JIT-generated vertex code during glDrawArrays. Native diagnoses this known
combination before drawing. The same package's softpipe passes all 18 target
regression executables, including real shader pixels and native context
ownership, and passes the original eleven-pass terminal/shell smoke:

```bash
cd /boot/home/Projects/native/run
GALLIUM_DRIVER=softpipe ./native_haiku_glsl_context_tests
GALLIUM_DRIVER=softpipe ./native_glsl_renderer_tests
GALLIUM_DRIVER=softpipe ./vision --retro-terminal --terminal-smoke
GALLIUM_DRIVER=softpipe ./vision --retro-terminal
```

The last command stays open for manual inspection. The user confirmed the
visible shader demo works. Preserve the complete retro-terminal asset directory
and its attribution files. SHA256 checks match all 19 deployed binaries
(Vision and the 18 regression executables) to the final Docker build;
`build/haiku-glsl-final-verification.log` records this evidence.
GLSL CTest environment and Haiku's VS Code F5 profile
supply GALLIUM_DRIVER=softpipe. Native does not change process environment or
system settings. Other applications' renderer choices remain their own.

Native contexts can nest in binding order; entered and retired peer teardown
is covered by native_haiku_glsl_context_tests. Foreign Mesa/BGLView bindings
are rejected unchanged before activation; applications using another GL client
must release that binding before Native image work. Missing Mesa or OpenGL 3.3
uses ordinary shader error/fallback handling. Software execution and synchronous
readback do not imply interactive full-resolution performance or exact upstream
visual parity.


On 2026-10-21 `hotel` reached the current Haiku desktop and ran both settings
regression modes successfully. The SDK's native shared-kit DateEdit and
TimeEdit controls deliver their Invoke callbacks through the shared rules.
Clear/re-add and recreation exposed synchronous old-row paint/latch calls
inside BOutlineListView removal; guarding the native rebuild fixes them.

The existing collection-runtime and window-API regressions also pass on
`hotel` after the tree rebuild correction.


## Native control review (2026-10-21)

The final Docker build ran 20 checks on `hotel`, all passing: the 19 registered
CTest commands and the additional collection-runtime executable. This includes
native settings/picker callbacks, inspector hit testing, toolbar commands,
all preview border masks, scrolling and recreation, plus existing lifecycle,
worker, audio and softpipe GLSL checks. The isolated deployment is
`/boot/home/Projects/native/run/bugs-review`.

A real desktop pointer check opened the native BCalendarView popup, selected
October 16 and confirmed the compact DateEdit field and status updated before
the popup closed. Captures also show grouped navigation without an idle
scrollbar, a full-width search field and matching page/checkbox backgrounds.
The popup uses native selection and invocation messages; unavailable dates
retain the shared validation policy and native disabled-day painting.
Alerts center their final native frame on the owner. File-panel placement
needed the additional 2026-10-22 correction described below.


## Control sizing and file-panel placement (2026-10-22)

The warning-free Docker Haiku build passes all 20 checks again on `hotel`.
The isolated `bugs-review` deployment's Vision, settings and inspector binaries
match the local build's SHA-256 hashes. Settings runtime now checks centering
of Open, Save and Select Folder after BFilePanel::Show, native text-field height,
complete date/time field widths and containment of the calendar trigger.
The test allows Tracker's initial asynchronous directory scan to settle before
destroying each panel. Inspector runtime verifies native button minimum sizes
at 16-, 24- and 32-pixel icons and captures status pixels to reject outer side
dividers. Existing lifecycle, worker, collection and softpipe checks still pass.

Desktop captures show an equally inset, framed outline-list navigator, centered
compact time text and equal arrows, plus the date calendar action inside its
text border. Real pointer input opens that BCalendarView popup and selects
October 16; both the field and status update and the popup closes. The toolbar
gallery shows complete native buttons and the status top rule without extra
side dividers. The completed Vision binary is also deployed to the regular
`/boot/home/Projects/native/run/vision` location.

## Footer, input and table consistency (2026-10-22)

The final Docker cross-build passes all 20 commands on `hotel`. The settings
fixture compares actual cached heights for single-line entries, both combo
styles and both pickers, checks repeated resizes preserve position, and
verifies the native calendar popup's left edge, width and bottom anchoring.
The collection fixture preserves native row identities and multi-selection
through a grid change, then sends a plain native click and verifies only the
clicked row remains selected. Inspector pixels check the flat panel footer.
Shared window tests cover footer priority in both toolbar construction orders.

Visible checks show the framed search field, the shared native dropdown arrow,
a full-width calendar immediately below the then full-width date field, and
the flat footer below the bottom toolbar. The compact-date refinement below
supersedes that field sizing. The tested Vision is copied to the regular run
location; its SHA-256 matches `build/haiku/src/vision`. Test-owned galleries
are closed after inspection.

## Compact date and single-frame dropdowns (2026-10-22)

DateEdit now keeps its preferred compact width inside a larger allocation.
The calendar opens below the actual field, matching its width when there is
room and growing when needed for seven readable day columns. Month navigation
uses equal square BButtons at the standard text-control height. Date and
editable-combo accessories draw only the shared BControlLook arrow on document
paper; the editor retains the sole outer frame. Selection-only fields retain
native menu-field surfaces with the same arrow painter.

The Docker cross-build is warning-free and all 20 native commands pass again
on `hotel`. Settings runtime covers wide/narrow allocations, popup anchoring
and minimum width, compact month buttons and both native month commands.
Collection runtime still covers both combo styles and native input callbacks.

Desktop captures confirm the date trigger has one white editor surface with
one native arrow, the field stays compact, and the calendar is wider only
when its day columns need room. A real pointer click on the smaller next-month
button advances October to November. These are native BButton messages, not
application-side date changes.

The editable-combo gallery also shows a single white field and native arrow,
with no nested light/dark button strips. Captured corner samples around both
accessories match the white editor paper. The tested Vision replaces the
regular run binary with matching local/deployed SHA-256 hashes.

## Dropdown feedback and toggling (2026-10-22)

DateEdit measures its slash separators instead of using fixed spacer cells.
At the VM's current font this reduces the compact field from 135 to 123
pixels, about nine percent. Date and editable-combo arrows occupy centered
14-pixel hit areas within the existing text border. Their idle paper stays
white; native BControlLook pressed backgrounds remain visible while open.

Calendar arrows toggle their existing popup and clear their pressed state
when it closes. Native combo tracking cancels on another press over the
original trigger. The hook accounts for Haiku waiting for the next press
before calling it again, and retains cancellation across the native menu's
quick-click tracking restart.

The warning-free Docker build passes all 20 native regression commands on
`hotel`. Settings runtime checks narrow arrow geometry and calendar toggle
state; collection runtime checks the editable arrow's width and text clearance.
Real desktop pointer checks separately open and close the calendar, editable
combo and selection-only combo with exactly two clicks on each arrow.
Captures show pressed feedback while held/open and cleared feedback after
the second click. The tested Vision is deployed to the regular run location
with matching local and remote SHA-256 hashes.
