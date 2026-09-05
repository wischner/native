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

- The X11 follow-up (2026-09-05) reproduces the Input/Window Chrome exception
  from graphics lookup before realization. Created-but-unshown graphics now
  work, also synchronizing initial table/collection metrics with painting.
  A private Athena Form specialization prevents content-driven shrinkage and
  competing layout. Full-row lists, complete borders, scrollbar separators,
  filled table viewports and live background-colored splitters have regression
  coverage. Six X11 CTests pass under Xvfb; visual checks now run in monochrome
  TWM inside Xephyr. The installed Xaw library and F5 profiles are
  unchanged. Seven SDL2 CTests pass after the shared tab-label contrast fix.
  The second X11 pass adds full-width, bordered combo menus with single-click
  selection and text synchronization, private splitter pane hosts preserving
  borders, an explicit divider grab including its leading pixel, monochrome
  collection imagery, accordion top rules, white table rows, open selected-tab
  joins and copied alert artwork. Menu titles/popups retain their own borders
  without an additional full-width bar rule. The current pass adds ordinary
  combo hover highlighting, centered native message buttons, a small splitter
  grip and actual Xaw scrollbars for tables, icon grids, trees and canvases.
  Native scrollbar tests cover exact endpoints, real middle-button dragging
  and visibility changes. The server-input
  regression also covers menu teardown before Xt destroys the callback owner.

- GEMix's local-rasta review exercises button feedback, check/radio parity,
  text editing and copy/paste after file open/save, overlapping modeless
  windows, modal close, extreme layout resize, collection disclosure/icons,
  table borders and thumb dragging, splitter dragging, combo popups, rotated
  stock text, four-edge tabs, and status chrome. Six CTest executables pass,
  including `native_gemix_runtime_tests`. These checks use the matching GEM
  runtime fixes in the sibling GEM source tree: staged rasta presentation,
  clipped AES pattern drawing, bitmap-copy damage tracking, and synchronous
  file-selector owner restoration. Rebuilding Native alone against an older
  GEM SDK does not include those runtime fixes. AES/VDI interfaces are unchanged.
  The reported Copy-field crash did not reproduce in the reviewed runtime;
  file open/save followed by copy/paste completed without a sanitizer report.
  On this review host, the repaired runtime and resources are installed in
  the local `wischner/gcc-x86_64-gemix:latest` image, also tagged
  `input-dialog-review-20260905`. This includes the follow-up fix that paints the
  desktop on AES workstation startup; its fresh-launch framebuffer regression
  failed before the fix and passes afterward. The preceding review image is
  retained under `security-review-20260905`, `open-review-20260905`, `desktop-review-20260905` and
  `native-review-20260905`, and the
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
  CTests pass in each transport; all five hosted GEM tests pass, including the
  held-button/redraw and keyboard-modifier RPC regression. A wholly blank source
  editor did not reproduce in the restarted session; its initial/reopen drawing
  was checked without claiming a separate diagnosed blank-editor defect.
  The opening follow-up fixes AES desktop/frame occlusion routing. Modal and
  modeless windows, fully or partly overlapping their owner, preserve covered
  pixels until client painting. The intermediate-frame regression fails with
  `input-review-20260905` and passes with `open-review-20260905`.

- The sibling GEM security follow-up (2026-09-05) adds peer/ownership checks,
  nonblocking bounded RPC transport, per-client VDI state and visible-region
  clipping, menu graph validation, lock/disconnect cleanup and cooperative
  standard-panel waits. Three separate Native processes were displayed in
  local Rasta; closing one left the others running. Real-server regressions
  additionally check three-client keyboard focus, menu/desktop-owner exit,
  malformed/stalled peers, and hostile font/framebuffer inputs. All five
  Native CTests pass in both direct and proxy modes against the local
  `security-review-20260905` runtime. AES/VDI public interfaces and F5 profiles
  are unchanged. This is a same-user shared desktop, not a sandbox: scrap,
  cursor and standard-panel input remain shared, and the private transport
  is not a network ABI. The sibling GEM `docs/SECURITY.md` records limits and
  remaining trust assumptions; the local image has not been registry-pushed.

- The renewed GEMix dead-field/copy-paste report did not reproduce in fresh
  direct or proxy viewer sessions. The new `native_gemix_input_tests` sends
  actual Rasta packets through the full application loop, including rapid
  focus clicks, both text modes, clipboard buttons and Ctrl+A/C/V, and input
  after modal/modeless and file-selector closure. All six Native tests pass
  in each transport; exact failing launch/interaction steps remain needed.
  The modal-frame changes are verified separately: untitled hosts reserve
  four client-excluded pixels for the `1011` enclosure, and AES alerts use
  the same edge pattern. The matching GEM runtime fixes `WF_KIND` queries
  that incorrectly returned work coordinates. Public AES/VDI APIs and F5
  configurations are unchanged.

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
