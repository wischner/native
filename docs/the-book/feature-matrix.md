# Feature Matrix

This chapter records only the feature surface that is implemented and exercised
in the current stable backends. It is intentionally conservative.

## Current stable backends

The project currently treats these Linux toolkit backends as stable:

- the Linux backend configured with the native window-system toolkit
- the Linux backend configured with the SDL-based toolkit

This chapter does not describe unfinished ports or planned features.

## Application lifecycle

Implemented in the stable backends:

- screen detection before application startup
- main application window creation
- main loop entry through `app::run`
- example programs linked against the shared `native` library

## Window events

Implemented in the stable backends:

- window creation notification
- window move notification
- window resize notification
- paint notification

## Mouse events

Implemented in the stable backends:

- mouse move
- mouse button press
- mouse button release
- mouse wheel

These events are exposed through the `signal<>` mechanism on `wnd`.

## Drawing surface

Implemented in the stable backends:

- window-backed graphics via `gpx_wnd`
- image-backed graphics via `gpx_img`
- clipping
- line drawing
- rectangle drawing
- image blitting

## Text drawing

Text drawing exists in the graphics interface.

For the SDL-based backend, text rendering is optional and depends on whether the
build finds `SDL2_ttf`. If that dependency is not present, text drawing becomes
a no-op rather than a build failure.

## Examples

The current examples exercise the stable feature set:

- minimal main window
- interactive painter using mouse input and retained strokes

## What this chapter does not do

This chapter is not a roadmap.
If a feature is not described here, it should be added only when the code and
the examples support it.
