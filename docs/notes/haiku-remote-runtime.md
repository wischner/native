# Haiku Remote Runtime

This note records the current Haiku workflow that is actually exercised.

## Current workflow

- Build locally through Docker:
  - `cmake -S . -B out`
  - `cmake --build out --target docker-haiku`
- Copy the produced binaries to the Haiku machine over `scp`.
- Run the binaries on the Haiku machine over `ssh`.

The VS Code tasks and launch entries include deploy-and-run workflows.

## What is verified

- `app-example` builds in `build/haiku/examples/01_app_example/app-example`
- `painter-example` builds in `build/haiku/examples/02_painter_example/painter-example`
- both binaries were copied to:
  - `/boot/home/Projects/native/run/`
- both binaries were launched on the Haiku machine and stayed alive during a
  short smoke test

## Why this note exists

This is an environment-specific constraint for a remote target. It belongs in
`docs/notes/` rather than in the normal build book.
