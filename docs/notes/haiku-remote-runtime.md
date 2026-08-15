# Haiku Remote Runtime

This note records the current Haiku workflow that is actually exercised.

## Current workflow

- Build locally through Docker:
  - `cmake -S . -B build/cmake`
  - `cmake --build build/cmake --target docker-haiku`
- Copy the produced binaries to the Haiku machine over `scp`.
- Debug the binaries on the Haiku machine through GDB over `ssh`.

The VS Code tasks and launch entry include VM start, deploy, and remote-debug
workflows.

## What is verified

- `vision` builds in `build/haiku/src/vision`.
- The binary is copied to `/boot/home/Projects/native/run/vision`.
- The binary is launched under `/boot/system/bin/gdb` over SSH.

## Why this note exists

This is an environment-specific constraint for a remote target. It belongs in
`docs/notes/` rather than in the normal build book.
