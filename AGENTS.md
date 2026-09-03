# AGENTS.md

The working project brief for contributors and coding agents, and the map of
every maintained Markdown document in this repository. It describes how the
library is structured, how builds must be handled, what quality bar is
expected, and which documents a code change obliges you to refresh.

## Read this first

**Every code change must leave the affected documents refreshed in the same
commit.** Documentation in this project describes the code as it exists now —
not a roadmap, not a wish list. A commit that changes behavior and leaves the
matching chapter, standard, note, or matrix stale is an incomplete commit.
See [Refresh obligations](#refresh-obligations) for the concrete mapping.

The two normative standards outrank everything else:

| Document | Why |
| --- | --- |
| [docs/standards/ARCHITECTURE.md](docs/standards/ARCHITECTURE.md) | The architectural requirements. 19 numbered sections; the book chapters expand them one by one. |
| [docs/standards/CPP-CODING-STYLE.md](docs/standards/CPP-CODING-STYLE.md) | Mandatory C++ style: directory structure, naming, header/implementation split, file headers, function documentation, build and test rules. |

---

# Part 1 — Project rules

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
Tests live in `tests/`, and developer automation in `scripts/`.

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
- CMake toolchain files live in `scripts/cmake/`.
- Developer automation lives in purpose-specific directories under
  `scripts/`, such as `scripts/linux/`, `scripts/windows/`, and
  `scripts/macos/remote/`.
- Do not add a new top-level `Makefile` for normal project orchestration.
- All builds must run through Docker-backed CMake targets.
- The purpose of Docker is to make backend builds reproducible and independent of host package drift.

### Docker targets

The top-level CMake project provides:

- `docker-x11`
- `docker-gemix`
- `docker-sdl2`
- `docker-openmotif`
- `docker-openlook`
- `docker-wmaker`
- `docker-win`
- `docker-haiku`

These targets use the following Docker images:

- X11: `wischner/gcc-x86_64-linux-x11`
- GEMix: `wischner/gcc-x86_64-gemix`
- SDL2: `wischner/gcc-x86_64-linux-sdl`
- OpenMotif: `wischner/gcc-x86_64-linux-motif`
- OPEN LOOK: `wischner/gcc-x86_64-linux-openlook`
- Window Maker: `wischner/gcc-x86_64-linux-window-maker`
- Windows MinGW-w64: `wischner/gcc-x86_64-windows-mingw-w64`
- Haiku cross toolchain: `wischner/gcc-x86_64-haiku`

The expected workflow is:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
cmake --build build/cmake --target docker-openlook
cmake --build build/cmake --target docker-wmaker
cmake --build build/cmake --target docker-win
cmake --build build/cmake --target docker-haiku
```

### Build directories

- `build/cmake/` is the host-side CMake control tree.
- `build/linux-x11/` is the Docker-produced X11 build tree.
- `build/linux-gemix/` is the Docker-produced GEMix build tree.
- `build/linux-sdl2/` is the Docker-produced SDL2 build tree.
- `build/linux-openmotif/` is the Docker-produced OpenMotif build tree.
- `build/linux-openlook/` is the Docker-produced OPEN LOOK build tree.
- `build/linux-wmaker/` is the Docker-produced Window Maker build tree.
- `build/windows-mingw-w64/` is the Docker-produced Windows MinGW-w64 build tree.
- `build/haiku/` is the Docker-produced Haiku build tree.

Do not collapse multiple platform or toolkit builds into the same CMake build directory.

## Linux backend rules

### X11 backend

- Files under `lib/native/toolkits/x11/` are for Xlib, Xt, and Athena code.
- Use Athena widgets for standard controls and menus instead of emulating
  their painting and input behavior with custom Xlib windows.
- Do not include Motif headers in the X11 backend.
- If a file only needs Xlib/Xutil/XRandR types, include only the matching X11
  headers.
- The Docker X11 image must provide Xt and Athena, and must compile the X11
  backend without OpenMotif installed.

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
- Keep the X11/Athena backend independent from Motif headers and libraries.

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

## Vision program

- `vision` is the single program built by the repository.
- Its source lives in `src/` and it links against `native` through the public API.
- VS Code launch configurations should point to `vision` for each active backend.
- Educational programs belong in
  [docs/manuals/PROGRAMMING-NATIVE.md](docs/manuals/PROGRAMMING-NATIVE.md) and
  its chapter files, not in a separate examples source tree.

## Quality bar

- Prefer small, explicit, maintainable code over clever code.
- Match the style and architecture already present in the project.
- Fix root causes, not just symptoms.
- Keep toolkit and platform code isolated to their own directories.
- Do not let backend-specific hacks leak into the public API.
- When adding fallback behavior, make it explicit and predictable.

---

# Part 2 — Documentation map

## Top level

| Document | Contents |
| --- | --- |
| [README.md](README.md) | Project pitch, feature summary, backend list, dependency list per backend, direct and Docker-backed build commands, build output layout, and links into the two manuals. The public front door. |
| [AGENTS.md](AGENTS.md) | This file: project rules, documentation map, and refresh obligations. |

## Standards — `docs/standards/`

Normative. Short, rule-shaped, and authoritative when a book chapter and a
standard appear to disagree.

| Document | Contents |
| --- | --- |
| [ARCHITECTURE.md](docs/standards/ARCHITECTURE.md) | Sections 1–19: code structure, native bindings, signals, setters/getters, windows, painting, custom drawing, application, screens, fonts, clipboard, text editing, advanced tables, source editing, classic trees, split views and tabs, input controls/standard dialogs/non-client chrome, structural containers, and paintable child surfaces. |
| [CPP-CODING-STYLE.md](docs/standards/CPP-CODING-STYLE.md) | Sections 1–9: project directory structure, naming conventions, header and implementation separation, file header block, function documentation, general coding rules, build rules, documentation rules, and tests. |

## The Book of Native — `docs/manuals/book-of-native/`

Explains *how the library is built and why*. It is the detailed companion to
the architectural standards: most chapters name the Architecture section they
expand. Index: [docs/manuals/BOOK-OF-NATIVE.md](docs/manuals/BOOK-OF-NATIVE.md)
— current scope, runtime-tested backend list, and the 19-chapter table of
contents.

| Chapter | Contents |
| --- | --- |
| [GETTING-STARTED.md](docs/manuals/book-of-native/GETTING-STARTED.md) | Build the project, run `vision`, and understand the output layout. |
| [BUILD-SYSTEM.md](docs/manuals/book-of-native/BUILD-SYSTEM.md) | How top-level CMake and the Docker-backed backend targets are organized. |
| [PATTERNS-LAYERING.md](docs/manuals/book-of-native/PATTERNS-LAYERING.md) | Public API boundaries, the three implementation layers, and native handle/object mappings. Expands Architecture 1–2. |
| [PATTERNS-GEOMETRY.md](docs/manuals/book-of-native/PATTERNS-GEOMETRY.md) | Shared value types used by geometry, windows, graphics, and events. |
| [PATTERNS-SIGNALS.md](docs/manuals/book-of-native/PATTERNS-SIGNALS.md) | Public event contracts, connection lifetimes, synchronous dispatch. Expands Architecture 3. |
| [PATTERNS-PROPERTIES.md](docs/manuals/book-of-native/PATTERNS-PROPERTIES.md) | Setter/getter naming, portable caches, native-originated updates. Expands Architecture 4. |
| [PATTERNS-WINDOWS.md](docs/manuals/book-of-native/PATTERNS-WINDOWS.md) | Window state, hierarchy, lifecycle, layout, backend obligations. Expands Architecture 5. |
| [PATTERNS-PAINTING.md](docs/manuals/book-of-native/PATTERNS-PAINTING.md) | Graphics contexts, paint-event lifetimes, clipping, invalidation. Expands Architecture 6. |
| [PATTERNS-THEME.md](docs/manuals/book-of-native/PATTERNS-THEME.md) | Native theme primitives, semantic control states, portable fallbacks. Expands Architecture 7. |
| [DRAWING-PRIMITIVES.md](docs/manuals/book-of-native/DRAWING-PRIMITIVES.md) | Application-facing reference for window and image graphics, PNG/JPEG I/O, text measurement, and themed control drawing. |
| [PATTERNS-APPLICATION.md](docs/manuals/book-of-native/PATTERNS-APPLICATION.md) | Portable startup through `program()`, `app::run()`, and backend launchers. Expands Architecture 8. |
| [PATTERNS-SCREENS.md](docs/manuals/book-of-native/PATTERNS-SCREENS.md) | Snapshot lifetime, work areas, primary selection, backend detection. Expands Architecture 9. |
| [PATTERNS-CLIPBOARD-TEXT-EDITING.md](docs/manuals/book-of-native/PATTERNS-CLIPBOARD-TEXT-EDITING.md) | Typed clipboard transactions, editor modes, live validation, selection, clipboard commands. Expands Architecture 11–12. |
| [PATTERNS-TABLE-VIEW.md](docs/manuals/book-of-native/PATTERNS-TABLE-VIEW.md) | Model ownership, stable row identity, virtualization, grouping, search, backend adaptation. Expands Architecture 13. |
| [PATTERNS-CODE-EDIT.md](docs/manuals/book-of-native/PATTERNS-CODE-EDIT.md) | Canonical source storage, overlays, lexers, completion, shared painting, backend input translation. Expands Architecture 14. |
| [PATTERNS-SPLIT-VIEWS-AND-TABS.md](docs/manuals/book-of-native/PATTERNS-SPLIT-VIEWS-AND-TABS.md) | Split/tab layout ownership, native content transitions, floating shells, interaction, themed painting, persistence. Expands Architecture 16. |
| [PATTERNS-INPUT-DIALOGS-WINDOW-CHROME.md](docs/manuals/book-of-native/PATTERNS-INPUT-DIALOGS-WINDOW-CHROME.md) | Combo and list boxes, directory and message dialogs, non-client rulers, status bars, native adaptation, extensibility. Expands Architecture 17. |
| [PATTERNS-PANELS-AND-CANVASES.md](docs/manuals/book-of-native/PATTERNS-PANELS-AND-CANVASES.md) | Container versus drawing-surface roles, explicit child lifecycle, chrome versus client geometry, 32-bit content bounds, scrollbar resolution, painting order. Expands Architecture 18–19. |
| [FEATURE-MATRIX.md](docs/manuals/book-of-native/FEATURE-MATRIX.md) | Per-backend feature and test status for what is implemented now. |

Gap to be aware of: Architecture Sections 10 (Fonts) and 15 (Classic trees)
have no book chapter expanding them. Fonts are covered from the application
side in [DRAWING-PRIMITIVES.md](docs/manuals/book-of-native/DRAWING-PRIMITIVES.md)
and trees in
[13-COLLECTION-AND-DISCLOSURE-CONTROLS.md](docs/manuals/programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md),
so the standard is the only normative source for their internals.

## Programming Native — `docs/manuals/programming-native/`

Teaches *how to write applications against the library*. Complete runnable
programs and focused snippets; this is where educational example code lives
instead of a separate examples source tree. Index:
[docs/manuals/PROGRAMMING-NATIVE.md](docs/manuals/PROGRAMMING-NATIVE.md) — how
a native program is organized, the 18-chapter table of contents, and build
instructions.

| Chapter | Contents |
| --- | --- |
| [01-FIRST-APPLICATION.md](docs/manuals/programming-native/01-FIRST-APPLICATION.md) | The smallest program: `app_wnd` passed to `app::run()`. |
| [02-PAINTING-AND-INPUT.md](docs/manuals/programming-native/02-PAINTING-AND-INPUT.md) | Deriving a window class, paint events, mouse input. |
| [03-MENUS-AND-COMMANDS.md](docs/manuals/programming-native/03-MENUS-AND-COMMANDS.md) | The `main_menu` model, construction order, commands. |
| [04-BUTTONS.md](docs/manuals/programming-native/04-BUTTONS.md) | Control objects and the native-resource creation lifecycle. |
| [05-CONFIGURING-CONTROLS.md](docs/manuals/programming-native/05-CONFIGURING-CONTROLS.md) | Geometry constructors and post-creation changes. |
| [06-ABSOLUTE-LAYOUT.md](docs/manuals/programming-native/06-ABSOLUTE-LAYOUT.md) | `absolute_layout_manager`. |
| [07-GRID-LAYOUT.md](docs/manuals/programming-native/07-GRID-LAYOUT.md) | `grid_layout_manager`, tracks, nesting. |
| [08-SELECTION-CONTROLS.md](docs/manuals/programming-native/08-SELECTION-CONTROLS.md) | `check`, `radio`, `list`. |
| [09-OWNED-WINDOWS-AND-DIALOGS.md](docs/manuals/programming-native/09-OWNED-WINDOWS-AND-DIALOGS.md) | Owned/modeless/modal windows and standard file dialogs. |
| [10-GRAPHICS-IMAGES-FONTS-THEMES.md](docs/manuals/programming-native/10-GRAPHICS-IMAGES-FONTS-THEMES.md) | The `gpx` drawing interface, images, fonts, themes. |
| [11-CLIPBOARD-AND-TEXT-EDITING.md](docs/manuals/programming-native/11-CLIPBOARD-AND-TEXT-EDITING.md) | UTF-8 encoding boundary, clipboard, text controls. |
| [12-BUILDING-AND-DISTRIBUTING.md](docs/manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md) | The nine build selections (six Linux toolkits plus Windows, Haiku, macOS), linking, distribution. |
| [13-COLLECTION-AND-DISCLOSURE-CONTROLS.md](docs/manuals/programming-native/13-COLLECTION-AND-DISCLOSURE-CONTROLS.md) | `accordion`, `icon_view`, `tree_view`. |
| [14-ADVANCED-TABLE-VIEWS.md](docs/manuals/programming-native/14-ADVANCED-TABLE-VIEWS.md) | `list` versus `table_view`, models, virtualization. |
| [15-SOURCE-EDITING.md](docs/manuals/programming-native/15-SOURCE-EDITING.md) | `code_edit`: line numbers, syntax styles, source UTF-8. |
| [16-SPLIT-VIEWS-AND-TABS.md](docs/manuals/programming-native/16-SPLIT-VIEWS-AND-TABS.md) | `split_view` and native tab controls. |
| [17-INPUT-DIALOGS-AND-WINDOW-CHROME.md](docs/manuals/programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md) | Choice controls, OS dialogs, non-client chrome. |
| [18-PANELS-AND-CANVASES.md](docs/manuals/programming-native/18-PANELS-AND-CANVASES.md) | `panel` grouping and child lifecycle, `canvas` painting, scrolling, scrollbar policy, and rulers. |

## Notes — `docs/notes/`

Exceptional material only: remote builds, unusual host requirements, open
issues, and TODO inventories. Standard local and Docker build instructions do
**not** belong here — they belong in the book. Index:
[docs/notes/README.md](docs/notes/README.md), which states what qualifies.

| Document | Contents |
| --- | --- |
| [CUSTOM.md](docs/notes/CUSTOM.md) | Native-versus-custom control audit. Per backend, per public control: native (**N**), hybrid (**H**), library-painted (**C**), or external helper (**E**), plus applied native replacements and deliberately retained custom implementations with reasons. Carries an audit date. |
| [BACKEND-OPEN-ISSUES.md](docs/notes/BACKEND-OPEN-ISSUES.md) | Backend-level issues that are real today, plus the runtime-tested versus in-progress backend status. |
| [HAIKU-REMOTE-RUNTIME.md](docs/notes/HAIKU-REMOTE-RUNTIME.md) | Haiku workflow: Docker cross-build, `scp` deploy, GDB over `ssh`, and what is verified. |
| [MACOS-REMOTE-RUNTIME.md](docs/notes/MACOS-REMOTE-RUNTIME.md) | Remote macOS workflow from Linux against host `leia`, the `MAC_REMOTE_*` environment variables, and the driving scripts. |
| [WINDOWS-WINE-RUNTIME.md](docs/notes/WINDOWS-WINE-RUNTIME.md) | Which MinGW runtime DLLs must sit next to the `.exe` for `docker-win` binaries to run under Wine. |
| [PRODUCTION-SOURCE-TODOS.md](docs/notes/PRODUCTION-SOURCE-TODOS.md) | Inventory of TODO markers in `include/`, `lib/native/`, and `src/`. Must be updated in the same commit that adds or removes a production TODO. |
| [LAB-SOURCE-TODOS.md](docs/notes/LAB-SOURCE-TODOS.md) | Inventory of TODOs in the legacy `lab/` tree. That tree is not currently present in the repository; treat this note as historical until it returns or is removed. |

## Other

| Document | Contents |
| --- | --- |
| [lib/native/third_party/README.md](lib/native/third_party/README.md) | Provenance and pinned upstream commit for the vendored `stb_truetype.h`, and its exemption from local formatting and file-length conventions. |

---

# Part 3 — Documentation obligations

## Refresh obligations

Documentation must be refreshed in the same commit as the code change that
invalidates it. Use this table to find what a change touches.

| If you change… | Refresh |
| --- | --- |
| Public API in `include/native.h` | The relevant [ARCHITECTURE.md](docs/standards/ARCHITECTURE.md) section, the book chapter that expands it, and the Programming Native chapter that teaches it. |
| A control's implementation on any backend | [CUSTOM.md](docs/notes/CUSTOM.md) (kind mark and audit date) and [FEATURE-MATRIX.md](docs/manuals/book-of-native/FEATURE-MATRIX.md). |
| Backend feature coverage or test status | [FEATURE-MATRIX.md](docs/manuals/book-of-native/FEATURE-MATRIX.md) and, if the runtime status changed, [README.md](README.md), [BOOK-OF-NATIVE.md](docs/manuals/BOOK-OF-NATIVE.md) "Current scope", and [BACKEND-OPEN-ISSUES.md](docs/notes/BACKEND-OPEN-ISSUES.md). |
| Build paths, CMake targets, or Docker images | [README.md](README.md), the build system rules in Part 1 of this file, [GETTING-STARTED.md](docs/manuals/book-of-native/GETTING-STARTED.md), [BUILD-SYSTEM.md](docs/manuals/book-of-native/BUILD-SYSTEM.md), [12-BUILDING-AND-DISTRIBUTING.md](docs/manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md), and `.vscode/launch.json` / `.vscode/tasks.json`. |
| Architectural layering or binding rules | [ARCHITECTURE.md](docs/standards/ARCHITECTURE.md), the architecture rules in Part 1 of this file, and [PATTERNS-LAYERING.md](docs/manuals/book-of-native/PATTERNS-LAYERING.md). |
| Add or remove a TODO in production source | [PRODUCTION-SOURCE-TODOS.md](docs/notes/PRODUCTION-SOURCE-TODOS.md). |
| A remote or unusual host workflow | The matching note in [docs/notes/](docs/notes/), and its entry in [docs/notes/README.md](docs/notes/README.md). |
| A vendored third-party file | [lib/native/third_party/README.md](lib/native/third_party/README.md). |

## Documentation conventions

- Keep docs aligned with the actual build flow.
- Markdown file names use uppercase words separated by hyphens with a
  lowercase `.md` suffix, for example `BACKEND-OPEN-ISSUES.md`. Conventional
  single-word names such as `README.md` remain valid.
- [BOOK-OF-NATIVE.md](docs/manuals/BOOK-OF-NATIVE.md) and
  `docs/manuals/book-of-native/` describe how the current code works; they are
  not a roadmap. No speculative features, no future plans, no "coming soon".
- If a feature is not implemented, leave it out until the code exists.
- Prefer backend-agnostic explanations in the book when the same pattern is
  shared.
- Use `docs/notes/` only for exceptional cases such as remote builds or unusual
  host requirements. Do not keep normal Docker build instructions there.
- Do not maintain generated API docs; `docs/api` stays absent until that work
  is intentionally started.
- Adding a new note also means adding its line to
  [docs/notes/README.md](docs/notes/README.md); adding a chapter also means
  adding it to its manual's table of contents — and adding either means adding
  a row to the documentation map above.

## High-quality contributor prompts

Use prompts like these when asking an agent or contributor to work on the
codebase. They are intentionally specific and should be preferred over vague
requests.

### Build and infrastructure

- "Update the top-level CMake workflow so Linux X11 and SDL2 builds run through the Docker-backed targets, and keep VS Code debug tasks aligned with the same paths."
- "Refactor the Linux build output layout so toolkit-specific builds live under `build/linux-*` without breaking the Docker CMake targets or debugger launch paths."

### General feature work

- "Implement the feature inside the correct layer: core logic in `lib/native/`, OS integration in `lib/native/platforms/`, toolkit behavior in `lib/native/toolkits/`, and keep the public headers in `include/` pure C++."
- "When changing build or backend behavior, update the code, docs, and VS Code launch/build integration together."
