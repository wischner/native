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

## Why this note exists

This is an environment-specific constraint for a remote target. It belongs in
`docs/notes/` rather than in the normal build book.
