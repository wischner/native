# Backend Open Issues

This note lists backend-level open issues that are real today.

## Runtime status

- Runtime-tested in the current workflow:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif in the `Tribblix-CDE` KVM guest
  - Linux OPEN LOOK/XView in the `Tribblix-OpenLook` KVM guest
  - Linux Window Maker/WINGs in the `Bookworm-WindowMaker` KVM guest
  - MS Windows binaries built with MinGW and run through Wine
  - Haiku binaries built through Docker, copied to a Haiku machine, and run
    there
  - Apple
- Still in progress:
  - other toolkit ports not listed above as runtime-tested

## Current open issues

- Other toolkit ports are still work in progress.
  They should stay out of the normal user workflow until they are built and
  exercised.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.
