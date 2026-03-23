# Lab Source TODOs

This note inventories TODOs in the `lab/` tree.

The `lab/` tree is legacy code.
It is not part of the current verified user workflow.

We are keeping it for now as a reference pool in case some parts are useful for
reuse during active development.
It is expected to be deleted eventually rather than brought up to production
quality as a whole.

These TODOs are tracked so we know what unfinished material exists in that
legacy tree.

## Geometry and shared headers

| File | Issue |
| --- | --- |
| `lab/src/geometry.hpp` | TODO: include `types.h`. |
| `lab/src/raster.hpp` | TODO marker exists without a description. |

## X11 lab backend

| File | Issue |
| --- | --- |
| `lab/src/native/x11/native_app_wnd.cpp` | TODO: implement lazy subscription. |
| `lab/src/native/x11/native_artist.cpp` | TODO marker exists without a description. |
| `lab/src/native/x11/native_wnd.cpp` | TODO: pass size. |
| `lab/src/native/x11/native_wnd.cpp` | TODO: client area handling is uncertain in two places. |
| `lab/src/native/x11/native_wnd.cpp` | TODO: one method is still not implemented. |
| `lab/src/native/x11/native_wnd.cpp` | TODO: key press and key release handling. |

## SDL lab backend

| File | Issue |
| --- | --- |
| `lab/src/native/sdl/native_artist.cpp` | TODO marker exists without a description. |
| `lab/src/native/sdl/native_start.cpp` | TODO: initialize application. |
| `lab/src/native/sdl/native_wnd.cpp` | TODO: one placeholder comment says "Whatever." |
| `lab/src/native/sdl/native_wnd.cpp` | TODO: check `SDL_GetRendererOutputSize`. |
| `lab/src/native/sdl/native_wnd.cpp` | TODO: current code assumes SDL only has one window. |
| `lab/src/native/sdl/native_wnd.cpp` | TODO: shift and ctrl status in event translation. |

## Windows lab backend

| File | Issue |
| --- | --- |
| `lab/src/native/win/native_wnd.cpp` | TODO: investigate `WM_GETMINMAXINFO` ordering on Windows 10. |

## `lab/include/nice.hpp` inventory

This file carries the same family of open items in its inlined backend code:

| File | Issue |
| --- | --- |
| `lab/include/nice.hpp` | TODO: investigate `WM_GETMINMAXINFO` ordering on Windows 10. |
| `lab/include/nice.hpp` | TODO: implement lazy subscription. |
| `lab/include/nice.hpp` | TODO: pass size. |
| `lab/include/nice.hpp` | TODO: client area handling is uncertain in two places. |
| `lab/include/nice.hpp` | TODO: one method is still not implemented. |
| `lab/include/nice.hpp` | TODO: key press and key release handling. |
| `lab/include/nice.hpp` | TODO: one placeholder comment says "Whatever." |
| `lab/include/nice.hpp` | TODO: check `SDL_GetRendererOutputSize`. |
| `lab/include/nice.hpp` | TODO: current code assumes SDL only has one window. |
| `lab/include/nice.hpp` | TODO: shift and ctrl status in event translation. |
| `lab/include/nice.hpp` | TODO: initialize application. |

## Notes

- Several `lab/` TODOs are mirrored between `lab/include/nice.hpp` and
  backend-specific `lab/src/native/*` files.
- Some TODO markers are low-information placeholders.
  Those should be rewritten into concrete issues when the lab code is worked on.
- This note does not mean `lab/` is part of the supported product surface.
  It is legacy material kept only for temporary reference and possible reuse.
