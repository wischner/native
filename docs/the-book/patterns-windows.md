# Patterns: Windows And App Windows

This chapter explains the current window model.

## `wnd`

`wnd` is the shared base class for windows.

It stores common state such as:

- bounds
- parent pointer
- optional layout manager
- lazily created graphics object
- event signals

The base class does not own native window handles directly.

## Current responsibilities of `wnd`

Today `wnd` provides:

- position and size access
- bounds updates
- parent assignment
- invalidation entry points
- access to a `gpx` drawing object
- common signals for paint and input

The shared class defines the interface.
Backends provide the native behavior for creation, showing, destruction,
invalidation, and painting.

## `app_wnd`

`app_wnd` is the main application window type.

It extends `wnd` with:

- a title
- backend window creation
- backend show logic
- backend destroy logic

An `app_wnd` is passed into `app::run()` and becomes the main window for the
application.

## Painting model

Painting is event-driven.

Backends are responsible for:

- noticing that a window needs repaint
- preparing the backend drawing surface
- setting the clip region
- clearing the window background
- emitting `on_wnd_paint`

The painter example exercises that model by storing strokes in user code and
redrawing them during paint events.

## Graphics object lifetime

The drawing object returned by `wnd::get_gpx()` is created lazily.

That means the backend graphics object is not created until it is first needed.
Its native resources live in backend-specific caches rather than in the public
window object.

## Why this structure is used

This window model keeps the shared API small while still allowing each backend
to manage:

- native window handles
- renderer or graphics state
- invalidation behavior
- event translation

The result is a public window abstraction that stays portable even though the
native work differs underneath.
