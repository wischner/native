# Backend Open Issues

This note lists backend-level open issues that are real today.

## Runtime status

- Runtime-tested in the current workflow:
  - Linux X11
  - Linux SDL2
  - MS Windows binaries built with MinGW and run through Wine
- Implemented but not yet runtime-tested in the current workflow:
  - Haiku
  - Apple
- Still in progress:
  - other toolkit ports not listed above as runtime-tested

## Current open issues

- Haiku runtime is not yet exercised in the current workflow.
  Source status appears in the build and book documentation, but runtime behavior is still unverified.
- Apple runtime is not yet exercised in the current workflow.
  Platform code exists, but it is not part of the current verified build/run path.
- Other toolkit ports are still work in progress.
  They should stay out of the normal user workflow until they are built and exercised.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.
