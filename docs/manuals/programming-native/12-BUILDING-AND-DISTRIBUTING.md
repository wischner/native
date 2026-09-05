# Chapter 12: Building, Linking, and Distributing

Native has nine supported build selections: six Linux toolkits plus Windows,
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
| Linux SDL2 | Executable and assets | SDL2, X11, PNG/JPEG, GCC runtime, and optional SDL2_ttf |
| Linux OpenMotif | Executable and assets | Xm, Xt, X11, PNG/JPEG, and GCC runtime |
| Linux OPEN LOOK | Executable and assets | XView, OLGX, libtirpc, X11/Xrandr, PNG/JPEG, and GCC runtime |
| Linux Window Maker | Executable and assets | WINGs, WUtil, wraster, Pango/Xft, Fontconfig/Freetype, X11/Xrandr, PNG/JPEG, and GCC runtime |
| Linux GEMix | Executable, assets, and matching GEMix/Rasta runtime when it is not installed system-wide | AES, VDI, Rasta resources, PNG/JPEG, and GCC runtime |
| Windows MinGW | Executable, assets, and three MinGW runtime DLLs | Windows system DLLs and GDI+ |
| Haiku | Executable or Haiku package and assets | Haiku Be, Tracker, Translation, and root system libraries |
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

All six Linux selections require:

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
built-in bitmap stock-font fallback. Typical Debian-family packages are
`libsdl2-dev`, optional `libsdl2-ttf-dev`, `libx11-dev`, `libpng-dev`, and
`libjpeg-dev`.

Portable fonts created from a file or memory do not depend on SDL2_ttf; they
use Native's vendored TrueType rasterizer.

### Link libraries

The `native` target links:

- SDL2.
- Xlib, used for full X11 clipboard integration.
- SDL2_ttf only when it was detected while configuring.
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
- SDL2_ttf when the configure log says it was found and linked.
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

The maintained interactive debug path performs a native 32-bit build in the
`Tribblix-OpenLook` VM and runs it on the guest's logged-in OPEN LOOK desktop.
The Docker target remains the reproducible build and isolated Xephyr smoke path.

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

The maintained interactive debug path performs a native Debug build in the
`Bookworm-WindowMaker` VM and runs it on the guest's logged-in Window Maker
desktop. The guest also needs `libpango1.0-dev` when its WINGs package metadata
names Pango include paths without declaring that development dependency.

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

- `gemix-aes`
- `gemix-vdi`

### Link libraries

The `native` target links the GEMix AES and VDI libraries selected by those
modules, plus libpng and libjpeg. The current Docker artifact resolves
`libaes.so`, `libvdi.so`, and `librasta.so`, together with the common Linux
libraries.

The separate `docker-gemix-gemd` target selects `GEMIX_USE_GEMD=ON` and
links `libgem` instead of AES/VDI. It requires a matching `gemd` server;
the **Linux GEMix, gemd proxy** debug configuration starts one automatically.
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
remove, or statically link compiler runtimes. Wine is useful for development
tests but is not a dependency of a Windows deployment.

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
workflow; that script currently creates a Debug build.

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
