# Notes

This directory is reserved for exceptional notes and active issue inventories.

Examples of material that belongs here:

- remote compilation or remote debugging instructions
- unusual host setup requirements for non-standard targets
- one-off integration constraints that do not belong in the normal build book
- current open issues
- design reviews and refactoring proposals
- code TODO inventories
- backend status notes when the status is not yet a finished user-facing workflow

What does not belong here:

- standard local Linux build instructions
- standard Docker build instructions
- normal example-running instructions
- speculative porting notes for unfinished backends

The normal maintained workflow belongs in `docs/manuals/book-of-native/`.

## Current notes

The repository-wide review and adjustment worklist is [ADJUST.md](../ADJUST.md).
It includes the measured Amiga runtime prerequisites; it is not a backend manual.

- [Native versus custom control audit](CUSTOM.md)
- [Deferred theme-interface split review](REFACTOR.md)
- [Infinity emulator: portable services, migration and remaining limits](INFINITY.md)
- [Infinity mouse and shader design review](MOUSE-AND-SHADERS-REVIEW.md)
- [Backend open issues](BACKEND-OPEN-ISSUES.md)
- [elementary OS VM runtime, sidebar spacing, GTK resize corrections and Pantheon test limits](ELEMENTARY-VM-RUNTIME.md)
- [Haiku remote runtime via `ssh hotel`, USB keyboard, VM walkthrough, footer/input/table checks and debug](HAIKU-REMOTE-RUNTIME.md)
- [macOS remote runtime, debugger shutdown, native sidebar/search/toolbar and picker/GLSL regressions, desktop permissions (macOS VM, mike)](MACOS-REMOTE-RUNTIME.md)
- [Production source TODOs](PRODUCTION-SOURCE-TODOS.md)
- [Lab source TODOs](LAB-SOURCE-TODOS.md)
- [Windows Wine runtime note](WINDOWS-WINE-RUNTIME.md)
- [Windows 11 VM runtime, WinUI dependency, native control checks and debugging](WINDOWS-VM-RUNTIME.md)

The backend-open-issues inventory also records GTK 4.14 Wayland/elementary
advanced-table, pointer and Pantheon acceptance limits. Normal
Docker/Weston setup belongs in the book's Build System chapter.
