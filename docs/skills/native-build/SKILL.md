---
name: native-build
description: Build and package Native C++ applications for a selected platform and toolkit. Use for SDK setup, target selection, missing dependencies or SDL2 bitmap fonts, and preparing a distribution with its runtime libraries, fonts, assets, and licenses.
---

# Build and distribute for a Native target

Locate the Native checkout/version and consumer project, and read their
instructions. Resolve the links below in that checkout when this skill is
copied elsewhere. Use the current CMake files and
[build and distribution chapter](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md)
as the dependency authority; package names and library versions follow the
chosen SDK and destination distribution.

## Select the build and destination

Establish the requested OS, architecture, Linux toolkit when applicable,
destination OS version/ABI, and whether the result is a development build,
system package, or self-contained distribution. Reuse existing configuration;
ask only for missing choices that affect the artifact. Identify required font,
dialog, audio, and shader features before accepting optional dependencies as
absent. The SDK environment and end-user runtime are separate requirements.

Read only the selected target's section in the build chapter:

| Target | Repository build entry | Dependencies and distribution |
| --- | --- | --- |
| Linux SDL2 | `docker-sdl2`, `TOOLKIT=SDL2` | [SDL2](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-sdl2), including the font checks below |
| Linux X11/Athena | `docker-x11`, `TOOLKIT=X11` | [X11](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-x11-with-athena-widgets) |
| Linux OpenMotif | `docker-openmotif`, `TOOLKIT=MOTIF` | [Motif](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-openmotif) |
| Linux OPEN LOOK | `docker-openlook`, `TOOLKIT=OPENLOOK` | [XView](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-open-look-with-xview) |
| Linux Window Maker | `docker-wmaker` (builds `docker-wmaker-sdk`, image `native-wmaker-sdk:1`), `TOOLKIT=WMAKER` | [WINGs](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-window-maker-with-wings) |
| Linux GEMix | `docker-gemix`, `TOOLKIT=GEMIX`; `docker-gemix-gemd` also sets `GEMIX_USE_GEMD=ON` | [GEMix](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#linux-gemix), including client/server resources |
| Linux GTK Wayland | `docker-wayland`, `TOOLKIT=WAYLAND` | [GTK Wayland](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#gtk-414-wayland-and-elementary-os) |
| elementary OS | `docker-elementary`, `TOOLKIT=ELEMENTARY` | [GTK/Granite](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#gtk-414-wayland-and-elementary-os) and [picker schemas](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#picker-dependencies) |
| Windows x64 | `docker-win`, MinGW toolchain | [Windows](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#windows-with-mingw-w64) |
| Haiku x86_64 | `docker-haiku`, Haiku toolchain | [Haiku](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#haiku) and [picker libraries](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#picker-dependencies) |
| macOS | `scripts/macos/remote/build.sh` | [macOS](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#macos) and the [remote configuration](../../notes/MACOS-REMOTE-RUNTIME.md) |

Image names, toolchains, and output paths are in the chapter's
[Docker build table](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#reproducible-docker-builds).
Check architecture against the actual SDK; a toolkit name does not promise
arbitrary cross-compilation. Amiga is a runtime qualification check only,
not a supported application build target.

## Build with the selected SDK

For this repository, configure `build/cmake` and build the selected Docker
target. For example, a distributable SDL2 build uses:

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --target docker-sdl2
```

For a separate application, follow
[consumer integration](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#native-as-a-cmake-dependency):
mount both checkouts into the selected SDK, configure the parent project with
the matching `TOOLKIT` or toolchain file, and link the CMake target `native`.
Native's Docker targets build Vision, not an external consumer. There is no
installed/exported Native CMake package. Keep target build trees separate;
do not replace SDK builds with an ad hoc host build.

Validate shared C++ with GCC/Docker before the remote Apple SDK build. The
macOS helper currently builds Debug; use the documented remote environment
for a separate Release configuration when packaging. Do not label its Debug
bundle a release. Remote aliases in this repository are examples of configured
machines, not prerequisites available to every user.

## SDL2 stock fonts: check before accepting the build

For normal scalable stock text, require **both SDL2_ttf and Fontconfig in the
build SDK**, plus **Fontconfig configuration and usable font files in the
runtime**. They are optional to the library so reduced builds remain possible;
accept bitmap stock fonts only when that reduced behavior is intended.

Follow the chapter's [SDL2 font preflight](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#sdl2-font-preflight).
Verify `pkg-config` discovery in the actual SDK and `HAVE_SDL2_TTF` and
`HAVE_FONTCONFIG` on Native's SDL2 font compilation. Installing Fontconfig on
the host or adding its runtime later cannot enable code omitted at configure
time. If missing, correct the SDK dependencies, reconfigure and rebuild; do
not work around it with application font changes or private compile defines.

Check the staged package on its destination: Fontconfig must resolve readable
sans and monospace files, and the application's stock roles must not report
`Built-in Bitmap`. A working embedded/file font does not prove stock fonts
work; portable fonts use a separate rasterizer. A successful link or a dummy
video test does not prove the intended font appearance.

## Assemble and verify the distribution

Create a concrete payload/dependency manifest for the selected artifact:

- Executable or complete `.app` bundle, application assets, and their licenses.
  Vision's retro-terminal assets are required for that mode; do not impose
  them on unrelated consumers. Preserve paths expected by the application.
- Shared libraries to bundle versus target system packages to require, with
  ABI/version constraints. Inspect the final Release binary using `readelf`
  and target-compatible `ldd`, MinGW `objdump -p`, or macOS `otool -L`.
  Resolve transitive dependencies too; the static `native` archive does not
  absorb them. Do not ship development sanitizer runtimes as a release fix.
- Non-library resources: installed fonts and Fontconfig configuration for
  SDL2; GTK themes, icons and required schemas; GEMix resources and matching
  server when selected; chooser helpers when required. Include optional
  dynamically loaded audio/GLSL runtimes when the application needs them;
  they may not appear in the executable's dependency list.
- Platform runtime requirements from the selected chapter: matching MinGW
  DLLs plus the WinUI bootstrap and Windows App Runtime; Haiku system kits;
  or macOS system frameworks and any application bundle resources. Keep OS
  system libraries supplied by the OS. Packaging/signing is not automatically
  performed by Native's CMake build.

Stage under the project's `build/` tree using its established packaging
format. Verify the package on a compatible clean destination without SDK
mounts, developer fonts, or accidental loader paths. Exercise fonts, requested
dialogs, codecs, clipboard and application-specific features through
[native-verify](../native-verify/SKILL.md). Preserve diagnostics and report
unavailable target access as unverified scope.

Hand back exact build commands, SDK/toolchain and target, artifact location,
payload and runtime dependency manifest, font checks, and separate build,
runtime and visual results using the [handoff template](../../ai/REQUEST-TEMPLATE.md).
Keep work within the requested build/package/deployment scope; preparing a
distribution does not authorize publishing it.

## Destination closure and graphics capability

Inspect the final deployed directory, not only the SDK binary. GTK source
editing needs GtkSourceView and its transitive runtime dependencies; when
staging a private copy use a relocatable loader path and include its notices.
Check ordinary and fixed-width stock text on that destination. Native's SDL2
Fontconfig discovery uses CMake with pkg-config fallback; fix discovery in
Native rather than injecting its private feature defines from a consumer.

OpenGL 3.3 availability is a runtime check. Windows Basic Display Adapter may
fail it even when every linked DLL is present. An optional application-local
Mesa software driver must have a pinned source/version/checksum, appropriate
notices and a tested architecture. Keep it beside the application; do not
replace system DLLs. Report software rendering and measure performance with
the requested effect enabled. Haiku GLSL requires its separately qualified
OSMesa runtime and driver selection in the launch environment.

Record a dirty Native checkout explicitly in the handoff. Preserve existing
consumer work and compatibility requirements; do not claim that fixes tested
from a local checkout are already present in a remote dependency pin.
