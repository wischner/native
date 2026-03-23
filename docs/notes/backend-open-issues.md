# Backend Open Issues

This note lists backend-level open issues that are real today.

## Runtime status

- Runtime-tested in the current workflow:
  - Linux X11
  - Linux SDL2
  - MS Windows binaries built with MinGW and run through Wine
  - Haiku binaries built through Docker, copied to a Haiku machine, and run there
- Build-tested but not yet runtime-tested in the current workflow:
  - Linux OpenMotif
  - Linux GNUstep
- Implemented but not yet runtime-tested in the current workflow:
  - Apple
- Still in progress:
  - other toolkit ports not listed above as runtime-tested

## Current open issues

- Haiku runtime is exercised in the current workflow, but source-level
  debugging is still blocked in practice on the tested Haiku machine.
  Remote `gdb` can start, but source-level stepping is not reliable enough for
  a supported VS Code debug workflow.
- Apple runtime is not yet exercised in the current workflow.
  Platform code exists, but it is not part of the current verified build/run path.
- Other toolkit ports are still work in progress.
  They should stay out of the normal user workflow until they are built and exercised.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.
