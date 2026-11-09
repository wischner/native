Native binary SDK
=================

This archive contains one Release backend, the complete public include tree,
link dependencies and their non-OS runtime library closure. No Vision, demos,
test executables, compiler or third-party toolkit development headers are
included. Toolkit headers are private to Native; applications need only the
Native headers and a compatible C++20 compiler and target OS SDK.

Configure a consumer with -DCMAKE_PREFIX_PATH=/path/to/extracted/sdk:
For a Haiku cross-build, use -DNative_DIR=/path/to/sdk/lib/cmake/Native
because its toolchain restricts package searches to the target sysroot.

    find_package(Native CONFIG REQUIRED)
    add_executable(my_application main.cpp)
    target_link_libraries(my_application PRIVATE Native::Native)

Define program(int, char **) as in the Programming Native manual. The package
propagates C++20, link dependencies and the Windows Common Controls manifest.
manifest.json records the compiler, architecture, dependency sources, hashes,
OS-provided libraries and optional libraries unavailable in the build image.
Use a compiler compatible with the recorded C++ ABI. Windows archives target
MinGW-w64, not MSVC. macOS architectures are distributed separately.

Runtime deployment remains application-specific:
- Linux/Haiku: put lib on the runtime library search path or install the
  libraries in your application's supported layout. For a local Linux run,
  use LD_LIBRARY_PATH=/path/to/sdk/lib. Do not replace the OS's C runtime.
- Windows: copy bin/*.dll beside your executable. Install x64 Windows App
  Runtime 1.8 for WinUI settings navigation; its bootstrap DLL is included.
- macOS: system frameworks come from the target macOS installation.
- Haiku: the sysroot's OS libraries are included for linking. Their own
  runtime dependencies (including ICU and compiler runtimes) come from Haiku.

The target desktop, display server, graphics drivers, fonts/font configuration,
GTK schemas/themes/GtkSourceView language data, and GEM resources/server are
provided by the target environment. These SDKs are not desktop distributions.
Optional Haiku OSMesa is provided by the target installation. Optional Linux
audio/EGL/GLX libraries are bundled when present; their device/driver plugins
still come from the target installation. See the distribution chapter for
backend runtime setup. Native does not expose toolkit headers to consumers.

LICENSE covers Native. share/licenses preserves available third-party notices
from the SDK and embedded sources. The dependency manifest identifies the
exact library files for downstream redistribution and source-offer obligations.
