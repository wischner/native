# Backend Open Issues

This note lists backend-level open issues that are real today.

## Runtime status

- Runtime-tested in the current workflow:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif in the `Tribblix-CDE` KVM guest
  - Linux OPEN LOOK/XView in the `Tribblix-OpenLook` KVM guest
  - Linux Window Maker/WINGs in the `Bookworm-WindowMaker` KVM guest
  - Linux GEMix in Docker with local rasta
  - MS Windows binaries built with MinGW and run through Wine
  - Haiku binaries built through Docker, copied to a Haiku machine, and run
    there
  - Apple
- Still in progress:
  - other toolkit ports not listed above as runtime-tested

## Current open issues

- GEMix's local-rasta review exercises button feedback, check/radio parity,
  text editing and copy/paste after file open/save, overlapping modeless
  windows, modal close, extreme layout resize, collection disclosure/icons,
  table borders and thumb dragging, splitter dragging, combo popups, rotated
  stock text, four-edge tabs, and status chrome. Five CTest executables pass,
  including `native_gemix_runtime_tests`. These checks use the matching GEM
  runtime fixes in the sibling GEM source tree: staged rasta presentation,
  clipped AES pattern drawing, bitmap-copy damage tracking, and synchronous
  file-selector owner restoration. Rebuilding Native alone against an older
  GEM SDK does not include those runtime fixes. AES/VDI interfaces are unchanged.
  The reported Copy-field crash did not reproduce in the reviewed runtime;
  file open/save followed by copy/paste completed without a sanitizer report.
  On this review host, the repaired runtime and resources are installed in
  the local `wischner/gcc-x86_64-gemix:latest` image, also tagged
  `open-review-20260905`. This includes the follow-up fix that paints the
  desktop on AES workstation startup; its fresh-launch framebuffer regression
  failed before the fix and passes afterward. The preceding review image is
  retained under `desktop-review-20260905` and `native-review-20260905`, and the
  original image as
  `wischner/gcc-x86_64-gemix:before-native-review-20260905`. No image was
  pushed to a registry. The separate gemd launch profile preserves the original
  direct launch/task configuration.
  Other hosts must build the matching sibling GEM changes into their SDK;
  pulling the registry image is not evidence that these fixes are included.

- The GEMix interaction follow-up verifies typing, button and keyboard
  copy/paste, selector button inversion/release, immediate split-pane list
  selection, inactive left-tab separators, complete title restoration, and
  removing the menu and popup on Exit. The ordinary fields worked with
  Editing enabled; the read-only state observed during interleaved manual
  input intentionally rejects typing/paste. Focused editor clipboard commands
  now precede menu accelerators. Wheel scrolling remains unavailable in local
  rasta: its input broadcaster does not forward SDL wheel events. No AES/VDI
  extension was introduced to work around that missing input.

- The libgem/gemd transport is also runtime-tested with local rasta.
  Its matching sibling GEM runtime fixes missing query/selector wrappers,
  malformed-request checks and menu lifetime on client disconnect. The
  local Docker `open-review-20260905` image contains these changes; a registry
  image is not evidence of the same runtime. This verifies Native's call set
  using the same-host RPC ABI, not every AES/VDI entry point or a network ABI.
  The input follow-up fixes zero mouse state on queued redraw replies, hosted
  source-editor navigation codes and LF Enter, and intermediate desktop
  presentation during window close. Viewer-driven typing, both text modes,
  clipboard buttons and source-editor rendering/navigation pass. Five Native
  CTests pass in each transport; all three hosted GEM tests pass, including the
  held-button/redraw and keyboard-modifier RPC regression. A wholly blank source
  editor did not reproduce in the restarted session; its initial/reopen drawing
  was checked without claiming a separate diagnosed blank-editor defect.
  The opening follow-up fixes AES desktop/frame occlusion routing. Modal and
  modeless windows, fully or partly overlapping their owner, preserve covered
  pixels until client painting. The intermediate-frame regression fails with
  `input-review-20260905` and passes with `open-review-20260905`.

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
    Window Maker's alert panel was driven in its Bookworm guest. The same
    guest was used under GDB for direct text copy/paste, both combo modes,
    native file-panel movement, modal and modeless close, collection teardown,
    repeated extreme layout resize, native splitter dragging with complete
    pane borders, full-width editable-combo popup presentation with a centered
    inset arrow, semantic alert
    badges, menu-title hover switching, and top, bottom, and right tab
    placements, including seam-free selected tabs, single inactive
    bottom/right page edges; the owner
    remained responsive and exited normally afterward.
  - The macOS implementation, including
    `lib/native/platforms/mac/surfaces.mm`, compiles and launches through the
    remote `leia` workflow. That SSH session cannot access WindowServer for a
    screenshot or interactive visual pass.

## Why this note exists

The book should describe current behavior in plain engineering language.
Open backend status belongs here so it can be tracked without turning the book
into a roadmap.
