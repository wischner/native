# Patterns: Windows And App Windows

This chapter expands Section 5 of the architectural standards. `wnd` defines
the portable behavior shared by top-level windows and child controls. Backend
code supplies native resources without changing that behavior.

## Responsibilities of `wnd`

The base class owns or records only portable state:

- Cached bounds.
- A non-owning parent and non-owning child list.
- An owned layout manager.
- Access to a lazily created graphics abstraction.
- Lifecycle, geometry, paint, and input signals.
- Invalidation and lifecycle entry points.

Derived classes add properties and signals specific to one kind of window.
`app_wnd`, for example, adds a title, menu, and menu-command signal. `button`
adds its text and activation signal.

Native window handles, widgets, views, renderers, device contexts, and toolkit
callbacks never belong in these public classes. Backends keep them in private
bindings and graphics caches.

## Window lifecycle

A window moves through a small, consistent lifecycle:

```text
constructed -> created -> shown -> destroyed
                    ^                 |
                    +-----------------+
```

Construction records portable state. It must not create a native resource.
This lets application code configure properties, parents, layouts, and signals
before a backend is involved.

`create()` must be idempotent. On its first successful call it:

1. Verifies lifecycle prerequisites.
2. Creates the native resource.
3. Registers native bindings.
4. Applies every cached property.
5. Marks the object as created.
6. Emits `on_wnd_create` exactly once for that creation.

A repeated call while the resource exists does nothing. If a destroyed object
supports creation again, the next successful creation is a new lifecycle and
may emit one new create event.

`show()` requires a created resource. It exposes that existing resource; it
does not substitute for creation.

`destroy()` is also idempotent. It destroys child resources as required,
releases graphics resources, removes bindings, destroys the native resource,
and clears the created state. Destruction initiated by a toolkit must converge
on the same shared state through `on_native_destroy()`.

## Parent and child relationships

Parents and children refer to one another but do not own one another. The
application controls their C++ lifetimes. Destroying a C++ parent detaches
surviving child objects, and destroying a child removes it from the parent's
portable child list.

A child control needs an assigned, already-created parent before its own
native resource can be created. This ordering matters because most toolkits
require the native parent handle during child creation.

The main window's create signal is the standard place to create controls:

```cpp
class main_window final : public native::app_wnd
{
public:
    main_window()
        : native::app_wnd("Vision"),
          accept("Accept") {
        on_wnd_create.connect(this, &main_window::handle_create);
    }

private:
    native::button accept;

    bool handle_create() {
        accept.set_parent(this);
        accept.create();
        accept.show();
        return false;
    }
};
```

Keeping the button as a member makes its C++ lifetime cover the connection and
the main window's active event loop.

Parent assignment must reject hierarchy cycles. Reparenting a created object
must also preserve the backend's lifecycle requirements; an uncreated parent
cannot receive a created child.

## Layout ownership and geometry

A window owns its installed layout manager through a unique pointer. The
layout manager observes the window's non-owning child list and assigns child
bounds.

Portable geometry is always cached. Calling `set_position()`,
`set_dimensions()`, or `set_bounds()` updates that cache and applies it to a
created native resource. Dimension changes relayout children.

When the user or toolkit resizes a window, the backend calls
`on_native_resize()`. That updates cached dimensions and runs layout without
sending the same resize request back to the toolkit. Native move notifications
follow the same no-echo rule.

## Invalidation and painting boundary

`invalidate()` asks the backend to schedule a repaint of all or part of the
client area. It does not paint immediately and does not emit `on_wnd_paint`
itself. The backend later receives a native paint event and performs the paint
flow described in [Window Painting](patterns-painting.md).

This separation allows native event coalescing and ensures drawing happens
with the correct native context and clip.

## Adding a window type

A new public `wnd` subclass is not complete when only its shared declaration
exists. Every supported backend must implement the same lifecycle and public
behavior.

The implementation checklist is:

1. Add portable properties, cached state, and public signals.
2. Implement shared validation and cache behavior.
3. Implement creation, cached-property application, showing, and destruction
   in every backend.
4. Add and remove native bindings at the correct lifecycle points.
5. Translate native events to public Native event types.
6. Support invalidation, painting, and graphics cleanup where applicable.
7. Add build coverage for every backend and runtime coverage where available.

Platform differences are expected below the public API. They must not produce
different public lifecycle or ownership rules.
