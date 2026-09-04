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
- `panel` and `canvas` are implemented on every enabled backend, but their
  verification depth differs:
  - SDL2 has runtime assertions through `native_surface_runtime_tests`,
    covering layout, nesting, scrollbar thresholds, scrolling, pointer
    routing, and a destroy/recreate cycle under ASan/UBSan.
  - X11, OpenMotif, OPEN LOOK, Window Maker, GEMix, Windows, and Haiku are
    compiled through their Docker targets and share the portable geometry,
    clamping, and painting code the SDL2 run exercises. Their own hosts have
    not been driven interactively.
  - OpenMotif and OPEN LOOK split hosts were driven interactively in their
    Tribblix guests, including divider dragging. Haiku input/chrome controls
    were driven interactively after deployment, including both combo modes.
  - Windows table and native file-open hosts were driven through Wine, and
    Window Maker's alert panel was driven in its Bookworm guest.
  - The macOS implementation, including
    `lib/native/platforms/mac/surfaces.mm`, compiles and launches through the
    remote `leia` workflow. That SSH session cannot access WindowServer for a
    screenshot or interactive visual pass.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.
