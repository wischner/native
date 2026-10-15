# Native Library Adjustment Instructions

Reviewed 2026-10-15 against `docs/standards/ARCHITECTURE.md`,
`docs/standards/CPP-CODING-STYLE.md` and `AGENTS.md`. This reviews the current
working tree, including the earlier uncommitted shader corrections. It is an
adjustment worklist, not a claim that the implementation is fully compliant.

The requested AmigaOS library is **not implemented**. A reproducible runtime
qualification target exists, but the available compiler/runtime fails its
prerequisites. Do not count that target as a backend, a library build or a
successful port. A00 records the blocking evidence and the implementation gate.

## Repair pass (2026-10-15)

The user excluded A00 (Amiga) from this pass. A01–A18 are repaired as described
below. Their original findings and acceptance instructions remain under their
IDs as historical review evidence; original line numbers are not current source
locations. No Amiga files or qualification behavior were changed by this pass.

| ID | Implemented adjustment | Maintained verification |
| --- | --- | --- |
| A01 | Private object/resource-generation guards stop callback continuations after deletion or recreation; graphics restoration observes drawer lifetime; modal completion follows peer release; signals retain callable/state during mutation and owner deletion | Lifecycle, core signal, native control/input and close tests |
| A02 | Reference-counted display leases distinguish initialized external displays from Native-owned displays; only the final owned lease terminates EGL | Deterministic EGL/GLX fixture, simultaneous real renderers and live replacement |
| A03 | OPEN LOOK locks pipe publication/write/close, validates nonblocking/CLOEXEC setup and clears the wake callback before close | Shared concurrent-delivery tests and OPEN LOOK runtime suite; no Haiku/Linux thread-sanitizer claim |
| A04 | Haiku serializes startup, application messages and window callbacks through one recursive UI gate, always acquired before native looper locks; native retirement is asynchronous; Quit releases the gate before BeAPI thread exit; adopted button state publishes its completed native view before attachment | Native two-window paint/resize/update stress, lifecycle and close tests; no target race-detector claim |
| A05 | Rectangle endpoints return int32; line differences/products use int64; containment/intersection widen before arithmetic | Boundary geometry plus nested window/input suites |
| A06 | Live GLSL installation commits the prepared renderer, output and history together; failed replacement preserves the previous installation | Real renderer/live installation test |
| A07 | All history edges exist before any pass samples them; reset/resize targets are staged; failed ticks preserve committed history | Real later-pass history pixels, reset, resize and failed later-pass retry |
| A08 | Source/viewport/asset/derived extents obey 4096 and device limits; conservative 256 MiB admission counts retained and candidate pixels, readback and staging; uploaded assets have no duplicate renderer CPU copy | Real renderer extent and oversized graph tests; package bounds tests |
| A09 | Vision retires stopped workers to an independent join controller; captured services remain owned through completion | Blocked-worker retirement regression and process/audio/service suites |
| A10 | Font roots and registry discovery live in platform adapters; traversal and inspection remain standard C++ in core | Font enumeration tests and final native Windows registry/font assertions pass |
| A11 | Motif list content/combo widgets and macOS delegates use their owning peers; callback-facing reverse lookups remain | Native Motif/macOS runtime, recreation and teardown tests |
| A12 | Collapsing an unselected tree preserves no selection; replacement expands ancestors of a preserved selected ID | Window API tree regression and collection suites |
| A13 | XView menu/combo items have explicit release ownership; independently reproduced SDK allocations have narrow documented exceptions | Toolkit-only controls and leak-enabled affected runtime tests; policy below |
| A14 | Optional Fontconfig C API replaces fc-match shell execution; SDL2_ttf and bitmap fallback remain optional | Normal SDL2 suite and Docker bitmap-only build with three focused runtime tests |
| A15 | Required file headers restored; shortcut/parser and GL loader bodies moved to cpp; renderer expanded into readable functions with private snake_case entry names | Warning-clean backend rebuilds, header/tab/dense-code scan |
| A16 | Both manuals, standards, matrix, dependency lists and remote notes describe implemented behavior and measured evidence; missing font/tree chapters supplied | Markdown link audit and documented test inventory |
| A17 | Native close requests on all existing backends route through request_close; explicit destruction still forces teardown | Real native cancellation then acceptance on each backend |
| A18 | Peer, lifetime and input allocation belong to the creation rollback transaction; partial native acquisition is unwound and retry remains possible | Fault-injected allocations, throwing hooks, retry and callback teardown regression |

`rect::x2()`/`y2()` intentionally widen their public return type to
`std::int32_t`; rebuild consumers and update exact-return-type declarations.
Public window/graphics private layout also changed; rebuild all consumers.
No OS or toolkit types were added to the public interface.

## Scope and verification

The original inventory covered 605 first-party C++/Objective-C++ files and
119,439 lines. After repairs it covers 628 files and 121,874 lines, plus 62
maintained Markdown documents. Vendor code and attributed assets retain their
provenance exemptions. The mechanical scan finds no missing required file
headers, tabs, dense multi-statement files or broken maintained Markdown file
links. It does not prove every execution path correct. Approximately 50 files
still exceed the advisory 500-line size; splitting them without a demonstrated
responsibility boundary was not justified. The deferred theme review remains.

All nine existing Docker-backed backend targets build. The configured remote
AppleClang/macOS build is the explicit documented SDK exception. Current tests:

| Build selection | Final verification |
| --- | --- |
| SDL2 | 24/24 pass; bitmap-only build also passes 3/3 focused tests |
| X11/Athena | 19/19 pass under Docker/Xvfb |
| OpenMotif | 22/22 pass under Docker/Xvfb, with narrow SDK exceptions below |
| OPEN LOOK | 22/22 pass under Docker/Xvfb, with narrow SDK exception below |
| Window Maker | 18/18 pass under Docker/Xvfb, with narrow SDK exception below |
| GEMix direct | 18/18 pass |
| GEMix/gemd | 18/18 pass; private renderer fixture runs directly without an unnecessary server |
| macOS | 17/17 pass on leia, including real CGL and GLSL history execution |
| Windows | Final native VM run: 16 pass, 2 OpenGL skips; all deployed binary hashes match Docker artifacts |
| Haiku | 18/18 regression executables pass with Mesa softpipe, including real GLSL rendering/context ownership; original-GLSL terminal smoke passes |
| AmigaOS qualification | Excluded from this repair pass; A00 remains open |

Logs are local `build/adjust-*-build.log`, `build/adjust-*-ctest.log` and
platform-specific runtime logs. Final VM evidence is
`build/adjust-windows-final-report.json`, `build/adjust-haiku-runtime.log` and
`build/adjust-haiku-final-hashes.log`. The user approved both deployments; no
deployment approval remains pending. The temporary Windows test task was removed,
preserving the maintained debugger task. Independent reproductions remain scratch
artifacts; maintained regressions are registered in tests/CMakeLists.txt and
the book's build-system test tables. Cross-built test fixtures accept explicit
`--data`/asset paths rather than assuming Linux source paths on a target OS.

### Sanitizer policy and remaining acceptance limits

The repaired XView menu-item leak is not suppressed. SDK-only executables link
no Native code and independently reproduce Motif's two 104-byte selection
records (`XtOwnSelection`/`XtDisownSelection`) and 16-byte notebook navigator
(`_XmSFAddNavigator`), XView's 16-byte PANEL_TEXT drop-interest node
(`win_add_drop_interest`), and WINGs' font-pattern retention (`WMCreateFont`,
6,633 bytes in 134 allocations in the minimal run). Only those SDK paths are
excepted in the affected tests. Their library-owned allocations remain checked.

The Linux EGL/GLX loader keeps vendor dispatch/TLS metadata resident using
RTLD_NODELETE where supported, while releasing actual context/surface/display
leases. Hosted GNU Debug real-renderer tests keep Mesa's software-driver code
mapped with ASan first in LD_PRELOAD; this preserves SDK metadata roots and
symbols across unload rather than suppressing Native leaks. The test uses no
leak suppression or detect_leaks=0. Existing test-specific historical sanitizer
settings elsewhere are unchanged: not every registered test performs leak
checking. Do not describe these results as proof that the upstream SDKs leak
nothing.

Windows desktop OpenGL availability and Haiku's optional Mesa runtime/driver
selection are separate from CPU profile correctness. Full-gallery visual approval, real-device sound and
non-SDL held-key/capture parity remain limited to the evidence in the matrix;
this repair pass does not invent new acceptance. No whole-library race-detector
or exhaustive-path certification is claimed.

## Haiku GLSL implementation follow-up (2026-10-15)

The earlier Haiku factory stub is replaced by a private OSMesa adapter. Core
OpenGL 3.3 is checked at runtime; missing library/entry points and insufficient
versions retain the diagnostic/fallback contract. No public API or native
control kind changes. A thread-local weak activation marker and peer-owned
predecessor states preserve binding order and retire nested peers safely.
Foreign Mesa contexts are rejected before their dispatch changes; this avoids
interpreting another native GL client's private state as an OSMesa context.
The lazily loaded Mesa driver stays resident until process shutdown while
per-peer GL resources/context release normally.

The VM already contains libOSMesa.so.8. Mesa 22.0.5 llvmpipe faults inside its
vertex JIT during the first draw. This is a reproduced runtime fault, separately
from the now-implemented adapter. Native rejects that known unsafe combination
before drawing and names GALLIUM_DRIVER=softpipe. Softpipe is selected by the
launching process, Haiku F5 profile and GLSL CTests; Native never mutates the
process environment. No package installation or global VM change was needed.

All 18 regression executables pass without skips under softpipe, including
native_haiku_glsl_context_tests and the real GLSL pixel/history/replacement
regression. The original eleven-pass GLSL CRT terminal smoke exits zero after
active shader output and successful shell capture. The visible demo also ran
and the user confirmed it works. This is software rendering, not a GPU
performance or exact upstream pixel-parity claim. Current local evidence is
`build/haiku-glsl-build.log`, `build/haiku-glsl-runtime.log`,
`build/haiku-glsl-terminal-smoke.log` and
`build/haiku-glsl-final-verification.log`. All 19 deployed binary hashes
(Vision plus the 18 tests) match the final Docker artifacts.
Original pre-port Haiku skip results above/elsewhere are historical and are
superseded for this supported runtime configuration.

## Original review findings

The following records describe the pre-repair triggers and the acceptance rules
used for this pass. Read them together with the disposition and verification
above; A01–A18 are not additional outstanding implementation work.

## A00 — Blocker: qualify the Amiga C++ runtime before the port

Baseline: AmigaOS 3.1/m68k, Intuition and GadTools, with later 3.x compatibility.
This is a compatibility choice, not a market-share measurement. Optional
ReAction or third-party MUI must not silently become requirements for a 3.1
binary.

Measured image:
`wischner/gcc-m68k-amiga@sha256:7622740b8aaa5c1a5444ec3ccf04e9506f71375300b0946e1382d67c56692cff`.
Its GCC is 16.1.1b, configured `--enable-threads=no`, reporting thread model
`single`. Linking `pthread` does not add the omitted `std::mutex`,
`std::jthread` or stop-aware condition-variable classes to its C++ headers.

Filesystem probes also fail: default newlib has unresolved filesystem/runtime
symbols including `stat` and `_link`; libnix (`nix20`) reaches an unresolved
`__xpg_strerror_r`; clib2 additionally exposes missing `statvfs` and `strtold`.
The ixemul probe cannot read the bundled `errno.h` under the host UID. These
are measured image results, not statements about every Amiga toolchain.

Reproduce through the maintained target:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-amiga-check
```

The target attempts both checks and fails configuration explicitly. Diagnostics
are `build/amiga-toolchain/threads.log` and `filesystem.log`. The same sources
compile, link and run in the hosted SDL2 image, including real cancellation,
stream writes, traversal, metadata, rename and cleanup.

Required work before backend implementation:

1. Repair or supply an Amiga C++ runtime with actual mutex/thread/jthread and
   cancellation support, including the matching libstdc++ ABI. Repair the
   runtime build and its dependencies; do not inject imitation classes into
   `namespace std`, override feature macros or replace synchronization with
   no-op locks. A standalone pthread archive is insufficient evidence.
2. Repair standard filesystem/stream linking and target behavior. Verify Amiga
   volume names and assigns (`RAM:`, `SYS:`, `PROGDIR:`), absolute and relative
   paths, iteration, metadata, errors and atomic publication. Do not replace
   Native's shared `std::filesystem` contract with direct DOS file operations.
3. Publish/pin the repaired image and override `DOCKER_IMAGE_AMIGA` in the host
   control tree. Run both retained probe executables on real AmigaOS or an
   appropriate emulator. `vamos` can qualify supported CLI paths; it does not
   validate Intuition, graphics, timing or device behavior. ROMs/Workbench media
   are not supplied by this repository.
4. Implement backend state only under `lib/native/platforms/amiga/`. Keep the
   public header pure C++, window state peer-owned, and only callback-facing
   resource recovery in `globals.*`. Use Intuition windows/IDCMP, GadTools for
   available standard controls, ASL requesters, native stock fonts and native
   clipboard integration. Record concrete reasons for each unavoidable custom
   control in CUSTOM.md. Do not duplicate another backend's whole dispatcher.
5. Adapt repaint clipping, palette/color allocation and planar/true-color image
   composition without presenting per primitive. Route native input through
   `on_native_*`, preserve stable IDs and modal ownership, and release all
   resources in reverse dependency order. Explicit unsupported audio formats,
   capture modes or GLSL must report failure, not simulated success.
6. Add the actual `docker-amiga` library/Vision build and a separate build tree
   only after these prerequisites work. Keep qualification and UI acceptance
   distinct. Add emulator/target acceptance for painter input, standard gadgets,
   menu commands, dialogs, recreation, clipping, clipboard, text/UTF-8, trees,
   tables, split/tab layout, owned/modal windows and background shutdown.
7. Refresh the complete build/doc/VS Code set in the AGENTS obligation table.
   Do not add an Amiga launch profile pointing at a nonexistent program.

The current change adds only the toolchain qualification project and its VS
Code task. This finding remains open; successful qualification would still
not constitute completion of the requested Amiga port.

## A01 — P1: survive callback-driven object destruction

**Reproduced.** [wnd.cpp](../lib/native/wnd.cpp), lines 433–435, emits the
paint signal and then calls `draw_non_client()` on the same object. Resetting
an owning `unique_ptr<app_wnd>` in that callback produces an ASan
heap-use-after-free at line 435. `signal` keeping its dispatch state alive
protects its own iteration, not the caller's window or graphics peer.

Capture a weak resource lifetime before dispatch and check it without reading
`this` afterward. Audit the analogous post-callback reads in `create()`
(after `on_native_create()`), resize/non-client handling, modal completion,
control action hooks and every backend callback. Keep paint resources alive
through their borrowed transaction or stop using them when destruction occurs;
a window-alive check alone does not protect a freed drawer.

Acceptance: a real handler destroys the native resource, deletes the C++ owner,
or destroys/recreates the same object during create/paint/input/modal callbacks.
Run ASan/UBSan and assert no subsequent chrome paint, stale generation access,
backend cache access or double destruction. Do not merely forbid deletion in
new documentation while existing contracts support callback-driven teardown.
Refresh Architecture 3/5/6, window/paint/signal chapters and Chapters 2/9/18.

## A02 — P1: share EGL display ownership correctly

**Reproduced.** [Linux glsl_context.cpp](../lib/native/platforms/linux/glsl_context.cpp),
line 164, terminates the surfaceless EGL display from every peer destructor.
Create two contexts, enter/leave the second, destroy the first, then enter the
second: activation fails with `EGL_BAD_DISPLAY` (`0x3008`). A live effect
replacement and two shader views can therefore invalidate surviving peers.
Per-peer contexts and surfaces do not imply per-peer display lifetime. See
[Khronos EGL initialization/termination semantics](https://registry.khronos.org/EGL/specs/eglspec.1.4.pdf).

Give the shared display an explicit lifetime owner; terminate it only after
all Native contexts/surfaces are gone, accounting for any display borrowed from
another EGL user. Keep the existing EGL/GLX caller restoration. Do not fix this
by leaking every context or removing orderly teardown without an owner.
Acceptance: two live views, replacement with an old live renderer, failed
candidate creation, teardown in either order and continued surviving rendering
against both deterministic fixtures and real Mesa EGL. Refresh the image
shader chapter, Architecture 21 and the per-backend feature evidence.

## A03 — P1: synchronize OPEN LOOK wakeup teardown

**Source-review finding.** [OPEN LOOK app.cpp](../lib/native/toolkits/openlook/app.cpp),
lines 50–76 and 89–110, shares `posted_pipe` between producer wakeups and UI
teardown without a lock. `app::post()` invokes its captured wake outside the
queue lock, so clearing `loop_wake` does not exclude an in-flight call. The
worker can race descriptor close/reuse; nonblocking setup errors are ignored,
and this pipe lacks close-on-exec.

Use scoped pipe ownership and a mutex around publishing, writing and closing
the wake target, as the X11/Motif/WINGs adapters already do. Validate all fcntl
operations and close both ends on setup failure. Prevent writes after receiver
closure without globally changing SIGPIPE policy. Acceptance: repeatedly stop
loops while producers post, exercise descriptor reuse/setup failure, and run
a threading sanitizer on the portable handoff where supported. Refresh the
application/background chapters and OPEN LOOK runtime evidence.

## A04 — P1: define and enforce Haiku UI serialization

**Source-review finding; not race-detector-verified on Haiku.**
[Haiku app.cpp](../lib/native/platforms/haiku/app.cpp) drains posted work on
`BApplication`; [native_window.cpp](../lib/native/platforms/haiku/native_window.cpp)
delivers paint/input/geometry callbacks from `BWindow` loopers. Shared window
fields, signals, peer maps and shader state have no common serialization
boundary across those callbacks. Acquiring a native window lock in selected
apply methods does not protect portable state mutations before the apply call.

Choose a consistent dispatch/locking policy compatible with the public UI
contract. Serialize shared C++ state and callbacks across both routes; include
multiple windows and destruction. Do not sprinkle locks around signals while
calling application code under unrelated native locks, which risks deadlock.
Acceptance: queue source updates while painting/resizing/closing several
windows, assert the intended callback thread/serialization, and verify no
post-destroy access or lock-order cycle. Refresh Architecture 3/5/20 and the
Haiku note; retain the issue until target evidence exists.

## A05 — P1: perform geometry calculations before narrowing

**Reproduced.** [geometry.cpp](../lib/native/geometry.cpp), lines 30–37 and
71–78, narrows coordinate differences and rectangle endpoints to `int16_t`.
The line `(-32768,-32768) → (32767,32766)` incorrectly contains
`(-32767,-32766)`. `rect(30000,0,10000,10)` incorrectly excludes `(31000,1)`.
These inputs fit the public member types. Int32 cross-products are also too
small for products of full-range coordinate differences; use int64 arithmetic.

Compute deltas/endpoints/intersections in sufficient width. Explicitly decide
how unrepresentable endpoint return values are handled without silently
changing the public ABI. Audit layout, nested origins, clipping and chrome
sums for the same pattern. Acceptance: extreme signed coordinates, large
unsigned dimensions, disjoint/empty intersections, collinearity and actual
nested clip/input behavior. Refresh geometry/window standards and chapters,
and every API example affected if the endpoint contract changes.

## A06 — P2: retain the successfully prepared GLSL renderer

**Reproduced.** [shader_view.cpp](../lib/native/shader_view.cpp), lines 49–83,
creates a temporary GLSL renderer during live installation, keeps its output,
then resets the installed renderer. With a created view and submitted source,
`set_effect(original_glsl_package)` succeeds and status is active while
`get_shader_capabilities().glsl_profile_1` is false. Source-update mode can keep
that mismatch indefinitely; the next source unnecessarily recompiles the graph.

Prepare the candidate renderer/output/history together, then commit all of
them transactionally. Keep the previous installation intact until preparation
succeeds. Acceptance: live successful replacement reports coherent status and
capabilities immediately, the next source reuses programs, temporal state is
consistent, and a failed replacement preserves the old renderer/output.
Include A02's real-display replacement case. Refresh Architecture 21 and both
shader chapters.

## A07 — P2: initialize the entire GLSL history graph before sampling

**Source-review finding.** [glsl_renderer.cpp](../lib/native/glsl_renderer.cpp),
render loop, allocates/resets each pass's history only when that pass executes.
The decoder permits a pass to sample the retained output of a later pass.
On its first tick that history texture can still be zero/unallocated; on a
reset it can still contain the old frame until the later pass is visited.

Resolve and allocate/clear all current/history targets before executing any
pass. Preserve previous completed history until the whole candidate tick
succeeds; define failure handling for partially written scratch targets.
Acceptance: a first pass samples later-pass history on first tick, after resize,
clear, a long gap and retry following an injected later-pass failure. Assert
actual transparent-black and committed-history pixels, not just active status.
Document CPU and GLSL history behavior separately where formats differ.

## A08 — P2: specify and verify GLSL limits and storage

**Source-review finding.** The GLSL renderer checks viewport/pass extents and
estimates source/current/history bytes, but does not apply the CPU profile's
complete source-extent gate. The estimate excludes persistent packaged texture
storage and the returned CPU image. Its targets are `GL_RGBA8`, while the CPU
executor uses linear premultiplied float4 intermediates. Current manuals mostly
describe the latter and must not imply GPU float-target equivalence.

Document the actual separate profile contracts: CPU packages are capped at
one MiB; the overall/GLSL decoder accepts eight MiB, up to sixteen GLSL passes,
with different reduction/alignment, textures and uniform records. Keep the CPU
one-MiB gate; do not reduce the GLSL limit blindly (the current terminal package
is 2,126,949 bytes). Validate every source and target against advertised limits
and the device, and define a conservative allocation estimate including retained
candidate/old output. Add overflow, maximum-boundary and failure tests. Extend
capabilities only if the existing fields cannot describe the real renderer;
keep all device types private. Refresh Architecture 21, both shader chapters,
the packaging tool documentation and feature matrix in the same change.

## A09 — P2: keep potentially slow joins outside window callbacks

**Source-review finding.** [infinity_window.cpp](../src/infinity_window.cpp),
`stop()` and worker restarts, and [vision_retro_terminal.cpp](../src/vision_retro_terminal.cpp),
`on_native_destroy()`/`stop()`, join workers directly from UI lifecycle paths.
A child cancellation timeout or slow worker can block event delivery. Closing
weak UI delivery first prevents stale callbacks but does not make a join fast.

Keep workers in an owning controller whose shutdown can finish independently
of UI dispatch. Request cancellation on the UI; complete the join outside a
window callback while preserving every captured object's lifetime. Do not
replace joining with a detached worker borrowing the destroyed window.
Acceptance: close/restart during a slow child, audio backpressure and a pending
result; measure bounded UI responsiveness and assert worker completion without
requiring another UI turn. Refresh Architecture 20 and both service chapters.

## A10 — P2: move OS font discovery behind the platform layer

**Confirmed layering violation.**
[installed_fonts.cpp](../lib/native/installed_fonts.cpp), line 21 and registry
helpers, includes Win32 APIs in the shared C++ layer. The umbrella/public
headers themselves passed the native-header scan; those are different checks.

Extract platform font roots/registry discovery into a private platform adapter.
Keep portable inspection, traversal, sorting and resource reads in the core
using std::filesystem/streams. Keep system folder/registry calls isolated and
allowed in the Windows layer. Acceptance: portable enumeration tests plus a
Windows registry/font-discovery test, and no OS includes in shared core files.
Refresh Architecture 1/10, AGENTS, layering and font chapters.

## A11 — P2: finish peer-owned state migration

**Confirmed source mismatch.** Motif `list_content_bindings` and
`combo_box_bindings` in [globals.h](../lib/native/toolkits/openmotif/globals.h)
are process-wide object-to-widget maps used only to recover a control's state.
macOS `delegate_bindings` in [globals.h](../lib/native/platforms/mac/globals.h)
is similarly object-to-delegate state. These do not need a second bidirectional
registry when the owning window already has a peer.

Move these state records into typed peers. Preserve actual callback-handle
reverse lookups: file-panel bindings that recover a dialog from an async panel
are not automatically redundant just because their template parameters are
written object-first. Audit each use before conversion. Acceptance: recreation,
partial creation failure, owner teardown, callback invalidation and stable native
resource counts. Refresh Architecture 2, layering/windows chapters, CUSTOM.md
and each changed backend's feature evidence.

## A12 — P2: preserve empty tree selection when collapsing a branch

**Reproduced.** [tree_view.cpp](../lib/native/tree_view.cpp), `set_expanded()`,
changes selection to the collapsed ID whenever the old selected ID is absent
from visible rows, including the no-selection sentinel. Construct an expanded
root ID 1 with child ID 2 and no selection; `set_expanded(1,false)` produces
selected ID 1 rather than 0.

Only move selection when an actual selected descendant becomes hidden. Also
review replacement with a preserved ID beneath newly collapsed ancestors.
Acceptance: no selection, a selected descendant, a selected unrelated branch,
invalidated IDs and silent programmatic changes; assert stable IDs and exact
user signal counts on native and painted adapters. Refresh Architecture 15,
the tree patterns chapter and Programming Native Chapter 13.

## A13 — P2: resolve failing sanitizer suites without hiding them

**Reproduced.** Motif's runtime suite reports 328 bytes in four allocations
through Xt selection/focus paths; full allocation stacks reach test traversal
and Native's `select_all_native()`. OPEN LOOK reports 2,832 bytes in thirteen
allocations through `menu_create_item_internal` and `win_add_drop_interest`.
Functional assertions print success before LeakSanitizer returns failure.

Audit Native's actual destroy/disown paths and minimal toolkit-only cases.
Free owned resources. Only if a retained toolkit allocation is independently
proven unavoidable should a narrow, documented, allocation-specific suppression
be considered. Do not apply `detect_leaks=0` globally or relabel either suite
as fully passing. Acceptance: the same committed CTest command succeeds under
the stated sanitizer policy, or the matrix retains the specific exception.
Refresh tests/CMakeLists, build test tables, backend notes and feature matrix.

## A14 — P2: remove shell-based stock-font discovery

**Confirmed implementation pattern, no injection reproduction.**
[SDL2 font.cpp](../lib/native/toolkits/sdl2/font.cpp), lines 59–78, constructs an
`fc-match` shell command and reads it through `popen`/C streams. Current callers
use fixed internal patterns, so this finding does not assert an exploitable
user-input injection. It still couples font initialization to a shell/external
utility and performs synchronous process work on the UI initialization path.

Prefer a private Fontconfig/native discovery adapter with explicit dependency
and fallback, or another justified shell-free route; retain SDL2_ttf optionality
and bitmap fallback. Do not add a public family-to-shell API or silently replace
application-supplied portable fonts. Acceptance: stock roles with and without
Fontconfig/SDL2_ttf and no shell execution in that path. Refresh font/build
chapters and dependency lists if the link closure changes.

## A15 — P3: restore mandatory headers and implementation separation

Six first-party files lack the required descriptive MIT/copyright header:

- `lib/native/menu_shortcut.h`
- `lib/native/platforms/haiku/split_view.cpp`
- `lib/native/platforms/windows/split_view.cpp`
- `lib/native/toolkits/openlook/tab_view.cpp`
- `lib/native/toolkits/wmaker/split_view.cpp`
- `lib/native/toolkits/wmaker/tab_view.cpp`

Add the real file header. Move the non-template shortcut parsing/decorating
bodies from `menu_shortcut.h` into a `.cpp` with corresponding CMake wiring;
they are not tiny constexpr helpers. Review `glsl_api.h`'s loader constructor
under the same rule. Reformat `glsl_renderer.cpp`'s compressed multi-statement
control flow into small, explicit functions. Preserve required native symbol
spellings inside API lookup strings. No tabs were found in first-party C++.

Fifty scanned files exceed the approximate 500-line guidance. This is a
refactoring signal, not an automatic defect: split by responsibility only when
it improves readability and testability. Start with window lifecycle/dispatch,
collection adapters and code rendering. Respect the existing deferred theme
review; do not implement interface segregation solely to shrink a file count.
Acceptance: full warnings, unchanged behavior and same backend test coverage.

## A16 — P2: make books and feature claims match executable profiles

The older front-of-matrix CPU-only/approximation text was stale after the
original GLSL renderer landed. The review refreshes the current explanation,
adds the previously missing font and tree internals chapters, and distinguishes
historical CPU tests from GLSL execution. Amiga qualification is documented as
a failing prerequisite, not a supported build selection.

Retain this distinction in future edits: source review, cross-linking, inert
core tests, real toolkit tests, shader execution, interactive visual acceptance
and measured performance are different evidence. Update current summaries and
tables when a later dated result supersedes them; appending a success paragraph
does not correct contradictory current prose. Reconcile the universal Docker-build rule with the already documented and
explicitly used remote macOS workflow; do not silently turn one session
exception into a general unrecorded rule. Fix broken external-checkout
links or label the referenced proposal as external, rather than requiring a
sibling repository for manual navigation.

## A17 — P2: unify the documented native-close policy divergence

Architecture Section 5 currently documents that SDL2 alone routes native close
requests through `app_wnd::request_close()`. Other adapters directly destroy
or report native destruction: for example X11's WM-delete handler and the
AppKit/Haiku window delegates. This is a documented limitation, not an
undisclosed exception to that section, but an application overriding the close
policy cannot use the same unsaved-work behavior on every target.

Route close requests through the policy before committing native destruction,
including cancellable and asynchronous confirmation. Keep explicit `destroy()`
as the forced resource operation. Acceptance: an override keeps the window
alive after close, later accepts destruction, and cancels without losing input,
focus or owned windows on every backend. Refresh Architecture 5 and all
window/owned-dialog/close examples together; preserve existing F5 routes.

## A18 — P2: roll back allocation failures during window creation

**Reproduced.** [wnd.cpp](../lib/native/wnd.cpp), lines 328–334, marks the
window created and allocates its peer, lifetime token and input/mouse state
before entering the rollback `try` block. An injected `std::bad_alloc` on
the first allocation leaves `get_created()` true with no peer. A subsequent
`create()` silently returns without calling the native creation hook. The
isolated Docker probe reports `bad_alloc=1 created_after_failure=1
native_create_calls_after_retry=0`; evidence is
`build/review_create_rollback.log`.

Make the complete acquisition sequence transactional, including all shared
allocations. Invalidate any acquired lifetime token before peer teardown,
restore the uncreated state on failure and preserve the original exception.
Audit partial native creation and cached-property application failures for
resource and callback-binding cleanup; the allocation reproduction does not
establish that every backend already unwinds those later failures correctly.
Keep this separate from A01's callback-driven destruction continuation.

Acceptance: inject failure at each shared acquisition point, verify no native
create signal or leaked peer/state, and retry successfully on the same C++
object. Exercise a throwing backend hook and a failure after native resource
creation without leaving callback bindings behind. Refresh Architecture 5,
the window/painting patterns and the control-lifecycle tutorial with the fix.

## Adjustment order and completion gate

Resolve A00 in the toolchain before claiming an Amiga backend. Implement A01
and A02 before large renderer refactors, then A03/A04/A05/A18. Add the semantic
regressions with those fixes. Follow with A06–A14 and the focused style work.
Each implementation change must carry its matching documentation changes;
do not postpone books until a final formatting commit.

A future completion report must identify the reviewed revision, tests and
runtime conditions, resolved findings, remaining limitations and actual Amiga
artifacts. This worklist is not such a completion report. Do not claim the
whole library is compliant while the reproduced findings remain open.


## Architecture and documentation coverage

Each row identifies the corresponding adjustment or the reviewed boundary;
"no additional finding" is not a blanket proof of correctness.

| Architecture section | Review focus and required documentation |
| --- | --- |
| 1: Structure | A00/A10; preserve pure public C++, shared filesystem/streams and platform isolation; layering chapter |
| 2: Bindings | A11; distinguish real callback reverse lookups from object-to-state caches; peer/layering chapters |
| 3: Signals | A01/A03/A04; synchronous dispatch and receiver lifetime; signal/input chapters |
| 4: Properties | A06/A12; cached values, silent setters and exact native notifications; properties and control chapters |
| 5: Windows | A01/A04/A05/A17/A18; creation rollback, resource generations, hierarchy and ownership; windows and Chapters 4/5/9 |
| 6: Painting | A01/A05; borrowed drawer lifetime, clip/state restoration and one logical notification; painting and Chapter 2 |
| 7: Theming | A15; native palette/metrics isolation and complete protected stages; theme/drawing chapters; keep deferred REFACTOR criteria |
| 8: Application | A03/A04/A09; event-loop wake, exit and controller shutdown; application and background chapters |
| 9: Screens | Coordinate/extent boundaries feed A05; native types stay private; screens chapter |
| 10: Fonts | A10/A14; byte ownership, shared rasterization and native stock roles; new font patterns chapter and Chapter 10 |
| 11: Clipboard | Native typed transaction adapters and standard streams inspected; no additional confirmed finding; clipboard chapter |
| 12: Text editing | A01/A04/A13 lifetime/threading and native resource cleanup; UTF-8/selection/clipboard chapter |
| 13: Tables | Stable IDs/models, selection/scrolling and native vs painted adapters inspected; A01/A04/A05 still apply; table chapter |
| 14: Code editing | Source model and render/input split inspected; A01/A05/A15 apply; code-edit chapter |
| 15: Classic trees | A12 and native ID preservation; new tree patterns chapter and Chapter 13 |
| 16: Split views/tabs | Native host/content transitions inspected; A01/A15 apply; split/tab chapters |
| 17: Input/dialogs/chrome | Native panel callbacks, ownership and popup delivery inspected; A01/A05/A17 apply; input/chrome chapters |
| 18: Panels | Non-owning child contract and explicit native resource lifecycle; A01/A04 apply; panel/canvas chapters |
| 19: Canvases | Paint clipping/chrome/32-bit content and inherited shaders; A01/A05 apply; panel/canvas chapters |
| 20: Services | A00/A03/A04/A09; bounded admission, callback threads and independent shutdown; both service chapters |
| 21: Shaders | A02/A06/A07/A08; resource/display lifetime, replacement, history, limits and capability truth; both shader chapters |

The two manuals' indexes and all maintained chapters were inventoried against
the standards/documentation map. The new font/tree chapters fill the two
explicit internal-book gaps. Future semantic changes must update the linked
chapters in the same change; do not fill manuals with unimplemented designs.
Chapter-specific application snippets remain in Programming Native, not a new
examples source tree. No generated API documentation was added.

## Backend review disposition

Every existing backend retains its native control choices and private peer
boundaries. Shared repairs apply across all toolkits; native cancellation,
callback continuation and presentation paths were adjusted in their own
layers. Build and runtime coverage is recorded above and in FEATURE-MATRIX.md.
Amiga remains excluded, with no speculative control classifications.

## Contributor workflow for each adjustment

Use one focused change per semantic repair. Quote the finding ID, exact trigger,
resolved behavior and test evidence in the change description. Add regressions
that fail on the observed defect, rather than tests that merely mirror the new
implementation. Do not combine a lifetime fix with wholesale formatting of all
platforms. Carry forward unrelated working-tree changes without overwriting them.

Run the affected Docker CMake targets, then their meaningful runtime suites.
Use the configured remote macOS workflow and native Windows/Haiku targets when
needed; Wine is not the Windows acceptance route. Record unavailable hosts and
unrun cases explicitly. A compile or core model test never substitutes for a
native callback/paint/clipboard acceptance check.

Refresh CUSTOM.md kind/audit/reason entries and FEATURE-MATRIX.md for control
implementation changes. If runtime status changes, update README.md,
BOOK-OF-NATIVE.md and BACKEND-OPEN-ISSUES.md. Refresh build chapters, this
repository's AGENTS map and VS Code integration for new targets/paths/images.
Every new/moved test belongs in tests/CMakeLists and the build-system test table.
Keep the production TODO inventory synchronized if markers are introduced;
none were found by the token-based production scan in this review.
