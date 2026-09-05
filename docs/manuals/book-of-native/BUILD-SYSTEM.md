# Build System

This chapter describes the build structure that exists in the repository today.
It focuses on the current CMake workflow and the separation between the host
control tree and backend-specific build trees.

## Overview

### GEMix transport selection

`docker-gemix` builds the direct AES/VDI variant in `build/linux-gemix/`.
`docker-gemix-gemd` uses the same Docker image, sets `GEMIX_USE_GEMD=ON`,
and builds in `build/linux-gemix-gemd/`. Its executable links `libgem`, not
`libaes` or `libvdi`; the separate `gemd` process owns AES/VDI and Rasta.
The SDK headers still come from `gemix-aes` and `gemix-vdi` pkg-config modules.

The image must supply matching proxy libraries and `/opt/gemix/bin/gemd`.
`scripts/gemix/gemd-session.sh <command> [args...]` starts a private server,
exports its `GEMD_SOCKET`, runs the command, and cleans up only that server.
Its inherited framebuffer, dimensions and resource environment configure the
server. Client and server share filesystem paths for file selectors and scrap.
The proxy debug configuration invokes this wrapper around GDB, while the
original direct configuration remains available. Server logs go to stderr,
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
- `docker-sdl2`
- `docker-openmotif`
- `docker-openlook`
- `docker-wmaker`
- `docker-win`
- `docker-haiku`

The images are:

- `wischner/gcc-x86_64-linux-x11`
- `wischner/gcc-x86_64-gemix`
- `wischner/gcc-x86_64-linux-sdl`
- `wischner/gcc-x86_64-linux-motif`
- `wischner/gcc-x86_64-linux-openlook`
- `wischner/gcc-x86_64-linux-window-maker`
- `wischner/gcc-x86_64-windows-mingw-w64`
- `wischner/gcc-x86_64-haiku`

The source tree is mounted into the container at the same absolute path that it
has on the host. This keeps CMake build trees and cache paths stable between
host-side and Docker-side invocation.

## Backend status from this workflow

- Runtime-tested:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif under Xvfb in its Docker image
  - Linux OPEN LOOK/XView in the `Tribblix-OpenLook` KVM guest
  - Linux Window Maker/WINGs in the `Bookworm-WindowMaker` KVM guest
  - Linux GEMix through Docker and local rasta
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
| `native_core_tests` | Color, geometry, and signal behavior; no display access | Every hosted build |
| `native_windows_runtime_tests` | Real Win32 styles and custom-draw defaults, keyboard/clipboard, splitter dragging, paint clipping/resource lifetime, independent modeless stacking and modal exclusion, grouped/virtual grid pixels and scrolling, four-edge tabs and short vertical label sizing, status-strip resize clipping and hit-testing | Windows VM |
| `native_window_api_tests` | Backend-neutral window, control, layout, and model contracts; no control windows, but Haiku initializes an app-server connection for fonts and themes | Every hosted build |
| `native_table_model_tests` | `table_model`/`table_store` behavior, native-pitch paging and scroll endpoints | Every hosted build |
| `native_code_document_tests` | `code_document` text and marker behavior | Every hosted build |
| `native_collection_runtime_tests` | Live collections, source-editor lifecycle, combo composition and four-edge tab switching; Haiku native visibility, inset-arrow geometry, scrollbar endpoints, and drawing-state checks | Registered as a test on SDL2; run on Haiku over SSH |
| `native_modal_runtime_tests` | SDL live nested modal sessions, synchronous file-dialog completion, message-box focus restoration, callback-safe control dispatch, and table/scrollbar/split pointer routing | Registered as a test on SDL2 |
| `native_surface_runtime_tests` | Live `panel` and `canvas` lifecycle: layout, nesting, scrollbar thresholds, scrolling, pointer routing, and destroy/recreate | Registered as a test on SDL2 |
| `native_gemix_runtime_tests` | Initial desktop, menu topology/teardown, root damage, pressed feedback, popup borders, overlap/title restoration, occlusion-correct modal/modeless opening, untitled modal geometry and `1011` edges, atomic close presentation, text/source-editor input and clipboard, splitter capture, and stock bitmap fonts | GEMix; uses a separate rasta framebuffer without requiring a viewer |
| `native_gemix_input_tests` | Actual Rasta packets through `app::run`: rapid focus clicks, single/multiline typing, clipboard buttons and shortcuts, input after owned-window and file-selector closure | GEMix direct and proxy; private framebuffer and UDP viewer endpoint, no graphical viewer required |
| `native_x11_runtime_tests` | Pre-show graphics, full-row selection, grow/shrink layout, idle repaint counts, collection/table edges, bottom rows, bordered full-width combo menus with hover/selection, four open tab joins, individual menu borders without a full-width rule, teardown, centered message buttons/shells with edge and hidden-owner placement, and live bordered-pane drag | X11; X server (including private Xvfb) and the image's XTest runtime |
| `native_x11_scrollbar_tests` | Stock Xaw widget identity, table/icon/tree/canvas ranges, million-row endpoints, horizontal scrolling, signed canvas origins, real middle-button dragging and automatic/policy hiding | X11; X server and the image's XTest runtime |

Portable runtime executables are built on each applicable backend; SDL2
registers them with `SDL_VIDEODRIVER=dummy`. GEMix and X11-specific tests are
registered only for their respective builds. Toolkit sessions can also run
portable runtime binaries by hand; each closes its own window. When using
the X11 image's `xvfb-run` as a Docker command, pass Docker `--init` so X-server
readiness signals are delivered normally.

### Monochrome X11 runtime session

After `docker-x11`, launch the gallery under TWM inside a dedicated Xephyr:

```bash
bash scripts/linux/x11/run-twm.sh
```

The host needs `Xephyr`, `twm`, `xrdb`, `xdpyinfo`, `xwininfo`, `xsetroot`, and Docker.
The script starts a normal 1280x900 Xephyr on the first free display from
`:10` through `:99`, loads only
that display's black/white resources and the repository's TWM configuration,
then runs Docker-built Vision. `NATIVE_X11_DISPLAY=:11` selects another free
display; an explicitly requested occupied display is refused. No host X
resources or personal TWM files are changed. TWM uses automatic placement
and a monochrome stippled root; Xephyr retains normal color depth so application
images remain usable. The local server accepts local clients without X
authentication (`-ac`) and disables TCP listening; use only on a trusted host.
Without arguments it runs Vision. With arguments it runs the supplied
debugger through Docker, keeping stdout exclusively for GDB/MI and session
diagnostics on stderr. The X11 F5 configuration uses this same wrapper and
waits for TWM before starting GDB. Closing Vision in standalone mode, or
ending GDB in debug mode, cleans up only that session's TWM and Xephyr processes.

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
| Linux OPEN LOOK | `Tribblix-OpenLook` KVM guest | GDB over SSH |
| Linux Window Maker | `Bookworm-WindowMaker` KVM guest | GDB over SSH |
| Linux GEMix | Local Docker container and local Rasta display | GDB through Docker |
| OpenMotif | `Tribblix-CDE` KVM guest | GDB over SSH |
| Windows | Windows 11 VM, interactive autologon desktop | Docker MinGW GDB through SSH to native Win64 GDBserver |
| Haiku | `Haiku` KVM guest | GDB over SSH |
| macOS | Remote host `leia` | LLDB over SSH |

The X11 and SDL2 debugger pipes keep the program inside their build images.
This is important because a Debug binary can depend on the sanitizer runtime
supplied by that image rather than the version installed on the host. The X11
pipe first creates the nested monochrome session and forwards that `DISPLAY`;
SDL2 keeps the host display. Both forward the X11 socket and active Xauthority
file. Select **Debug Vision (Linux X11, Xephyr/TWM mono)** for this X11 F5
workflow; its Docker build task and executable path are unchanged.
The X11 profile disables LeakSanitizer's exit-time leak scan under GDB
(`ASAN_OPTIONS=detect_leaks=0`), since it cannot run under ptrace; address
and undefined-behavior checks remain enabled.
The GEMix pipe starts Rasta locally and shares its framebuffer with the
container. OPEN LOOK and Window Maker synchronize and build the source in
their desktop guests, then run GDB against display `:0`.

### Development-host preparation

The development host needs:

- VS Code with the Microsoft C/C++ extension for `cppdbg` configurations.
- CMake, Docker, GDB, SSH, SCP, rsync, and libvirt's `virsh`.
- TCP port 31337 available on loopback for the Windows VM SSH debug tunnel.
- Permission to use Docker and to start the required libvirt domains.
- The Rasta executable at `/home/tstih/data/tstih/rasta/bin/rasta`, or a
  different executable selected through `RASTA_BIN`.
- The Docker images named by the top-level CMake project. In particular, the
  GEMix image must contain its runtime resources; Native does not mount a
  repository-owned resource directory.

### Windows VM preparation

The Windows library propagates a Common Controls v6 manifest resource into
consuming executables. This enables operating-system visual styles, not a
library-painted skin. The backend uses Windows 7-or-newer API declarations.

**Debug Vision (Windows 11 VM)** retains the `docker-win` Debug build and
deploys its executable, MinGW DLLs and native Win64 GDBserver through SSH.
The `Windows` guest is identified by UUID so a name change does not break F5.
`Native-Vision-Debug` is an interactive-user scheduled task: Vision runs on
the visible Windows desktop, not SSH's session 0 and not Wine. The debugger
wrapper owns a loopback SSH tunnel and cleans up only its deployed debug
processes when GDB exits; a post-debug task also handles forced adapter
termination. It leaves Windows running. Source breakpoints work, but
interrupting a running GUI remains an open debugger issue; close Vision's
window to end a running session cleanly.

The one-time key-only SSH setup, host-key verification, guest firewall rule,
connection overrides and exact deployed paths are documented in
[Windows VM Runtime](../../notes/WINDOWS-VM-RUNTIME.md). `prepare.sh` copies
the exact runtime DLLs reported by the Docker cross-compiler; no host Wine
installation is involved. The original Wine helper scripts remain historical
tools and are not referenced by any F5 configuration or task.

### Tribblix OPEN LOOK preparation

The OPEN LOOK configuration expects:

- A libvirt domain named `Tribblix-OpenLook` at `192.168.122.28`.
- The SSH target `tomaz@192.168.122.28`, authenticated without an interactive
  password prompt.
- CMake, GCC/G++, rsync, the 32-bit XView and OLGX development files, and
  X11/Xrandr development files installed in the guest.
- The multilib GDB at `/usr/bin/amd64/gdb`. It debugs the 32-bit executable;
  `/usr/bin/gdb` cannot handle the current Tribblix procfs interface reliably.
- A logged-in `olvwm` or `olwm` session using display `:0`, with
  `/export/home/tomaz/.Xauthority` available.
- A writable `/export/home/tomaz/Projects/` directory.

F5 starts the guest if necessary, waits for SSH, synchronizes the source into
`/export/home/tomaz/Projects/native`, configures the 32-bit
`build/tribblix-openlook-debug` tree, builds Vision, and starts remote GDB. The
task keeps Debug symbols and GCC warnings but disables the sanitizer bundle,
which is unavailable for this Tribblix configuration. Tribblix installs XView
and OLGX without `pkg-config` files; CMake falls back to their system headers
and libraries. Synchronization uses `--delete`, so files changed only in the
guest project copy are not preserved.

### Bookworm Window Maker preparation

The Window Maker configuration expects:

- A libvirt domain named `Bookworm-WindowMaker` and an SSH alias named
  `whiskey` for `tomaz@192.168.122.99`, authenticated without an interactive
  password prompt.
- CMake, GCC/G++, GDB, rsync, `pkg-config`, the WINGs/wraster development
  files, X11/Xrandr development files, and the Pango development files.
- A logged-in Window Maker session using display `:0`, with
  `/home/tomaz/.Xauthority` available.

On the current Bookworm guest the development prerequisites are installed with:

```bash
sudo apt-get update
sudo apt-get install -t bookworm-backports \
  cmake pkg-config libwings-dev libwraster-dev libxrandr-dev
sudo apt-get install gdb rsync libpango1.0-dev
```

The explicit Pango package is required because the configured Window Maker
repository's WINGs development package exposes Pango headers in `WINGs.pc` but
does not declare that development dependency. F5 starts the guest if necessary,
waits for SSH, creates `/home/tomaz/Projects/native`, synchronizes the source,
builds `build/bookworm-wmaker-debug`, and starts remote GDB on display `:0`.
Leak reporting is disabled for the debug launch because the WINGs/font stack
retains process-lifetime allocations. Synchronization uses `--delete`;
remote-only source changes are not preserved.

### Tribblix CDE preparation

The OpenMotif configuration expects:

- A libvirt domain named `Tribblix-CDE`.
- The SSH target `tomaz@charlie`, authenticated without an interactive
  password prompt.
- CMake, GCC/G++, the 64-bit GDB at `/usr/bin/amd64/gdb`, and rsync installed
  in the guest.
- The 64-bit X11, Xt, and Motif development libraries under `/usr/lib/amd64`.
- A logged-in CDE session using display `:0`.
- A writable `/export/home/tomaz/Projects/` directory.

F5 starts the guest if necessary, waits for SSH, synchronizes the source into
`/export/home/tomaz/Projects/native`, configures a 64-bit
`build/tribblix-openmotif-debug` tree, builds Vision, and starts remote GDB.
The target remains a Debug build with GCC warning flags. The task disables the
project's sanitizer bundle because Tribblix GCC does not support ASan for this
64-bit configuration.
The synchronization uses `--delete`; files changed only in the remote project
copy are not preserved.

### Haiku preparation

The Haiku configuration expects:

- A libvirt domain named `Haiku` with the fixed address `192.168.122.90`.
- The SSH target `user@192.168.122.90`, authenticated without an interactive
  password prompt.
- GDB at `/boot/system/bin/gdb` and the SSH server enabled in the guest.
- A graphical Haiku session for the same user.

F5 starts the guest if necessary, waits for SSH, cross-builds the Debug binary,
creates `/boot/home/Projects/native/run`, copies Vision there, and starts GDB
inside the guest. Source files stay on the development host; the debug symbols
retain their absolute host paths so VS Code can open them locally.

### macOS preparation

The macOS configuration expects `tomaz@leia` to accept key-based SSH and the
Mac to have an active graphical login. Remote Login must be enabled. The Mac
needs rsync, Clang, LLDB, and CMake either in Homebrew, `/usr/local/bin`, or the
CMake application bundle.

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
  Tribblix, Window Maker in Bookworm, the native Windows VM, Haiku deploy-and-run over
  SSH, and Apple on the configured remote host.
