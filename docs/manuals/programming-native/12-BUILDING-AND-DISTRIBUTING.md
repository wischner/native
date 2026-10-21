# Chapter 12: Building, Linking, and Distributing

Native has eleven build selections: eight Linux toolkits plus Windows,
Haiku, and macOS. Application source remains the same, but each selection has
its own compiler, link closure, and deployment requirements.

This chapter distinguishes three different lists:

- **Build dependencies** provide compilers, headers, CMake packages, and
  import or static libraries.
- **Link libraries** are the direct libraries named by Native's CMake target.
- **Runtime dependencies** must be present on the target machine. A package
  can declare them as system requirements or, when their licenses and ABI
  permit it, bundle them in an application-specific runtime.

## Application CMake target

Applications include only the portable API:

```cpp
#include <native.h>
```

Within this source tree, define an executable after `lib/native` has defined
the library and link the CMake target rather than a bare archive:

```cmake
add_executable(my_application
    main.cpp
)

target_link_libraries(my_application PRIVATE native)
```

`native` publishes the `include/` directory, requires C++20, and carries the
selected backend's link closure. Do not repeat toolkit headers in application
source. Avoid linking `build/.../lib/native/libnative.a` by hand: a static
archive does not contain libpng, X11, GDI+, or the other libraries it calls,
and manual link order differs by toolchain.

The repository does not currently install or export a standalone CMake
package. Consumer executables are therefore built in-tree, like `vision`, or
from a parent project that adds the Native source target.

Portable applications define `program(int, char **)`. The selected backend
provides the actual process entry point and calls `program()` after its native
startup work.

## Reproducible Docker builds

Linux hosts can drive all Linux toolkit builds and the Windows and Haiku
cross-builds through the repository's Docker images. Configure the host
control tree once:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release
```

Then select a target:

```bash
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
cmake --build build/cmake --target docker-openlook
cmake --build build/cmake --target docker-wmaker
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-gemix-gemd
cmake --build build/cmake --target docker-win
cmake --build build/cmake --target docker-haiku
```

The control build type is forwarded to the backend build. The images and
outputs are:

| Target | Docker image | Output |
| --- | --- | --- |
| `docker-x11` | `wischner/gcc-x86_64-linux-x11:latest` | `build/linux-x11/src/vision` |
| `docker-sdl2` | `wischner/gcc-x86_64-linux-sdl:latest` | `build/linux-sdl2/src/vision` |
| `docker-openmotif` | `wischner/gcc-x86_64-linux-motif:latest` | `build/linux-openmotif/src/vision` |
| `docker-openlook` | `wischner/gcc-x86_64-linux-openlook:latest` | `build/linux-openlook/src/vision` (run through `scripts/linux/toolkit-session-run.sh openlook`) |
| `docker-wmaker` | `wischner/gcc-x86_64-linux-window-maker:latest` | `build/linux-wmaker/src/vision` (run through `scripts/linux/toolkit-session-run.sh wmaker`) |
| `docker-gemix` | `wischner/gcc-x86_64-gemix:latest` | `build/linux-gemix/src/vision` |
| `docker-gemix-gemd` | `wischner/gcc-x86_64-gemix:latest` | `build/linux-gemix-gemd/src/vision` |
| `docker-win` | `wischner/gcc-x86_64-windows-mingw-w64:latest` | `build/windows-mingw-w64/src/vision.exe` |
| `docker-haiku` | `wischner/gcc-x86_64-haiku:1.1.0` | `build/haiku/src/vision` |

The Docker image supplies build dependencies only. It does not make its shared
libraries appear on an end user's machine.

## Deployment summary

| Target | Application payload | Runtime that must be present |
| --- | --- | --- |
| Linux X11 | Executable and assets | X11, Xaw7, Xt, pixman, Xrandr, PNG/JPEG, and GCC runtime; optional Zenity or KDialog desktop integration |
| Linux SDL2 | Executable and assets | SDL2, X11, PNG/JPEG, GCC runtime, optional SDL2_ttf and Fontconfig |
| Linux OpenMotif | Executable and assets | Xm, Xt, X11, PNG/JPEG, and GCC runtime |
| Linux OPEN LOOK | Executable and assets | XView, OLGX, libtirpc, X11/Xrandr, PNG/JPEG, and GCC runtime |
| Linux Window Maker | Executable and assets | WINGs, WUtil, wraster, Pango/Xft, Fontconfig/Freetype, X11/Xrandr, PNG/JPEG, and GCC runtime |
| Linux GEMix | Executable, assets, and matching GEMix/Rasta runtime when it is not installed system-wide | AES, VDI, Rasta resources, PNG/JPEG, and GCC runtime |
| Windows MinGW | Executable, assets, and three MinGW runtime DLLs | Windows system DLLs and GDI+ |
| Haiku | Executable or Haiku package and assets | Haiku Be, Tracker, Translation, and root system libraries; optional Mesa OSMesa for GLSL |
| macOS | Complete `.app` bundle and assets | macOS system frameworks |

For Linux packages, “must be present” normally means declaring a package
dependency. For a self-contained archive, it means bundling the permitted
non-system libraries and configuring the loader to find them. The detailed
sections below name the direct links and observed runtime libraries.

## Release and debug runtimes

Hosted GCC Debug builds enable AddressSanitizer and UndefinedBehaviorSanitizer
when `NATIVE_ENABLE_DEVELOPER_CHECKS` is on. Those builds require the matching
`libasan` and `libubsan` at runtime and are for development. Build distributed
binaries with `CMAKE_BUILD_TYPE=Release`; sanitizer runtimes are then absent.

Native itself is a static library. A normally linked application therefore
ships no separate `libnative` file, but it still depends on the backend's
shared libraries. Always inspect the final artifact built in the release
environment:

```bash
readelf -d build/linux-x11/src/vision
objdump -p build/windows-mingw-w64/src/vision.exe
otool -L build/macos-release/src/vision.app/Contents/MacOS/vision
```

On Linux, `ldd` gives a convenient resolved view on a compatible build host.
Package against the oldest distribution ABI you intend to support. Exact
SONAME versions, especially libjpeg and the C++ runtime, follow the build
image and target distribution.

## Linux dependencies shared by all toolkits

The six original Linux selections require:

- CMake 3.20 or newer and a C++20 compiler.
- `pkg-config` where the selected toolkit CMake file uses it.
- libpng development headers and library.
- libjpeg development headers and library.

The common Linux layer links `PNG::PNG` and `JPEG::JPEG`. A dynamically linked
release must have the compatible libpng and libjpeg SONAMEs, and libpng also
requires zlib. Normal GCC runtime requirements include libstdc++, libgcc,
libc, and libm.

For a distribution package, declare these runtime packages instead of copying
random `.so` files from the build container. For a self-contained packaging
format, bundle the permitted non-base libraries using that format's loader and
RPATH rules, and verify the result on a clean target system.

## Linux X11 with Athena Widgets

### Compile

Use the Docker target above, or configure a host that has all development
packages installed:

```bash
cmake -S . -B build/linux-x11-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=X11
cmake --build build/linux-x11-release --parallel
```

The build requires the CMake X11 package plus these `pkg-config` modules:

- `xaw7`
- `xt`
- `pixman-1`
- `xrandr`

On Debian-family systems, the corresponding development packages are commonly
`libx11-dev`, `libxaw7-dev`, `libxt-dev`, `libpixman-1-dev`,
`libxrandr-dev`, `libpng-dev`, and `libjpeg-dev`. Package names vary by
distribution; the CMake and `pkg-config` names above are authoritative.

### Link libraries

The `native` target links:

- Xlib from `find_package(X11)`.
- Athena Widgets (`Xaw7`) and Xt.
- pixman-1 and Xrandr.
- libpng and libjpeg.

Xaw and Xlib bring in their normal X11 support libraries. The current
GCC-linked artifact resolves `libX11`, `libXext`, `libXaw.so.7`, `libXt`,
`libpixman-1`, `libXrandr`, `libSM`, and `libICE`, in addition to the common
Linux libraries.

The repository's X11 F5 profile runs the Docker-built program under GDB in
a monochrome TWM desktop inside Xephyr. The launcher picks a free nested
display, loads display-local resources and cleans up that session when GDB
ends. This is a development setup, not a requirement for distributed X11
applications. See [Build System](../book-of-native/BUILD-SYSTEM.md#monochrome-x11-runtime-session)
for the scaled session and standalone launch command.

### Distribute

The target system must provide:

- The compatible X11, Xaw7, Xt, pixman, and Xrandr shared libraries.
- The common libpng, zlib, libjpeg, and compiler runtime libraries.
- A working X server and desktop session.
- Optionally `zenity` or `kdialog` for a desktop-integrated file chooser;
  the Xaw chooser remains available without either helper.

The Athena backend uses native Xaw menu, button, check, radio, list, and text
widgets. Those widget libraries are runtime requirements, not code copied into
the application. Declare the dependencies in an RPM/DEB package or include
them through the chosen self-contained Linux packaging format.

## Linux SDL2

### Compile

```bash
cmake -S . -B build/linux-sdl2-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=SDL2
cmake --build build/linux-sdl2-release --parallel
```

The build requires SDL2 development files and X11 development files.
SDL2_ttf is optional: CMake uses it when found and otherwise enables the
built-in bitmap stock-font fallback. Fontconfig is also optional and discovered
through pkg-config; stock-role lookup calls its C API directly. Without it,
explicit font paths are tried and missing stock faces use the bitmap fallback.
Typical Debian-family packages are libsdl2-dev, optional libsdl2-ttf-dev and
libfontconfig1-dev, libx11-dev, libpng-dev and libjpeg-dev.

Portable fonts created from a file or memory do not depend on SDL2_ttf; they
use Native's vendored TrueType rasterizer.

### Link libraries

The `native` target links:

- SDL2.
- Xlib, used for full X11 clipboard integration.
- SDL2_ttf only when it was detected while configuring.
- Fontconfig only when its pkg-config target was detected.
- libpng and libjpeg.

The CMake link command also receives the X11 support libraries reported by
`FindX11`, commonly Xext, SM, and ICE. With the current host Release linker,
unused support libraries are dropped and the executable records only
`libSDL2-2.0` and `libX11` directly, plus the common Linux libraries. Inspect
the packaged artifact because linker `--as-needed` behavior can differ.

### Distribute

The target system must provide:

- A compatible SDL2 shared library.
- X11 client libraries used by the clipboard backend.
- SDL2_ttf and Fontconfig when the configure log says they were linked.
- The common libpng, zlib, libjpeg, and compiler runtime libraries.
- Optionally `zenity` or `kdialog` for desktop-integrated file dialogs.

An application that requires usable SDL2 file dialogs must declare or bundle
one of those helper programs. Without either helper, `show()` completes the
dialog as cancelled and restores owner input; missing optional integration is
not an exception.

SDL selects its video driver at runtime. Full cross-application image
clipboard support is available with the X11 driver. On another SDL video
driver, the PNG fallback remains available only inside the current process.

## Linux OpenMotif

### Compile

```bash
cmake -S . -B build/linux-openmotif-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=MOTIF
cmake --build build/linux-openmotif-release --parallel
```

The build requires X11, Xt, and Motif development files. Xt is located through
`pkg-config`; Motif is found by CMake's `FindMotif` module. A typical
Debian-family set is `libx11-dev`, `libxt-dev`, `libmotif-dev`, `libpng-dev`,
and `libjpeg-dev`.

### Link libraries

The `native` target links:

- Xlib.
- Motif (`Xm`) and Xt.
- libpng and libjpeg.

The current artifact resolves `libXm.so.4`, `libXt`, `libX11`, `libXext`,
`libSM`, and `libICE`, plus the common Linux libraries.

### Distribute

The target system must provide compatible Motif, Xt, and X11 shared libraries,
the common image and compiler runtimes, and an X server. File selection uses
`XmFileSelectionBox`, so Zenity and KDialog are not required.

Motif's shared-library ABI and package name vary across OpenMotif and operating
system releases. Resolve the exact dependency from the release executable on
the oldest supported target rather than assuming `libXm.so.4` everywhere.

## Linux OPEN LOOK with XView

### Compile

The reproducible build uses the OPEN LOOK image shown above. A host with
matching XView and OLGX development files can configure the same backend:

```bash
cmake -S . -B build/linux-openlook-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=OPENLOOK
cmake --build build/linux-openlook-release --parallel
```

CMake uses the `xview` and `olgx` `pkg-config` modules when available, and
falls back to standard system include and library discovery for Tribblix. It
also requires the `xrandr` module and the CMake X11 package. XView requires
libtirpc headers on current Linux systems. The reference image installs the
historical toolkit below `/usr/openwin`, publishes its include and library
paths through those modules, and supplies GCC 11 with C++20 support. It is the
authoritative build environment when a host distribution has no maintained
XView packages.

The maintained F5 profile builds with `docker-openlook` and runs GDB in the
same image through `openlook-xephyr`, with Xephyr and olwm supplied by the
image. Run without a debugger using:

```bash
./scripts/linux/toolkit-session-run.sh openlook ./build/linux-openlook/src/vision
```

### Link libraries

The `native` target links:

- XView and OLGX.
- Xlib and Xrandr.
- libtirpc through the XView package metadata.
- libpng and libjpeg through the common Linux image layer.

The current executable directly resolves `libxview.so.1`, `libolgx.so.1`,
`libtirpc.so.3`, `libX11.so.6`, `libXrandr.so.2`, `libpng16.so.16`, and
`libjpeg.so.8`. Their dependency closure brings in Xext, Xrender, XCB, Xau,
Xdmcp, RPC authentication libraries, zlib, and the normal compiler runtime.
Inspect the Release artifact because exact SONAMEs follow the target
distribution.

Application code still includes only `<native.h>`. XView, OLGX, Xlib, and
libtirpc headers are private backend build dependencies and must not appear in
the portable application interface.

### Distribute

The target installation must provide ABI-compatible XView, OLGX, libtirpc,
X11/Xrandr, PNG/JPEG, zlib, and compiler runtime libraries, plus a working X
server. Install the libraries through distribution packages when possible;
otherwise bundle the permitted non-system libraries under an application
loader path and include their licenses. Do not copy a random subset of
`/usr/openwin/lib`: verify the final dependency closure with `ldd` on a clean
target.

Any ICCCM window manager can host the application. Distribute OpenWindows
`olwm` when the complete OPEN LOOK desktop behavior is required. The backend
uses native XView Panel controls, OpenMenu menus, `File_chooser`, and Selection
objects, and OLGX theme primitives. It does not require Zenity or KDialog.
Historical Lucida X server fonts are optional; when they are unavailable the
backend installs compatible core-font resource fallbacks before creating
XView controls.

## Linux Window Maker with WINGs

### Compile

Use `docker-wmaker`, or configure a host with the Window Maker development
files installed:

```bash
cmake -S . -B build/linux-wmaker-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=WMAKER
cmake --build build/linux-wmaker-release --parallel
```

CMake requires the `WINGs` and `xrandr` `pkg-config` modules and the CMake X11
package. The reference image contains Window Maker 0.96, the WINGs, WUtil, and
wraster headers and shared libraries, and the Pango/Xft font stack used by
that WINGs build. It is the reproducible environment when a host distribution
does not package the WINGs development files separately.

The **Linux Window Maker, Xephyr** F5 profile builds with `docker-wmaker`
and starts GDB in the same image using `window-maker-xephyr`. The image
supplies Xephyr and Window Maker. Run the compiled program directly with:

```bash
./scripts/linux/toolkit-session-run.sh wmaker ./build/linux-wmaker/src/vision
```

### Link libraries

The `native` target links:

- WINGs, WUtil, and wraster through the `WINGs` package metadata.
- Xlib and Xrandr.
- libpng and libjpeg through the common Linux image layer.

The current artifact directly resolves `libWINGs.so.3`, `libWUtil.so.5`,
`libwraster.so.6`, `libX11.so.6`, `libXrandr.so.2`, `libpng16.so.16`, and
`libjpeg.so.8`. The WINGs closure also includes Pango, Xft, Fontconfig,
Freetype, HarfBuzz, GLib, and supporting X11 libraries. The reference wraster
build enables several foreign image loaders, including ImageMagick, TIFF,
WebP, GIF, and XPM; those libraries therefore also appear in its runtime
closure even though Native decodes its portable PNG and JPEG images directly.
Inspect the Release executable because these optional wraster dependencies
depend on how Window Maker was built.

### Distribute

The target system must provide ABI-compatible WINGs, WUtil, wraster,
X11/Xrandr, font-stack, image-codec, and compiler runtime libraries. Declare
the complete closure reported by the packaged Release executable, or bundle
the permitted non-system libraries with suitable loader paths and licenses.
Do not copy only the three Window Maker libraries from the build image: their
font and image-loader dependencies are required as well.

The Window Maker executable itself is not needed merely to load WINGs, and an
ICCCM window manager can host the application. Install Window Maker when the
intended desktop behavior and decoration are part of the product. The backend
uses native WINGs windows, command/switch/radio buttons, lists, text widgets,
scrollers, standard open/save panels, and selection handlers. Application
menus use a persistent context-style popup because the WINGs popup button is a
press-drag selector rather than a normal desktop menu. Custom theme primitives
use WINGs colors, fonts, relief, and indicator pixmaps; the reference Window
Maker session also aligns the base panel gray to its inactive-title gray.
Zenity and KDialog are not required.

## Linux GEMix

Both GEMix build targets use the published `wischner/gcc-x86_64-gemix:latest`
image, which already contains the GEM `v1.0.0` SDK built from commit
`0df463d9ff4b4cbbdd0612ce700c9e11e5e33661`: headers and Rasta-backend
libraries in `/usr/local` with `gemix-*` pkg-config files, plus `gemd`,
`resgen`, the GEM resources, and the recorded revision in
`/opt/gemix/share/metadata/gem.version`. Docker pulls the image on first use;
no local SDK image build and no sibling GEM checkout is needed.

### Compile

The Docker target is the normal build path because its image contains the
GEMix headers, libraries, and Rasta environment. A host build uses:

```bash
cmake -S . -B build/linux-gemix-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DTOOLKIT=GEMIX
cmake --build build/linux-gemix-release --parallel
```

In addition to the common Linux dependencies, `pkg-config` must resolve:

- `gemix-aes >= 1.0.0`
- `gemix-vdi >= 1.0.0`

### Link libraries

The `native` target links the GEMix AES and VDI libraries selected by those
modules, plus libpng and libjpeg. The current Docker artifact resolves
`libaes.so`, `libvdi.so`, and `librasta.so`, together with the common Linux
libraries.

The separate `docker-gemix-gemd` target selects `GEMIX_USE_GEMD=ON` and
links `libgem` instead of AES/VDI. It requires a matching `gemd` server;
the **GEM, Rasta@SDL2** debug configuration starts the server and the
image-provided SDL2 Rasta viewer automatically inside the Docker container.
The viewer and server use 32-bit output with inversion off so AES chrome
and portable content share white paper and black ink. Closing the viewer
terminates the owned debugger, Vision and server session.
The local GEMix launch configuration and host-checkout runner are removed.
Deploy `libgem` and its runtime dependencies to the client, and AES/VDI/Rasta
and resources to the server. Both processes must see the same file-selector
and clipboard paths. This changes transport, not the public Native API.

### Distribute

A runnable GEMix package must provide:

- The executable and application-owned assets.
- ABI-compatible GEMix `libaes`, `libvdi`, and Rasta runtime libraries.
- The GEMix/Rasta runtime resources and display environment supplied by the
  target installation.
- The common libpng, zlib, libjpeg, and compiler runtime libraries.

Native does not copy GEMix runtime resources out of the Docker image. Treat
the image's GEMix/Rasta distribution as a versioned runtime and deploy the
application against the matching target installation. AES supplies the file
selector; no desktop chooser process is required.

## Windows with MinGW-w64

### Compile

Use `docker-win`, or install a 64-bit MinGW-w64 C++ toolchain and configure
with the repository toolchain file:

```bash
cmake -S . -B build/windows-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/scripts/cmake/toolchain-mingw.cmake"
cmake --build build/windows-release --parallel
```

The toolchain selects `x86_64-w64-mingw32-g++` and the
`/usr/x86_64-w64-mingw32` sysroot. It needs CMake, MinGW-w64 headers and
import libraries, and a C++20-capable MinGW compiler. No libpng, libjpeg, or
third-party widget development package is used.

### Link libraries

The `native` target links these Windows import libraries:

- `user32`
- `gdi32`
- `msimg32`
- `advapi32`
- `shell32`
- `ole32`
- `gdiplus`

They supply windowing and controls, graphics and alpha blending, registry font
enumeration, shell/common dialogs, COM clipboard services, and PNG/JPEG
codecs.

### Distribute

Ship `vision.exe`, or the renamed application executable, plus its
application-owned data. Do not copy Windows system DLLs. Windows provides
`KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `MSIMG32.dll`, `ole32.dll`,
`gdiplus.dll`, `msvcrt.dll`, and any shell components used by the program.

The current MinGW build uses the shared compiler runtime. Place these three
DLLs beside the executable, or install them through the application's runtime
installer:

- `libstdc++-6.dll`
- `libgcc_s_seh-1.dll`
- `libwinpthread-1.dll`

Locate the exact copies belonging to the compiler that built the executable:

```bash
x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll
x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll
x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll
```

Recheck every release with `objdump -p`; changing toolchain flags can add,
remove, or statically link compiler runtimes. The maintained F5 workflow
deploys the Docker-built executable and these runtime DLLs to the native
Windows 11 VM (`Windows 11, VM` F5 profile, `ssh whiskey`), with an
interactive desktop debug server and SSH tunnel. Preparation uses host Python
3.11+ to send changed files, keeps the full local executable for symbols and
caches loaded Windows DLLs under `build/windows-vm/` for repeat launches.
Neither execution nor debugging uses Wine. See
[Windows VM Runtime](../../notes/WINDOWS-VM-RUNTIME.md) for the one-time guest
setup and debugger lifecycle.

## Haiku

### Compile

Use `docker-haiku`, or install the 64-bit Haiku cross compiler used by the
toolchain file:

```bash
cmake -S . -B build/haiku-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/scripts/cmake/toolchain-haiku.cmake"
cmake --build build/haiku-release --parallel
```

The toolchain locates `x86_64-unknown-haiku-g++` and asks it for the matching
sysroot. A native build on Haiku can omit the toolchain file and use Haiku's
system compiler.

### Link libraries

The `native` target links:

- `be` for the Application/Interface Kit.
- `tracker` for standard file panels.
- `translation` for image codecs.

The current cross-built executable reports runtime dependencies on
`libbe.so`, `libtracker.so`, `libtranslation.so`, and `libroot.so` only.

### Distribute

Copy the executable and application-owned assets to an ABI-compatible Haiku
installation, or place them in a Haiku package. The four libraries above are
operating-system components and should be required from Haiku rather than
copied beside the program. Build against the oldest Haiku ABI supported by the
application and verify the executable on a clean target system.

Fonts, controls, clipboard, file panels, and PNG/JPEG translation are supplied
through the Haiku system kits. No separate toolkit or codec library is shipped
with the application.

## macOS

The macOS remote build checks Apple Command Line Tools and CMake before
source synchronization, including `~/Applications/CMake.app` in its search
path. CMake probes `std::jthread`/`std::stop_token` and enables libc++
`-fexperimental-library` on Apple SDKs only when required.

### Compile

Build on macOS with CMake, Xcode Command Line Tools, and a C++20-capable Apple
Clang. Objective-C++ is enabled automatically:

```bash
cmake -S . -B build/macos-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF
cmake --build build/macos-release --parallel
```

The output is the application bundle
`build/macos-release/src/vision.app`. The repository also provides
`scripts/macos/remote/build.sh` for its configured remote-Mac development
workflow on VM `mike` (password-free `ssh mike`); that script creates a
Debug build. VS Code exposes **macOS, VM** with LLDB.

### Link libraries

The `native` target links these system frameworks:

- AppKit
- Foundation
- CoreFoundation
- CoreGraphics
- ImageIO
- UniformTypeIdentifiers

They provide windows and controls, fonts and drawing, pasteboards and panels,
and PNG/JPEG codecs.

### Distribute

Distribute the complete `.app` bundle, not only its Mach-O executable. Keep
application-owned fonts, images, and other resources inside the bundle.
Apple's system frameworks are provided by macOS and must not be copied into
the application.

The current CMake target creates the basic bundle but does not install
application resources, sign it, notarize it, or build an installer. A release
pipeline must add the resources and perform the signing and packaging required
by its chosen distribution channel. If later code links a third-party dynamic
library, embed that library in the bundle and update its install names; none
is required by the current Native macOS backend.

## Final deployment checklist

For every backend:

1. Build `Release` with the same toolchain and oldest ABI intended for users.
2. Run the automated tests in that build tree when the target can execute
   them.
3. Inspect the final executable's dynamic dependency list.
4. Test on a clean target without development packages or build-container
   mounts.
5. Ship the executable or application bundle and all application-owned assets.
6. Bundle or declare every non-system runtime dependency listed above.
7. Exercise menus, controls, both file dialogs, PNG and JPEG, installed and
   embedded fonts, clipboard text and images, and single-line and multiline
   editing on the packaged result.

Return to the [manual contents](../PROGRAMMING-NATIVE.md), or continue with
the [Book of Native](../BOOK-OF-NATIVE.md) for backend implementation details.

## Native as a CMake dependency

A parent project can `add_subdirectory()` the Native checkout and link its
`native` target. The dependency build configures the library, without Vision,
Native tests or host Docker orchestration. The parent supplies its own
application `program()` and backend selection/toolchain and runs its build
inside the corresponding Docker image, mounting both source checkouts.
No installed/exported package is provided. Do not reproduce Native's toolkit,
codec or audio link closure in the application. Non-SDL Linux sound loads
`libasound.so.2` at runtime; missing ALSA/default output is reported by
`audio_out::open() == false`. Windows uses WASAPI, macOS links AudioToolbox,
and Haiku links the media kit. Audio dependencies remain private to Native.

Programmable image-shader packages are generated once with the optional
standard-library Python 3 tool `scripts/shaders/package.py`. Distribute the same
`.nshader` file for every backend, or embed its bytes and decode them. The
implemented CPU profile needs no GPU SDK, target-specific shader compiler or
runtime translator. Ordinary Native builds already compile its shared executor.
Package/profile limits and fallback are documented in
[Portable image shaders](20-IMAGE-SHADERS.md).

Vision's CRT terminal acceptance mode loads external shader/font assets from
`retro-terminal/` beside the binary (or `--terminal-assets PATH`). CMake copies
that directory; Windows VM and Haiku VS Code deployment also include it. Retain
the GPL shader sources/license and CC-BY-SA font attribution/license described
in its asset README. Python packages these developer assets; Qt is not needed
at runtime. The library itself does not embed them.


## AmigaOS qualification status

The eleven backend selections do not include AmigaOS. The
separate `docker-amiga-check` target checks the proposed 3.1/m68k compiler
runtime and currently fails for C++20 threading and filesystem support. It
creates no `native` archive, Vision executable, application link target or
deployable Amiga package. See [Build system](../book-of-native/BUILD-SYSTEM.md)
and [ADJUST.md](../../ADJUST.md) for the check and measured prerequisites.

### Haiku GLSL runtime selection

The portable CPU shader profile has no OpenGL dependency. The separate GLSL
profile loads the optional OSMesa runtime at first use; Mesa development files
are not needed by the cross-build. The tested Haiku Mesa 22.0.5 package supplies
libOSMesa.so.8. Its llvmpipe vertex JIT faults on the configured VM; select
GALLIUM_DRIVER=softpipe in the launching process for verified execution.
The Haiku F5 profile and GLSL test environment use this setting. An unsupported
runtime reports a diagnostic/fallback instead of assuming shader availability.

### Motif/CDE in Xephyr

Select **Linux Motif/CDE, Xephyr** for F5. The configuration builds with
`docker-openmotif` and runs GDB through the image's `cde-xephyr` helper in
`wischner/gcc-x86_64-linux-motif:latest`, which supplies CDE, Xephyr,
headers and libraries. No CDE VM or SSH setup is required.
For standalone execution:

```bash
./scripts/linux/toolkit-session-run.sh openmotif ./build/linux-openmotif/src/vision
```

The helper starts CDE services as container root on Docker's isolated
network, then runs the command as its CDE user. It forwards the host
X display or supplies a headless Xvfb parent. Debugging retains ASan/UBSan
with LeakSanitizer disabled under GDB.

## GTK 4.14 Wayland and elementary OS

The `WAYLAND` and `ELEMENTARY` selections both require GTK 4.14.x,
GtkSourceView 5.12, GLib/GIO, Cairo/Pango/HarfBuzz, xkbcommon, Fontconfig,
PNG/JPEG and EGL/Mesa. `ELEMENTARY` additionally links Granite 7.4 through
its C ABI. Applications continue to use only the pure C++ public API.

| Target | SDK image | Program |
| --- | --- | --- |
| `docker-wayland` | `native-wayland-sdk:24.04` | `build/linux-wayland/src/vision` |
| `docker-elementary` | `native-wayland-sdk:24.04` | `build/linux-elementary/src/vision` |

Both Docker targets depend on `docker-wayland-sdk`, which builds the local
image from `scripts/linux/wayland/Dockerfile`, including the elementary OS 8
stylesheet and icons. Build them through `build/cmake` like the other Docker
targets. **Linux GTK4, Wayland** forwards the host Wayland and available
D-Bus sockets without X11/Xauthority. **Elementary OS, VM** deploys to
`ssh echo` and uses its installed desktop theme. It is the only elementary
F5 profile.

Deploy with the same GTK/Granite/GtkSourceView major ABIs and native theme
assets. Top-level placement belongs to the compositor. See the book's
[Build System](../book-of-native/BUILD-SYSTEM.md#gtk-414-wayland-and-elementary-os)
for pinned SDK provenance and headless Weston commands, and the feature matrix
for advanced-table/custom-paint and interactive desktop acceptance limits.

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
