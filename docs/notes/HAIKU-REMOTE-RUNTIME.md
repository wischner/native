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
SSH alias `haiku`. The host SSH configuration supplies the user, address and
identity; a plain `ssh haiku` must open the guest shell without a password.
The libvirt domain remains `Haiku`.

On 2026-10-19, `ssh haiku` opened the guest shell successfully; the session
reported `Haiku` as `user`. This guest currently has no `gdb` on `PATH` or
at the configured `/boot/system/bin/gdb`; SSH access is verified, but F5 remote
debugging requires GDB to be installed in the guest.

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
