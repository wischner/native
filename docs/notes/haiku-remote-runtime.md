# Haiku Remote Runtime And Debug

This note records the current Haiku workflow that is actually exercised.

## Current workflow

- Build locally through Docker:
  - `cmake -S . -B out`
  - `cmake --build out --target docker-haiku`
- Copy the produced binaries to the Haiku machine over `scp`.
- Run the binaries on the Haiku machine over `ssh`.

The current VS Code tasks and launch entries use this model for run only.

## What is verified

- `app-example` builds in `build/haiku/examples/01_app_example/app-example`
- `painter-example` builds in `build/haiku/examples/02_painter_example/painter-example`
- both binaries were copied to:
  - `/boot/home/Projects/native/run/`
- both binaries were launched on the Haiku machine and stayed alive during a
  short smoke test

## Current debug blocker

Remote source-level debugging is not working on the tested Haiku machine yet.

The blocker is not the build or SSH path. Remote `gdb` can start, breakpoints
can be hit, and execution can continue. The remaining problem is source-level
stepping.

On the tested machine, `next` and `step` fail because `gdb` cannot insert the
temporary breakpoints it needs for stepping.

Because of that, the repository does not currently expose a Haiku VS Code debug
launch. Only deploy-and-run entries are kept in the normal user workflow.

## Why this note exists

This is an environment-specific constraint for a remote target. It belongs in
`docs/notes/` rather than in the normal build book.
