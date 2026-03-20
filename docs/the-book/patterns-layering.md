# Patterns: Source Layering And Native Bindings

This chapter describes the main structural rule of the project: public C++ code
stays clean, while native implementation details stay in backend code.

## Layering

The code is split into three implementation layers:

- `src/`
  - shared C++ logic

- `src/platforms/`
  - operating-system-specific code

- `src/toolkits/`
  - toolkit-specific code used on platforms that need a separate windowing or
    rendering backend

The public header in `include/native.h` sits above those layers and should not
expose native handles or native headers.

## Public API boundary

The user-facing types are written as portable C++ abstractions:

- geometry types
- windows
- graphics objects
- events
- signals
- screen metadata

Backend code is responsible for mapping those abstractions to native resources.

## Native bindings

The code uses `native::bindings<A, B>` as a bidirectional mapping between native
objects and library objects.

Typical mappings include:

- native window handle to `native::wnd*`
- `native::wnd*` to backend graphics cache

This lets the backend:

- find the `wnd` that owns a native event
- find the native handle for a given `wnd`
- keep renderer or graphics state outside the public window class

## Backend namespaces

Each backend keeps its helper types and binding instances inside its own
namespace.

That is where backend globals belong:

- native handle bindings
- cached display or renderer state
- backend-specific graphics caches

This keeps backend state grouped with the code that owns it instead of leaking
into shared code.

## Why the pattern matters

This pattern keeps responsibilities clear:

- shared code defines the model
- backend code performs native work
- native resources stay outside the public API

That separation is one of the main reasons the project stays readable even while
supporting multiple backends.
