# Feature Matrix

This chapter records what is implemented now and what is currently exercised in
runtime checks.

Legend:

- `Yes (tested)` = implemented and exercised in current workflow
- `Yes (untested)` = implemented, not yet exercised in current workflow
- `WIP` = still under development

## Backend status

| Backend | Build path | Runtime status |
| --- | --- | --- |
| Linux X11 | `docker-x11` | Yes (tested) |
| Linux SDL2 | `docker-sdl2` | Yes (tested) |
| MS Windows (MinGW) | `docker-win` + Wine | Yes (tested) |
| Haiku | `docker-haiku` | Yes (untested) |
| Apple | platform code present | Yes (untested) |
| Other toolkit ports | varies | WIP |

## Core feature matrix

| Feature | Linux X11 | Linux SDL2 | MS Windows | Haiku | Apple | Other WIP ports |
| --- | --- | --- | --- | --- | --- | --- |
| Build through current Docker workflow | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | No | WIP |
| `app::run` startup path | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Screen detection | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Main window create/show | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Paint event (`on_wnd_paint`) | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Mouse move | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Mouse button press/release | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| Mouse wheel | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| `gpx_wnd` line/rect/image drawing | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| `gpx_img` software drawing | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | Yes (untested) | WIP |
| `painter-example` build | Yes (tested) | Yes (tested) | Yes (tested) | Yes (untested) | No | WIP |
| `painter-example` runtime | Yes (tested) | Yes (tested) | Yes (tested) | No (not run) | No (not run) | No (not run) |

## Notes

- SDL2 text drawing depends on `SDL2_ttf`.
  If `SDL2_ttf` is not present, text rendering is intentionally a no-op.
- Windows runtime checks in this workflow use Wine and require MinGW runtime
  DLLs for MinGW-built executables.
- Haiku and Apple entries are marked untested because this workflow has not yet
  exercised runtime behavior there.

## Why this matrix exists

This matrix is the contract for documentation quality:

- if code is implemented but not tested, we mark it untested
- if code is still moving, we mark it WIP
- when runtime checks expand, this table should be updated immediately
