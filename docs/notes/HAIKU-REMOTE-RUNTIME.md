# Haiku Remote Runtime

This note records the current Haiku workflow that is actually exercised.

## Current workflow

- Build locally through Docker:
  - `cmake -S . -B build/cmake`
  - `cmake --build build/cmake --target docker-haiku`
- Copy the produced binaries to the Haiku machine over `scp -O` when the
  target does not provide the SFTP subsystem required by newer SCP clients.
- Debug the binaries on the Haiku machine through GDB over `ssh`.

The VS Code tasks and launch entry include VM start, deploy, and remote-debug
workflows.

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
