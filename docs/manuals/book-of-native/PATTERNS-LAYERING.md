# Patterns: Source Layering And Native Bindings

This chapter expands Sections 1 and 2 of the architectural standards. The main
rule is simple: applications see one modern C++20 interface, while all native
types and native calls remain below that interface.

## Source layers

Native separates declarations, shared behavior, operating-system behavior,
and toolkit behavior:

```text
include/native.h                 complete public include
include/native/*.h               public declarations and portable types
lib/native/*.cpp                 shared C++ implementation
lib/native/platforms/<name>/     operating-system implementation
lib/native/toolkits/<name>/      toolkit implementation
src/                             Vision application
```

Application code includes `<native.h>`. It must not need a Windows, AppKit,
BeAPI, X11, SDL, Motif, or GEM header to use the library. Public class
declarations live under `include/`; their non-template implementations live
under `lib/native/` or the appropriate backend directory.

The three implementation levels have distinct purposes:

1. Shared code in `lib/native/` owns portable state, validation, algorithms,
   and behavior that must be identical everywhere.
2. Platform code in `lib/native/platforms/<platform>/` handles facilities
   supplied directly by an operating system.
3. Toolkit code in `lib/native/toolkits/<toolkit>/` handles a selectable UI or
   graphics toolkit used on a platform.

Do not duplicate portable rules in every backend. For example, cached window
geometry and screen-snapshot normalization belong in shared code. Creating an
`HWND`, `NSWindow`, `BWindow`, `SDL_Window`, or X11 `Window` belongs in backend
code.

## Public API boundary

The public API may contain Native types and standard-library types. Examples
include `point`, `rect`, `rgba`, `std::string`, `signal<>`, `wnd`, and `gpx`.
It must not contain native handles, native callback structures, or native
headers, including in private members of public classes.

Keeping a native handle in a private member would still leak its type into the
public class definition. It could alter layout, require a native header, and
make the supposedly portable object dependent on one backend. The binding
layer avoids all three problems.

Conceptually, the boundary looks like this:

```cpp
// Public object: portable state only.
class wnd
{
public:
    virtual void create() const = 0;
    virtual void destroy() const = 0;
};
```

```cpp
// Windows backend: the native handle remains in a private registry.
HWND handle = CreateWindowExW(/* native arguments */);
windows::wnd_bindings.register_pair(handle, window);
```

Other backends implement the same public operation using their own resources
without changing the public class.

## Native bindings

`native::bindings<handle_type, object_type>` is the internal bidirectional
registry used to associate a backend resource with the Native object that owns
or represents it. A lookup works in both directions:

- A native event supplies a handle, and the backend finds its `native::wnd *`.
- Shared code supplies a `native::wnd *`, and the backend finds the handle it
  must update or destroy.
- A window can map to a second backend object such as a graphics cache.

The registry maintains a one-to-one relationship. Registering a reused handle
or object removes its previous association first, which prevents stale reverse
lookups.

```cpp
native::bindings<Window, native::wnd *> wnd_bindings;

wnd_bindings.register_pair(x_window, owner);
native::wnd *target = wnd_bindings.object_from_handle(x_window);
Window handle = wnd_bindings.handle_from_object(owner);
wnd_bindings.unregister_by_handle(x_window);
```

Registration belongs to successful native creation. Unregistration belongs to
native destruction, including destruction initiated by the toolkit. A backend
must not leave a mapping that refers to a destroyed handle or object.

## Backend globals and namespaces

Bindings, process-wide native resources, native helper structures, and helper
functions shared by several backend files belong in that backend's private
`globals.h` and `globals.cpp`.

A platform implemented directly by the operating system uses its platform
namespace:

```cpp
namespace windows
{
    native::bindings<HWND, native::wnd *> wnd_bindings;
}
```

A toolkit selected on a platform uses a nested namespace:

```cpp
namespace linux::x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
}
```

These headers are internal. They must never be included by an application or
re-exported through `<native.h>`.

## Adding a backend operation

When adding behavior, decide its layer in this order:

1. Define the portable contract and public types in `include/native/`.
2. Put common validation, caching, and algorithms in `lib/native/`.
3. Add only the unavoidable native calls to every supported backend.
4. Store native state in private backend bindings or helper structures.
5. Translate native inputs back to public Native types before crossing the
   boundary upward.

This division keeps backend implementations small and makes differences easy
to audit. More importantly, an application has the same observable behavior
regardless of the selected platform or toolkit.
