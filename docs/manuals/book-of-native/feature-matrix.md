# Feature Matrix

This chapter records what is implemented now and what is currently exercised in
runtime checks. For the contracts and examples behind the drawing rows, see
[Drawing Primitives](drawing-primitives.md).

Legend:

- `Yes (tested)` = implemented and exercised in current workflow
- `Yes (build tested)` = implemented and compiled in the current workflow,
  without a runtime assertion for that behavior
- `Yes (untested)` = implemented, not yet exercised in current workflow
- `No (not run)` = not exercised by the current runtime workflow
- `WIP` = still under development

## Backend status

| Backend | Build path | Runtime status |
| --- | --- | --- |
| Linux X11 | `docker-x11` | Yes (tested) |
| Linux SDL2 | `docker-sdl2` | Yes (tested) |
| Linux OpenMotif | `docker-openmotif` | Yes (untested) |
| MS Windows (MinGW) | `docker-win` + Wine | Yes (tested) |
| Haiku | `docker-haiku` + SSH deploy/run | Yes (tested) |
| Apple | platform code present | Yes (untested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Build through current Docker workflow | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No | WIP |
| `app::run` startup path | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Screen detection | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Main window create/show | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Paint event (`on_wnd_paint`) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse move | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse button press/release | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Mouse wheel | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `vision` build | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | No | WIP |
| `vision` runtime | Yes (tested) | Yes (tested) | No (not run) | Yes (tested) | Yes (tested) | No (not run) | No (not run) |

## Drawing, text, and image matrix

| Feature | Linux X11 | Linux SDL2 | Linux OpenMotif | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `gpx` ink/paper/pen/font state and clipping | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_wnd`: complete-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: clear, line, outlined/filled rectangle | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: UTF-8 text drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `gpx_img`: image-to-image drawing | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| `img`: owned RGBA storage and direct pixel access | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | Yes (tested) | Yes (untested) | WIP |
| Font, text-run, and Unicode-character measurement | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | Yes (build tested) | WIP |
| PNG file load/save and memory decode/encode | libpng (tested) | libpng (tested) | libpng (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| JPEG file load/save and memory decode/encode | libjpeg (tested) | libjpeg (tested) | libjpeg (tested) | GDI+ (tested) | Translation Kit (build tested) | ImageIO (tested) | WIP |
| `theme` factory, metrics, palette, and state preservation | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed button drawing | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed menu bar/title/item/popup drawing | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |
| Themed list-item drawing | Yes (tested) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (tested) | WIP |

## Notes

- Linux X11 uses Xt for its event loop and Athena `Command`, `MenuButton`,
  `SimpleMenu`, and `SmeBSB` widgets for buttons and application menus.
- SDL2 text drawing depends on `SDL2_ttf`.
  If `SDL2_ttf` is not present, text rendering is intentionally a no-op.
- Windows runtime checks in this workflow use Wine and require MinGW runtime
  DLLs for MinGW-built executables.
- OpenMotif uses widget resource colors when they are available through Motif/Xt.
  Its theme also uses Motif `Xme` shadow primitives for native-window targets.
  If resource lookup or native drawing is unavailable, the backend supplies a
  Motif-specific emulation.
- Windows themes use native GDI control primitives and system colors. Haiku
  themes use `BControlLook`, and macOS themes use AppKit cells and colors.
- X11/Athena, SDL2, and GEMix keep their native-look emulation in their own
  toolkit backends because they do not expose a suitable painter for every
  graphics target.
- Haiku runtime checks in this workflow use Docker for the build, then copy the
  binaries to a Haiku machine and launch them there over SSH.
- A tested drawing row means the implementation is covered by the current
  build/runtime workflow. It does not imply pixel-perfect visual comparison of
  every state on every desktop theme.

## Why this matrix exists

This matrix is the contract for documentation quality:

- if code is implemented but not tested, we mark it untested
- if code is still moving, we mark it WIP
- when runtime checks expand, this table should be updated immediately
