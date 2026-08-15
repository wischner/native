# Native Rules

This file is the working project brief for contributors and coding agents.
It describes how the library is structured, how Linux builds must be handled,
and what quality bar is expected when extending the codebase.

## Project intent

- `native` is a modern C++ UI library.
- The public surface must feel like pure C++, not like a wrapper around native APIs.
- Platform-specific and toolkit-specific code must stay behind the public interface.
- The library should remain small, readable, educational, and easy to reason about.

## Architecture

The library implementation is organized into three layers:

1. Core C++ layer
   - Located in `lib/native/`
   - Shared across all targets
   - Contains portable logic such as geometry, events, windows, app flow, and graphics abstractions

2. Platform layer
   - Located in `lib/native/platforms/`
   - Handles operating-system-specific integration
   - Examples: Windows, Linux, Haiku, macOS

3. Toolkit layer
   - Located in `lib/native/toolkits/`
   - Used when a platform needs a separate windowing or rendering backend
   - Examples on Linux: `x11`, `sdl2`, `openmotif`

The `src/` directory contains the `vision` application. It is a consumer of
the public library API and must not contain private library implementation.

## Public interface rules

- The public API is exposed through `include/native.h`.
- `include/native.h` must remain pure C++.
- Do not expose native OS handles, toolkit types, or platform headers in the public API.
- User-facing classes must not contain platform-specific implementation details.
- Prefer clear, modern, lowercase API naming consistent with the current codebase.

## Native state and bindings

- Native resources are associated with C++ objects through external bindings.
- Use `native::bindings` to map native handles to library objects and caches.
- Keep those bindings in toolkit or platform `globals.*` files inside the relevant namespace.
- Example pattern:
  `native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings`

## Build system rules

- The build system entry point is `CMakeLists.txt`.
- Do not add a new top-level `Makefile` for normal project orchestration.
- All builds must run through Docker-backed CMake targets.
- The purpose of Docker is to make backend builds reproducible and independent of host package drift.

### Docker targets

The top-level CMake project provides:

- `docker-x11`
- `docker-gemix`
- `docker-sdl2`
- `docker-openmotif`
- `docker-win`
- `docker-haiku`

These targets use the following Docker images:

- X11: `wischner/gcc-x86_64-linux-x11`
- GEMix: `wischner/gcc-x86_64-gemix`
- SDL2: `wischner/gcc-x86_64-linux-sdl`
- OpenMotif: `wischner/gcc-x86_64-linux-motif`
- Windows MinGW-w64: `wischner/gcc-x86_64-windows-mingw-w64`
- Haiku cross toolchain: `wischner/gcc-x86_64-haiku`

The expected workflow is:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
cmake --build build/cmake --target docker-win
cmake --build build/cmake --target docker-haiku
```

### Build directories

- `build/cmake/` is the host-side CMake control tree.
- `build/linux-x11/` is the Docker-produced X11 build tree.
- `build/linux-gemix/` is the Docker-produced GEMix build tree.
- `build/linux-sdl2/` is the Docker-produced SDL2 build tree.
- `build/linux-openmotif/` is the Docker-produced OpenMotif build tree.
- `build/windows-mingw-w64/` is the Docker-produced Windows MinGW-w64 build tree.
- `build/haiku/` is the Docker-produced Haiku build tree.

Do not collapse multiple platform or toolkit builds into the same CMake build directory.

## Linux backend rules

### X11 backend

- Files under `lib/native/toolkits/x11/` are for plain X11 code.
- Do not include Motif headers in the X11 backend.
- If a file only needs Xlib/Xutil/XRandR types, include only the matching X11 headers.
- The Docker X11 image must be able to compile the X11 backend without OpenMotif installed.

### SDL2 backend

- Files under `lib/native/toolkits/sdl2/` are for SDL2 code only.
- `SDL2_ttf` is optional.
- If `SDL2_ttf` is not found, text drawing must degrade gracefully instead of breaking the build.
- SDL2 paint flow must render at frame boundaries, not present once per primitive.
- Avoid repaint backlogs and laggy input paths; invalidation should be efficient.
- Default SDL2 windows should open in a reachable on-screen position.

### OpenMotif backend

- Files under `lib/native/toolkits/openmotif/` are for Motif/Xt integration only.
- Keep Motif and Xt types out of `include/native.h`.
- Use widget or resource colors when available, so Motif environments such as CDE can provide the default look.
- If resource lookup fails, fall back to explicit defaults:
  - paper = white
  - ink = black
- Keep the plain X11 backend independent from Motif headers and libraries.

## Windows backend rules

- Files under `lib/native/platforms/windows/` are for Win32 integration only.
- Keep Win32 handle types and message-loop details out of `include/native.h`.
- Mouse press/release/move/wheel events must map to the same `native` semantics used by Linux backends.
- `painter` behavior must match Linux stable backends:
  - press starts stroke
  - move extends stroke
  - release ends stroke
  - wheel clears strokes

## Haiku backend rules

- Files under `lib/native/platforms/haiku/` are for Haiku API integration only.
- Keep `BApplication`, `BWindow`, and `BView` usage behind the platform layer.
- Mouse press/release/move/wheel and paint invalidation flow must match Linux stable backends.
- `painter` behavior must match Linux stable backends:
  - press starts stroke
  - move extends stroke
  - release ends stroke
  - wheel clears strokes

## Documentation rules

- Keep docs aligned with the actual build flow.
- `docs/manuals/BOOK-OF-NATIVE.md` and `docs/manuals/book-of-native/` describe
  how the current code works; they are not a roadmap.
- Do not add speculative features, future plans, or "coming soon" text to the book.
- If a feature is not implemented yet, leave it out until the code exists.
- Prefer backend-agnostic explanations in the book when the same pattern is shared.
- Use `docs/notes/` only for exceptional cases such as remote builds or unusual host requirements.
- Do not keep normal Docker build instructions in `docs/notes/`.
- Do not maintain generated API docs yet; `docs/api` should stay absent until that work is intentionally started.
- If Linux build paths or targets change, update:
  - `README.md`
  - `docs/manuals/book-of-native/getting-started.md`
  - `docs/manuals/book-of-native/build-system.md` when relevant
  - `.vscode/launch.json` and `.vscode/tasks.json` when debug paths or build commands change

## Vision program

- `vision` is the single program built by the repository.
- Its source lives in `src/` and it links against `native` through the public API.
- VS Code launch configurations should point to `vision` for each active backend.
- Educational programs belong in `docs/manuals/PROGRAMMING-NATIVE.md` and its
  chapter files, not in a separate examples source tree.

## Quality bar

- Prefer small, explicit, maintainable code over clever code.
- Match the style and architecture already present in the project.
- Fix root causes, not just symptoms.
- Keep toolkit and platform code isolated to their own directories.
- Do not let backend-specific hacks leak into the public API.
- When adding fallback behavior, make it explicit and predictable.

## High-quality contributor prompts

Use prompts like these when asking an agent or contributor to work on the codebase.
They are intentionally specific and should be preferred over vague requests.

### Build and infrastructure

- "Update the top-level CMake workflow so Linux X11 and SDL2 builds run through the Docker-backed targets, and keep VS Code debug tasks aligned with the same paths."
- "Refactor the Linux build output layout so toolkit-specific builds live under `build/linux-*` without breaking the Docker CMake targets or debugger launch paths."

### General feature work

- "Implement the feature inside the correct layer: core logic in `lib/native/`, OS integration in `lib/native/platforms/`, toolkit behavior in `lib/native/toolkits/`, and keep the public headers in `include/` pure C++."
- "When changing build or backend behavior, update the code, docs, and VS Code launch/build integration together."
