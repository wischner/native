# Build System

This chapter describes the build structure that exists in the repository today.
It focuses on the current CMake workflow and the separation between the host
control tree and backend-specific build trees.

## Overview

### GEMix transport selection

Both GEMix build targets use the published `wischner/gcc-x86_64-gemix:latest`
image, which already contains the GEM `v1.0.0` SDK built from commit
`0df463d9ff4b4cbbdd0612ce700c9e11e5e33661`: headers and Rasta-backend
libraries in `/usr/local` with `gemix-*` pkg-config files, plus `gemd`,
`resgen`, the GEM resources, and the recorded revision in
`/opt/gemix/share/metadata/gem.version`. Docker pulls the image on first use;
no local SDK image build and no sibling GEM checkout is needed.

`docker-gemix` builds the direct AES/VDI variant in `build/linux-gemix/`.
`docker-gemix-gemd` uses the same Docker image, sets `GEMIX_USE_GEMD=ON`,
and builds in `build/linux-gemix-gemd/`. Its executable links `libgem`, not
`libaes` or `libvdi`; the separate `gemd` process owns AES/VDI and Rasta.
The SDK headers come from `gemix-aes` and `gemix-vdi` pkg-config modules,
with version 1.0.0 or newer required. The image builds GEM in Release mode;
Native Debug builds retain their own ASan/UBSan instrumentation.

The image must supply matching proxy libraries and `/opt/gemix/bin/gemd`.
`scripts/gemix/gemd-session.sh <command> [args...]` starts a private server,
exports its `GEMD_SOCKET`, runs the command, and cleans up only that server.
Its inherited framebuffer, dimensions and resource environment configure the
server. Client and server share filesystem paths for file selectors and scrap.
The proxy debug configuration invokes this wrapper around GDB through the
Rasta session runner. The direct build target remains available. Server logs go to stderr,
not GDB's MI output. GEMix runtime and window API tests also use this wrapper
when built with the proxy option, retaining their isolated test framebuffers.

### Common build flow

The project uses CMake as its build entry point.

At the root, CMake does three things:

- sets the project language and C++ standard
- builds the `native` library and the `vision` program
- exposes Docker-backed backend targets

The top-level build flow is:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-gemix-gemd
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
cmake --build build/cmake --target docker-openlook
cmake --build build/cmake --target docker-wmaker
cmake --build build/cmake --target docker-win
cmake --build build/cmake --target docker-haiku
```

This is the current reproducible path used for Linux, Windows, and Haiku
cross-builds.

The library source list selects one implementation of checkbox painting:
`check_drawing.cpp` retains the default stages for SDL2 and other backends;
Haiku instead supplies all stages in `platforms/haiku/check.cpp`. The common
`check.cpp` contains only the portable property/event model.

## Build directories

The repository uses separate build directories per backend:

- `build/cmake/`
  - host-side CMake control tree
  - contains generated top-level targets such as `docker-gemix`, `docker-x11`,
    `docker-sdl2`, `docker-openmotif`, `docker-win`, and
    `docker-openlook`, `docker-wmaker`, and `docker-haiku`

- `build/linux-x11/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=X11`

- `build/linux-gemix/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=GEMIX`

- `build/linux-sdl2/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=SDL2`

- `build/linux-openmotif/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=MOTIF`

- `build/linux-openlook/`
  - toolkit build tree configured with `TOOLKIT=OPENLOOK`

- `build/linux-wmaker/`
  - toolkit build tree configured with `TOOLKIT=WMAKER`

- `build/windows-mingw-w64/`
  - platform build tree for the Windows MinGW-w64 target

- `build/haiku/`
  - platform build tree for the Haiku target

These build trees are kept separate so backend-specific CMake cache data,
dependencies, and generated files do not overwrite each other.

## Docker-backed targets

The backend builds are driven by custom targets in the top-level
`CMakeLists.txt`.

Those targets run CMake inside Docker images that already contain the required
toolchain and system headers.

- `docker-x11`
- `docker-gemix`
- `docker-gemix-gemd`
- `docker-sdl2`
- `docker-openmotif`
- `docker-openlook`
- `docker-wmaker`
- `docker-win`
- `docker-haiku`

The images are:

- `wischner/gcc-x86_64-linux-x11`
- `wischner/gcc-x86_64-gemix:latest`
- `wischner/gcc-x86_64-linux-sdl`
- `wischner/gcc-x86_64-linux-motif`
- `wischner/gcc-x86_64-linux-openlook`
- `wischner/gcc-x86_64-linux-window-maker`
- `native-windows-winui-sdk:24.04` (local `docker-win-sdk` dependency)
- `wischner/gcc-x86_64-haiku`

The source tree is mounted into the container at the same absolute path that it
has on the host. This keeps CMake build trees and cache paths stable between
host-side and Docker-side invocation.

## Backend status from this workflow

- Runtime-tested:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif under Xvfb in its Docker image
  - Linux OPEN LOOK/XView in Docker with Xephyr and olwm
  - Linux Window Maker/WINGs in Docker with Xephyr and Window Maker
  - GEM, Rasta@SDL2 through Docker
  - Windows MinGW binaries run natively in the Windows 11 VM
  - Haiku binaries built through Docker, copied to a Haiku machine, and run there
  - Apple binaries built and run on the configured remote macOS host
- Other backends/toolkits:
  - still work in progress

## Root project structure

The root `CMakeLists.txt` adds the library and program subtrees:

- `lib/native/`
- `src/`

When tests are enabled, the root project also adds `tests/`. It defines these
executables:

| Executable | Scope | Where it runs |
| --- | --- | --- |
| `native_wayland_runtime_tests` | Real GTK/Granite widgets; 120 generic-GTK live-control/theme pixel comparisons including toolbar states, equal folder/leaf row heights, retained-widget callbacks after split close, all sixteen native button edge masks, primary press/release and Ctrl/Shift/group table routing, editable-combo caret and active-row synchronization, repeated native split allocation/destruction, native signals, validation, source overlays, lazy rows, container ownership, reparenting and reentrant close lifetimes | WAYLAND and ELEMENTARY under headless Weston |
| `native_core_tests` | Color, geometry, and signal behavior; no display access | Every hosted build |
| `native_mac_runtime_tests` | Stock AppKit painting, all 16 native button bezel masks and borderless corner removal, 16–48 px toolbar highlight bounds on both axes, selected Vision navigation icon contrast, retained-view shutdown guards, text/split hover cursors, ruler motion, composed property-dialog toolbars, native button/check/radio/toolbar theme parity in light/dark and disabled states, derived button hook, native cells and image pixels, upright alpha-blended image text, group disclosure, icon selection, accordion scrollbar ownership, split pane geometry and minimums, all tab placements/resizing, million-row native grids, viewport state and light/dark row contrast | macOS login session on remote `mike` |
| `native_motif_runtime_tests` | Pre-show graphics, editor focus/input isolation, native list metrics, accordion scrollbar containment, tree alignment, both table modes and native scrollbar dragging, grid pixels and continuous multi-selection, property foregrounds and native/custom combo and checkbox-indicator pixel parity, checkbox label alignment, settings margins/indentation, bounded repainting, code contrast, sash and divider dragging | Motif display; Docker Xvfb and the CDE Xephyr session |
| `native_openlook_runtime_tests` | Real XTest table modifiers and code-editor arrows/Tab/Escape, combo choice-menu synchronization, dot dates/colon times, native picker popup lifecycle, labeled search, nested page coordinates and preserved native editor pixels during background damage; radio exclusion, intrinsic button height, identical partial-expose pixels, keyboard focus/text/completion, pre-show chrome construction, pane/list resizing and repeated modal/modeless teardown | OPEN LOOK display with XTest; Docker Xvfb/olwm or Xephyr/olwm |
| `native_windows_runtime_tests` | Real Win32 styles and custom-draw defaults, keyboard/clipboard, splitter dragging, paint clipping/resource lifetime, independent modeless stacking and modal exclusion, grouped/virtual grid pixels and scrolling, four-edge tabs and short vertical label sizing, status-strip resize clipping and hit-testing | Windows VM |
| `native_window_api_tests` | Backend-neutral window, control, layout, and model contracts; no control windows, but Haiku initializes an app-server connection for fonts and themes | Every hosted build |
| `native_inspector_runtime_tests` | Modeless inspector creation and post-creation resizing, native checkbox/text/number input, icon/value pixels, Win32 combo paper and selection, live toolbar grow/shrink and sibling hits, all 16 SDL/Haiku/Motif border masks, arbitrary dropdown values with native popup content, cancellation/stale callbacks, scrolling and recreation | SDL2, X11, Motif, OPEN LOOK, WINGs, Windows and Haiku; native event loop, XTest on X11 toolkits |
| `native_inspector_tests` | Typed property validation, event silence, all sixteen border masks, multiple edge bars, sticky groups and exact 16x16/24x24/32x32 icon pixels | Every hosted build; gemd session wrapper for proxy GEM |
| `native_table_model_tests` | `table_model`/`table_store` behavior, native-pitch paging and scroll endpoints | Every hosted build |
| `native_code_document_tests` | `code_document` text and marker behavior | Every hosted build |
| `native_collection_runtime_tests` | Live collections, source-editor lifecycle, combo composition and four-edge tab switching; Motif/OPEN LOOK parent-child teardown; Haiku native visibility, inset-arrow geometry, scrollbar endpoints, and drawing-state checks | Registered on SDL2, Motif, OPEN LOOK, macOS, GTK Wayland and elementary; run on Haiku over SSH |
| `native_modal_runtime_tests` | SDL live nested modal sessions, synchronous file-dialog completion, message-box focus restoration, callback-safe control dispatch, and table/scrollbar/split pointer routing | Registered as a test on SDL2 |
| `native_mouse_shader_tests` | Generation-safe pointer transitions/cancellation, rich deltas, package validation, orientation/color/alpha, multipass/history, transaction rollback, copied source and recreation | All backends; inert window hooks, image drawing; no desktop cursor/raw-input claim |
| `native_surface_runtime_tests` | Live `panel` and `canvas` lifecycle: layout, nesting, scrollbar thresholds, scrolling, pointer routing, and destroy/recreate | Registered as a test on SDL2 |
| `native_sdl_presentation_tests` | Accelerated renderer flags exclude a blocking vertical-refresh wait by default | SDL2 dummy video startup in CTest; policy assertion requires a desktop accelerated renderer |
| `native_sdl_control_tests` | Queued SDL Ctrl/Shift table selection, text replacement and popup synchronization, tab child lists, completion keyboard/mouse, picker availability/toggling/time steps, compact picker widths and seconds resizing, square step-button geometry and mirrored glyph pixels, equal navigation margins/search gap, list primitive pixel parity and bounded repeated-frame text uploads | SDL2 dummy/software; earlier input coverage also verified through Docker/GDB on the host X11 desktop |
| `native_sdl_composition_tests` | Final renderer pixels for panel/tab/split/canvas and accordion/panel/canvas nesting, neighboring four-edge tab strips with non-client chrome, tab switching, resizing, and video shutdown preserving separately owned audio | SDL2 dummy video with software rendering; desktop pixel readback also requires `SDL_RENDER_DRIVER=software` |
| `native_canvas_input_tests` | Actual SDL button/motion events through nested tabs and split canvases, button identity, drag/release capture and cancellation without synthetic releases | SDL2 dummy video with software rendering |
| `native_gemix_runtime_tests` | Absolute displayed white/black colors (`--colors-only` isolates it), packed/RGBA framebuffer decoding, initial desktop, menu topology/teardown, root damage, pressed feedback, popup borders, overlap/title restoration, occlusion-correct modal/modeless opening, untitled modal geometry and `1011` edges, atomic close presentation, text/source-editor input and clipboard, splitter capture, stock bitmap fonts, and original scrollbar arrow pixels in both orientations with pressed/clipped/small-button coverage | GEMix; uses a separate rasta framebuffer without requiring a viewer |
| `native_gemix_session_tests` | Real viewer screenshot checks for AES menus/title/edges and Native content; WM close with live GDB/MI stdin, owned process shutdown, command exit status and clean MI stdout | GEMix proxy; Python 3, Xvfb, Xlib, ImageMagick and GDB from the pinned Docker image; saves `tests/rasta-desktop.png` in the backend build tree |
| `native_gemix_input_tests` | Actual Rasta packets through `app::run`: rapid focus clicks, typing/clipboard, SDK file/save/folder/alert acceptance and restored owner input, fresh combo/tab-list input, calendar activation/toggling, selected time components and square-arrow pixels, framed embedded fields, navigator spacing, final property rules, stippled modal-owner text, actual splitter cursor bitmap restoration, collection scrolling/capture, property editing, and toolbar activation/recreation | GEMix direct and proxy; private framebuffer and UDP viewer endpoint, no graphical viewer required |
| `native_x11_runtime_tests` | Visible single-line/search glyph pixels, real copy/paste, typed-match popup checkmarks, six-pixel labeled-search/navigation spacing, real calendar selection and holiday disabling, equal time arrows and selected-component steps, nested-grid shrink allocation queries, group-bottom pixels, pre-show graphics, real Ctrl/Shift table selection, native text replacement and Enter commits, full-row selection, grow/shrink layout, idle repaint counts, collection/table edges, bottom rows, bordered full-width combo menus with hover/selection, four open tab joins, individual menu borders without a full-width rule, teardown, centered message buttons/shells with edge and hidden-owner placement, server modal grabs, all three native file/folder panels, and live bordered-pane drag | X11; X server (including private Xvfb) and the image's XTest runtime |
| `native_x11_scrollbar_tests` | Stock Xaw widget identity, minimum-size thumb hit testing, primary grab offsets, page-up/down track clicks, table/icon/tree/canvas ranges, million-row endpoints, horizontal scrolling, signed canvas origins, real middle-button dragging and automatic/policy hiding | X11; X server and the image's XTest runtime |

The macOS runtime target includes `native_mac_presentation_checks.mm` and
Vision's property-dialog source, so pointer/theme comparisons and composed
toolbar checks exercise the actual gallery implementation.

Portable runtime executables are built on each applicable backend; SDL2
registers them with `SDL_VIDEODRIVER=dummy`. GEMix and X11-specific tests are
registered only for their respective builds. Toolkit sessions can also run
portable runtime binaries by hand; each closes its own window. When using
the X11 image's `xvfb-run` as a Docker command, pass Docker `--init` so X-server
readiness signals are delivered normally.

### Monochrome X11 runtime session

After `docker-x11`, launch the gallery under TWM inside a dedicated nested X server:

```bash
bash scripts/linux/x11/run-twm.sh
```

The host needs only Docker and a display. Xephyr, TWM, the X utilities and
GDB come from the published `wischner/gcc-x86_64-linux-x11` image:
`run-twm.sh` hands itself to `docker-debug.sh` as the session command and
raises the session inside the container. `NATIVE_X11_IMAGE` selects another
image. The script starts a nested server on the first free display from
`:10` through `:99` and loads only that display's black/white resources and
the repository's TWM configuration (`twmrc-bw`: black/white `Color` block,
`fixed` fonts, two-pixel borders). Then it runs Docker-built Vision.
`NATIVE_X11_DISPLAY=:11` selects another free display; an explicitly
requested occupied display is refused. No host X resources or personal TWM
files are changed. TWM uses automatic placement over a 50% stippled root.
The desktop is strictly black and white, but the server keeps the host's
color depth: Xephyr supports only depths 8, 16 and 24, so a 1-bit screen is
not available, and application images keep their colors.

The session follows the host desktop scale, read from the host `Xft.dpi`
(or `NATIVE_X11_DPI`) as `Xft.dpi / 96` in quarter steps. GNOME with
`xwayland-native-scaling` shows X clients in device pixels, so an unscaled
1280x900 Xephyr would appear at half size on a 2x monitor. When the image
provides Xwayland 23.1 or later and the host runs Wayland, a rootful
Xwayland of 1280x900 is used instead, and the compositor scales it like any
unscaled program. `docker-debug.sh` forwards the Wayland socket for this
session only. Otherwise Xephyr gets a 1280x900 screen multiplied by the
factor and the host value as its `Xft.dpi`, so the X11 backend scales itself
(see [Painting](PATTERNS-PAINTING.md)). TWM's spacing, icon manager and root
stipple are scaled to match. Its border widths stay as configured, and
`fixed` is replaced by the nearest installed bitmap size, such as Lucida
Typewriter 26 at 2x. The image's `xfonts-75dpi`, `xfonts-100dpi` and
`xfonts-terminus` (from image 1.4.0) provide those sizes. The local server accepts local clients without X
authentication (`-ac`) and disables TCP listening; use only on a trusted
host. Without arguments it runs Vision. With arguments it runs the supplied
debugger, keeping stdout exclusively for GDB/MI and session diagnostics on
stderr. The X11 F5 configuration uses this same wrapper and waits for TWM
before starting GDB. Closing Vision in standalone mode, or ending GDB in
debug mode, cleans up only that session's TWM and nested server.

The current top-level project does not build generated API documentation.
The manuals in `docs/manuals/` are maintained as source documentation only.

## Core library target

The `native` library is defined in `lib/native/CMakeLists.txt`.

The shared target contains portable code such as:

- geometry
- screen metadata
- application startup
- common graphics state

The same target is then extended by platform and toolkit subdirectories.

## Platform selection

Platform code lives under `lib/native/platforms/`.

The platform subtree is selected with standard CMake platform variables:

- `WIN32`
- `HAIKU`
- `APPLE`
- `UNIX`

On Linux, the platform layer is combined with a toolkit layer.
On Apple and Haiku, platform-specific source sets are used.

## Toolkit selection

Toolkit code lives under `lib/native/toolkits/`.

The active toolkit is selected through the cached `TOOLKIT` variable in
`lib/native/toolkits/CMakeLists.txt`.

Only one toolkit subtree is added per configured build tree.

That is why Linux toolkit builds use different directories instead of sharing
one `build/` directory.

Windows and Haiku do not use the toolkit selector in the current build path.

## Vision application

The `vision` application is defined in `src/CMakeLists.txt` and links against
`native`. It is the repository's only executable program.

Each backend-specific tree places it in its `src/` build directory:

- `build/linux-x11/src/vision`
- `build/linux-gemix/src/vision`
- `build/linux-sdl2/src/vision`
- `build/linux-openmotif/src/vision`
- `build/linux-openlook/src/vision`
- `build/linux-wmaker/src/vision`
- `build/windows-mingw-w64/src/vision.exe`
- `build/haiku/src/vision`

## VS Code Debug workflows

Every launch entry builds Vision with `CMAKE_BUILD_TYPE=Debug` and starts a
debugger in the environment that owns the backend. Select a configuration in
the VS Code Run and Debug view and press F5.

| Configuration | Execution target | Debugger |
| --- | --- | --- |
| Linux X11 | Local Docker container, dedicated monochrome Xephyr/TWM display | GDB through Docker |
| Linux SDL2 | Local Docker container and local display | GDB through Docker |
| Linux OPEN LOOK | Docker container, Xephyr/olwm session | GDB through Docker |
| Linux Window Maker | Docker container, Xephyr/Window Maker session | GDB through Docker |
| Linux GEMix | Docker container with image-provided SDL2 Rasta | GDB through Docker |
| Linux Motif/CDE, Xephyr | Docker container, Xephyr/CDE session | GDB through Docker |
| Windows | Windows 11 VM, interactive autologon desktop | Docker MinGW GDB through SSH to native Win64 GDBserver |
| Haiku | `Haiku` KVM guest | GDB over SSH |
| macOS | VM host `mike` | LLDB over SSH |

The X11 and SDL2 debugger pipes keep the program inside their build images.
This is important because a Debug binary can depend on the sanitizer runtime
supplied by that image rather than the version installed on the host. The X11
pipe first creates the nested monochrome session and forwards that `DISPLAY`;
SDL2 keeps the host display. Both forward the X11 socket and active Xauthority
file. Select **Linux X11, Xephyr** for this X11 F5
workflow; its Docker build task and executable path are unchanged.
The X11 profile disables LeakSanitizer's exit-time leak scan under GDB
(`ASAN_OPTIONS=detect_leaks=0`), since it cannot run under ptrace; address
and undefined-behavior checks remain enabled.
The **GEM, Rasta@SDL2** profile uses `scripts/gemix/debug-pipe.sh`
to run the image-provided SDL2 Rasta viewer, gemd and GDB in one container.
`scripts/gemix/rasta-session.sh` owns a private 32-bit framebuffer and starts
the viewer and server with inversion off. AES chrome and portable controls
therefore display the same white paper and black ink. The runner supervises
both viewer and debugger exits: closing Rasta terminates its owned process
session, including GDB, gemd and Vision, even while MI stdin remains open.
Command exit status is preserved. `gemd-session.sh` owns the server; logs go
to stderr, keeping GDB/MI stdout clean. The local GEMix launch profile is
removed. OPEN LOOK uses `docker-debug.sh --session openlook-xephyr`
to start GDB in the build image with Xephyr and olwm. Window Maker uses the
same pipe with `--session window-maker-xephyr` and the Window Maker build image.

### Development-host preparation

The development host needs:

- VS Code with the Microsoft C/C++ extension for `cppdbg` configurations.
- CMake, Docker, GDB, SSH, SCP, rsync, and libvirt's `virsh`.
- TCP port 31337 available on loopback for the Windows VM SSH debug tunnel.
- Permission to use Docker and to start the required libvirt domains.
- The Docker images named by the top-level CMake project. In particular, the
  GEMix image must contain its runtime resources; Native does not mount a
  repository-owned resource directory.

### Windows VM preparation

The Windows library propagates a Common Controls v6 manifest resource into
consuming executables. This enables operating-system visual styles, not a
library-painted skin. The backend uses Windows 10 API declarations. Settings
navigation hosts the real WinUI NavigationView through Windows App SDK 1.8.
`docker-win` depends on `docker-win-sdk`, which builds the pinned Ubuntu/GCC 13
image and generates native C++/WinRT projections. Wine is used only for the
build-time metadata generator. `scripts/windows/winui/install-sdk.py` records
SHA-256 checksums and exact NuGet/MSYS2 versions, including Windows SDK
contracts 10.0.26100.1 and C++/WinRT 3.0.260818.1. Extracted SDK license
notices remain in `/opt/native-winui/licenses`. F5 deploys the App SDK bootstrap DLL beside
Vision and checks the guest's installed x64 Windows App Runtime 1.8.

**Windows 11, VM** retains the `docker-win` Debug build and
deploys its executable, MinGW DLLs and native Win64 GDBserver through SSH.
The guest is reached through `ssh whiskey`; SSH configuration supplies its
account and key. Optional `WINDOWS_VM_DOMAIN` enables libvirt startup.
`Native-Vision-Debug` is an interactive-user scheduled task: Vision runs on
the visible Windows desktop, not SSH's session 0 and not Wine. The debugger
wrapper waits for its loopback SSH tunnel before starting the desktop server.
A workspace-specific SSH control socket lets normal and post-debug cleanup
close an orphaned tunnel without touching unrelated port owners. It cleans up
only its deployed debug processes when GDB exits; a post-debug task handles forced adapter
termination. It leaves Windows running. Source breakpoints work, but
interrupting a running GUI remains an open debugger issue; close Vision's
window to end a running session cleanly.

Windows preparation uses Python 3.11+ to compare content hashes and transfer
only changed deployment files. A debug-section-free staging executable under
`build/windows-vm/` reduces transfer size; GDB still reads the full local
`build/windows-mingw-w64/src/vision.exe`. The desktop launcher caches loaded
DLL versions for the next preparation's local GDB sysroot. Windows revision
changes invalidate that cache. Its scheduled action uses Windows Script Host
to start PowerShell hidden, and the Windows-specific F5 tasks keep terminal
output in the background.

The one-time key-only SSH setup, host-key verification, guest firewall rule,
connection overrides and exact deployed paths are documented in
[Windows VM Runtime](../../notes/WINDOWS-VM-RUNTIME.md). `prepare.sh` copies
the exact runtime DLLs reported by the Docker cross-compiler; no host Wine
installation is involved. The original Wine helper scripts remain historical
tools and are not referenced by any F5 configuration or task.

### Docker OPEN LOOK preparation

Select **Linux OPEN LOOK, Xephyr**. F5 builds the
`docker-openlook` target into `build/linux-openlook/` and runs the image's
GDB through `scripts/linux/docker-debug.sh --session openlook-xephyr`.
The `wischner/gcc-x86_64-linux-openlook:latest` image supplies XView, OLGX,
Xephyr, olwm, fonts, X utilities and GDB. No guest setup is required.

The pipe forwards the host X11 socket and Xauthority file and allocates free
nested displays. The image helper starts Xephyr and olwm, runs GDB, and cleans
up its session when debugging ends. If the host display is unavailable it
uses an Xvfb parent. Debug builds retain ASan/UBSan; the launch profile disables
LeakSanitizer under ptrace. Standalone execution uses:

```bash
./scripts/linux/toolkit-session-run.sh openlook ./build/linux-openlook/src/vision
```

### Docker Window Maker preparation

Select **Linux Window Maker, Xephyr**. F5 builds `docker-wmaker` into
`build/linux-wmaker/` and starts the image's GDB through
`scripts/linux/docker-debug.sh --session window-maker-xephyr`.
`wischner/gcc-x86_64-linux-window-maker:latest` supplies the compiler,
WINGs/wraster libraries, font stack, Xephyr, Window Maker and GDB.

The pipe forwards the X11 socket and active Xauthority file, choosing free
nested displays. The image helper starts Xephyr and Window Maker and cleans
up when GDB exits. Without a reachable host display it uses an Xvfb parent.
Debug builds retain ASan/UBSan; the launch profile disables LeakSanitizer
under ptrace. Standalone execution uses:

```bash
./scripts/linux/toolkit-session-run.sh wmaker ./build/linux-wmaker/src/vision
```

### Motif/CDE Xephyr session

Select **Linux Motif/CDE, Xephyr** for F5. It builds with
`docker-openmotif` and starts GDB through `scripts/linux/docker-debug.sh
--session cde-xephyr` in `wischner/gcc-x86_64-linux-motif:latest`.
The image supplies CDE, Xephyr, development headers and libraries.
The executable is `build/linux-openmotif/src/vision`; no CDE VM or SSH
setup is required. ASan/UBSan remain enabled; LeakSanitizer is disabled
under GDB. For standalone execution:

```bash
./scripts/linux/toolkit-session-run.sh openmotif ./build/linux-openmotif/src/vision
```

The image's `cde-xephyr` helper starts the full CDE desktop and runs the
command as its CDE user. Container root starts the required RPC services;
Docker's default network isolates those services from host RPC ports.
The runner forwards the host X socket and Xauthority, assigns separate
nested displays, and uses the helper's headless Xvfb fallback when the
host display is unavailable.

### Haiku preparation

The Haiku configuration expects:

- A libvirt domain named `Haiku`.
- The SSH alias `hotel`, authenticated without an interactive password prompt.
  `ssh hotel` must open the guest shell; the SSH configuration owns its user,
  address and identity settings.
- GDB at `/boot/system/bin/gdb` and the SSH server enabled in the guest.
- A graphical Haiku session for the same user.

The launch profile is named **Haiku, VM**. Its start, directory preparation,
SCP deployment and SSH debugger transport all use the `hotel` alias.
The 2026-10-21 connection verifies hostname `hotel` and GDB at the configured
path; see the [remote runtime note](../../notes/HAIKU-REMOTE-RUNTIME.md).
F5 starts the guest if necessary, waits for SSH, cross-builds the Debug binary,
creates `/boot/home/Projects/native/run`, copies Vision there, and starts GDB
inside the guest. Source files stay on the development host; the debug symbols
retain their absolute host paths so VS Code can open them locally.

### macOS preparation

The macOS remote build checks Apple Command Line Tools and CMake before
source synchronization, including `~/Applications/CMake.app` in its search
path. CMake probes `std::jthread`/`std::stop_token` and enables libc++
`-fexperimental-library` on Apple SDKs only when required.

The macOS configuration expects `ssh mike` to connect without a password and the
Mac to have an active graphical login. Remote Login must be enabled. The Mac
needs rsync, Clang, LLDB, and CMake either in Homebrew, `/usr/local/bin`, or the
CMake application bundle.

The terminal launcher stops at entry to install its process-exit listener,
then continues under interactive LLDB. Breakpoint/crash stops retain the
prompt; final process exit closes the debugger and SSH session automatically.

LLDB also needs Developer Tools authorization. Run this once in a local
terminal on the Mac:

```bash
sudo /usr/sbin/DevToolsSecurity -enable
sudo /usr/sbin/dseditgroup -o edit -a tomaz -t user _developer
```

Log out and back in if group membership was added by the second command. The
debug helper checks Developer Tools authorization and prints the enable command
instead of hanging on an invisible remote authorization prompt.

F5 synchronizes the source to `/Users/tomaz/Projects/native`, configures and
builds `build/macos-debug`, then runs the foreground application bundle at
`src/vision.app` under LLDB in the VS Code terminal. Log in to the Mac directly
when its graphical display is needed. The `MAC_REMOTE_HOST`, `MAC_REMOTE_USER`,
`MAC_REMOTE_BASE`, and `MAC_REMOTE_PROJECT` environment variables can override
those defaults.

## Summary

- CMake is the build entry point.
- `build/cmake/` is the host control tree.
- Reproducible backend builds run inside Docker; desktop-specific F5 builds run
  natively in the configured guests.
- Backend build trees are separate on purpose.
- The root project builds the library and Vision, not generated API docs.
- Runtime verification currently covers Linux X11/SDL2/OpenMotif, OPEN LOOK in
  Docker, Window Maker in Docker, the native Windows VM, Haiku deploy-and-run over
  SSH, and Apple on the configured remote host.

## Background-service regression targets and consumers

| Target | Coverage | Runtime |
| --- | --- | --- |
| `native_infinity_tests` | Physical pairing/consumption/reset and bounded scoped UI delivery, including concurrent close, reentrant destruction and recreation | All hosted backends; inert native hooks, no display |
| `native_audio_queue_tests` | Production PCM queue with deterministic private device factory: copied data, stereo frames, bounds, silence, failure and reopen | All hosted backends; no audio device |
| `native_process_tests` | Real child launch, quoting/environment, simultaneous bounded output, cancellation and reaping | All hosted backends; self-contained child modes |
| `native_keyboard_runtime_tests` | Real SDL input dispatch, focused-editor isolation, and audio survival across loop shutdown | SDL2 dummy video/software renderer and dummy audio |

These tests are registered with CTest and a timeout. Cross-build executables
run on their target OS; a successful cross-build is not a runtime pass.
`vision --infinity-test` is the public-API manual acceptance surface on every
backend, with held-key counters, stereo tone, progress/cancel and child tests.
GEMix explicitly reports physical keyboard unavailability. Device playback
and actual desktop focus remain manual acceptance checks.

When included through a parent's `add_subdirectory()`, Native builds only
its library; Vision, tests and Docker targets remain top-level entry points.
The parent owns its GUI target, platform launcher and Docker invocation.


The optional image-shader authoring tool is `scripts/shaders/package.py`. It
requires Python 3's standard library only and writes `native-image-1` packages
shared by every backend. Python and shader translation libraries are not
runtime/link dependencies; the interpreter sources are compiled in the ordinary
`native` target. Vision embeds the independent CRT test package at configure
time and exposes it through `--mouse-shaders`. See the
[image-shader chapter](../programming-native/20-IMAGE-SHADERS.md) for authoring.

The standard-library package compiler also has deterministic output and hostile
expression tests: `python3 tests/native_shader_package_tests.py`. These do not
build a host C++ executable or replace the Docker CTests.

| Terminal test | Coverage | Runtime needs |
| --- | --- | --- |
| `native_terminal_screen_tests` | Incremental ANSI/color/cursor/erase/scroll state, portable font pixels and CRT history, shaded bezel, glass reflection/recess and power-indicator pixels | Every backend; external asset fixtures |
| `native_retro_terminal_smoke` | Production Vision window, native editor/button siblings, host-shell output, real shader output, hidden cursor and posted close | Linux CTest: SDL dummy, X11-family Xvfb, GEMix direct/gemd; same CLI is available for target desktop checks |

Vision's post-build step copies `src/assets/retro-terminal/` beside the executable,
including attributed shader source, package, font and license files. Preserve
that directory during VM deployment. The test mode does not change the normal
Vision launch configuration.


| Test | Coverage | Backend |
| --- | --- | --- |
| `native_settings_model_tests` | Search filtering and retained page selection, Gregorian values, availability rules, stable IDs and silent setters | All selections |
| `native_settings_runtime_tests` | Native picker callbacks, rejection rollback, search, visible WinUI island creation and Win32 picker text padding, AppKit source-list sections/calendar popover, standard text-field picker heights, text-only date mode, compact centered time stepper and adaptive control colors, page switching/recreation; GTK navigator texture pixels and compositor grow/shrink allocation; Athena native navigator row widths/sensitivity, editable date/time commits, calendar captions/availability and popup toggling | Native display; SDL dummy and GEM framebuffer supported |
| `native_glsl_context_tests` | EGL/GLX handoff, API and draw/read restoration, repeated activation, creation/activation rollback and destruction with an active image context | Linux; deterministic shared driver fixtures, no display/GPU required |
| `native_desktop_glsl_context_tests` | Real WGL/CGL caller-context preservation, repeated activation, nesting rejection, idle leave and entered teardown | Windows/macOS; skips with code 77 when desktop OpenGL is unavailable |

The two settings CTests run the `native_settings_tests` executable, with
`--runtime` selecting the native display/lifecycle fixture.

The private driver fixture is loaded only by this test through its CTest
`LD_LIBRARY_PATH`; it is never linked into or deployed with Native or Vision.


## Amiga runtime qualification

`docker-amiga-check` configures the standalone `tests/amiga_toolchain/` project
with `scripts/cmake/toolchain-amiga.cmake` into `build/amiga-toolchain/`.
It is a diagnostic prerequisite and does not configure the `native` target or
Vision. No Amiga launch profile is supplied because no Amiga GUI binary exists.

The default `DOCKER_IMAGE_AMIGA` is
`wischner/gcc-m68k-amiga@sha256:7622740b8aaa5c1a5444ec3ccf04e9506f71375300b0946e1382d67c56692cff`.
The toolchain settings select m68k/68000 and libnix `nix20`. Configuration
attempts both compile/link checks, writes their complete diagnostics and fails
explicitly if either fails. The measured image has GCC 16.1.1b with thread
model `single`: C++20 worker/mutex types are omitted, and filesystem linking
also fails. The complete evidence and repair gate are in
[ADJUST.md](../../ADJUST.md), A00. A different repaired compiler image can be
selected through the host cache's `DOCKER_IMAGE_AMIGA` setting.

| Standalone executable probe | What it qualifies |
| --- | --- |
| `native_amiga_thread_probe` | Real mutex/jthread ownership, notification and stop-aware cancellation; links `pthread` |
| `native_amiga_filesystem_probe` | Streams, per-run scratch-directory creation, rename, traversal, metadata and cleanup |

These targets are generated only after both checks link. They are not
registered as another backend's CTests and are not included in Native's test
counts. Compile/link success still requires target runtime qualification.
Run the filesystem probe with a writable scratch directory, default `RAM:` on
Amiga. The same sources were verified on a hosted C++20 runtime to check the
qualification harness itself. A CLI/API emulator does not prove native UI or
hardware behavior. This target currently fails and does not add an Amiga build
selection to the library.

## Adjustment regressions

| Test | Purpose |
| --- | --- |
| `native_lifecycle_tests` | Shared allocation/hook rollback, retry, create/paint deletion and expired drawer-state guards |
| `native_glsl_renderer_tests` | Real GLSL later-pass history, reset/resize, failed tick retry and extent/allocation limits; explicit skip if no context |
| `native_motif_allocation_tests_0..2` | Toolkit-only baseline, selection and notebook allocation controls; no Native linkage |
| `native_openlook_allocation_tests_0..3` | Toolkit-only baseline, PANEL_TEXT, owned menus and PANEL_MULTILINE_TEXT/TextSW input/menu allocation controls; no Native linkage |
| `native_wmaker_repaint_tests` | Deferred, coalesced icon/tree collection repaint and cancellation across destruction/recreation | Window Maker display; Docker Xephyr |
| `native_wmaker_allocation_tests` | Toolkit-only WINGs font creation, measurement, drawing and native widget retention control; no Native linkage |
| `native_classic_control_tests` | Window Maker/Motif/GEM table modifiers, completion input, editable-combo synchronization, joined date/time fields, disabled holidays, calendar toggle/reopen/day selection and Motif outside-click dismissal after typed dates, Motif square-button visibility after relayout and selected hour/minute/second stepping, navigation search; WINGs additionally checks first-open editor input, actual I-beam pixels, native list sections/icons, symmetric margins, compact dates and framed calendars; X backends also check standard modal-panel centering |
| `native_worker_shutdown_tests` | Retire a blocked worker without blocking its caller; join independently of UI turns |
| `native_close_runtime_tests` | Actual native close cancellation then acceptance on every existing backend |
| `native_haiku_glsl_context_tests` | Real OSMesa core contexts, binding order, retired outer peers, viewport preservation and untouched foreign bindings; explicit skip when the runtime is unavailable |
| `native_haiku_dispatch_tests` | Native two-window paint/resize with 100 posted updates, callback serialization and shutdown |

Motif runtime/allocation tests use full allocation unwinding and only the
independently reproduced `XtOwnSelection`, `XtDisownSelection` and
`_XmSFAddNavigator` exceptions. XView suppresses only its independently
reproduced `win_add_drop_interest` node. Native's menu-item allocations remain
checked; explicit `MENU_RELEASE` ownership fixes that leak. These are SDK
exceptions, not a claim of leak-free upstream toolkits.

WINGs close/allocation tests except only independently reproduced WMCreateFont/WMWidthOfString
retention, with full allocation unwinding. The hosted GNU Debug Linux
native_glsl_renderer_tests conditionally preloads ASan followed by Mesa's
swrast_dri.so when present: keeping vendor driver metadata rooted avoids
unload-only bookkeeping leaks while Native remains leak-checked. The EGL/GLX
loader likewise keeps vendor dispatch/TLS code resident with RTLD_NODELETE
where available. Contexts, surfaces and owned display leases still release.
No leak suppression or detect_leaks=0 is applied to the real-renderer test.
The renderer fixture has inert window hooks and runs directly even for gemd;
it does not preload the driver into an unrelated server-launch shell.

Cross-built mouse_shader tests accept --data PATH for tests/data fixtures;
terminal_screen tests accept an asset-directory argument. Copy these fixtures
to target hosts along with the executables. --benchmark is an explicit opt-in
for the mouse/shader timing run, not part of the ordinary regression suite.
Windows/macOS real-context tests and the real-renderer test use skip code 77
when a suitable context is unavailable; a skip is not GLSL acceptance.

Haiku GLSL uses the optional runtime libOSMesa.so.8 and OpenGL 3.3 core; its
adapter resolves C entry points dynamically so the Docker SDK needs no Mesa
headers or link library. The supported test environment sets
GALLIUM_DRIVER=softpipe for native_haiku_glsl_context_tests and
native_glsl_renderer_tests. Mesa 22.0.5 llvmpipe's generated vertex code faults
on the configured VM, and Native diagnoses that known combination before drawing.
The Haiku F5 profile supplies the same per-process environment. No global VM
setting or application environment is modified by the library. Copy the normal
Vision assets and use the same setting for --retro-terminal --terminal-smoke.

## GTK 4.14 Wayland and elementary OS

`TOOLKIT=WAYLAND` selects GTK 4.14's GDK Wayland backend. `TOOLKIT=ELEMENTARY`
selects the same private adapter plus Granite 7's C interface. The implementation
is C++ throughout. It neither includes Xlib nor falls back to X11. The optional
image-shader adapter uses EGL and omits GLX discovery for these selections.

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-wayland
cmake --build build/cmake --target docker-elementary
```

Both targets first build `docker-wayland-sdk` from
`scripts/linux/wayland/Dockerfile`, tagged `native-wayland-sdk:24.04`.
`DOCKER_IMAGE_WAYLAND` overrides that local image name. The pinned Ubuntu 24.04
base supplies GCC, GTK 4.14.x, Granite 7.4, GtkSourceView 5.12, GLib/GIO,
Pango/HarfBuzz, Cairo, Fontconfig, xkbcommon, PNG/JPEG, EGL/Mesa and
`gsettings-desktop-schemas`. GraniteTimePicker reads the installed
`org.gnome.desktop.interface` schema, including under headless Weston. The SDK
installs elementary's icon theme and stylesheet 8.0.0, pinned to commit
`9c713d0f4c35f80a783e5fa6845a39d977c7140c`; upstream source and GPL notices
remain in `/opt/elementary-stylesheet`. GTK API availability is capped at 4.14.
The independent output trees are `build/linux-wayland/` and
`build/linux-elementary/`; each contains `src/vision`.

The **Linux GTK4, Wayland** F5 profile uses
`scripts/linux/wayland/debug-pipe.sh` and requires a host Wayland compositor.
It forwards the Wayland socket and available session/system D-Bus sockets,
with no X11 socket or Xauthority mount. **Elementary OS, VM** builds the
elementary toolkit and deploys to `ssh echo`; no local elementary F5 profile
exists. The VM follows the installed theme and Granite color preference.
Debugging retains address/undefined-behavior checks and disables only the
ptrace-incompatible exit-time leak scan.

A real headless Weston compositor provides unattended runtime checks:

```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:$PWD" -w "$PWD" \
  native-wayland-sdk:24.04 bash scripts/linux/wayland/headless-session.sh \
  ctest --test-dir build/linux-elementary --output-on-failure
```

Use `build/linux-wayland` for the generic selection. Weston uses its headless
Pixman backend, not Xwayland. This verifies adapter behavior, not a Pantheon
visual/accessibility or portal-dialog acceptance pass. The focused
`native_wayland_runtime_tests` checks native widget types, silent setters,
validated edits, source synchronization, lazy million-row tables and recreation.

Wayland intentionally leaves top-level placement to the compositor. Monitor
work areas use the advertised monitor geometry; no global pointer position,
explicit pointer capture or relative-motion protocol is claimed by this adapter.
Native GTK implicit pointer gestures and cursor shapes remain available.

### elementary OS VM debugging

Select **Elementary OS, VM** in VS Code and press F5. The pre-launch task
builds `docker-elementary`, then `scripts/linux/elementary-vm/deploy.sh`
copies Vision, all retro-terminal assets and a private GtkSourceView runtime
to `echo:/home/tomaz/Projects/native/run`. The debug pipe runs the VM's GDB
through SSH with its existing Wayland and D-Bus session, Cairo rendering,
and the installed desktop theme. It keeps stdout reserved for GDB/MI.
`ELEMENTARY_VM_HOST` overrides the default SSH alias `echo`; the remote
user/path remains `tomaz`. The VM must have GDB, rsync and an active Wayland
login session at `wayland-0`. See the elementary VM runtime note for observed
desktop test limits. For deployment without debugging, build the target and
run `scripts/linux/elementary-vm/deploy.sh` directly.


Haiku date/time pickers use the SDK's `private/shared/DateTimeEdit.h` and
`libshared` with `localestub`. Their dependency is private to the platform
adapter, just like its existing ColumnListView integration; public headers
remain portable. No additional runtime service or custom calendar is added.

Vision embeds its checked-in navigation PNG bytes from
`src/assets/navigation/icons.inc`. Regeneration is an optional asset-authoring
step documented beside those assets; production builds add no font or SVG
runtime dependency.


## Check book examples and documentation links

The application book keeps complete programs in Markdown rather than a second
examples source tree. Check local links and heading targets using Python 3:

```bash
python3 scripts/docs/check.py
```

Extract complete C++ programs and a chapter/line manifest under `build/`, or
syntax-check them with the documented GCC SDK. The checker uses warnings as
errors and never includes private backend headers:

```bash
docker run --rm -v "$PWD:/workspace" -w /workspace \
  wischner/gcc-x86_64-linux-sdl:latest \
  python3 scripts/docs/check.py --syntax-check
```

`--extract build/doc-check/current` extracts without compiling. Generated
sources are not new executable targets. Syntax checks establish public API
compatibility, not interactive behavior; use the chapter's interaction steps
and the relevant runtime fixture for that evidence. These checks also cover
links from AI skills and agent-role instructions to their maintained lessons.
