# Patterns: Cached Properties

This chapter expands Section 4 of the architectural standards. Public
properties are portable state owned by Native objects. They are not thin
wrappers around native getter and setter functions.

## Naming

Mutable properties use a consistent pair of operations:

```cpp
const std::string &get_title() const;
app_wnd &set_title(const std::string &title);
```

The rules are:

- A reader is named `get_<property>()` and is `const`.
- A writer is named `set_<property>(value)`.
- A setter returns a reference when chaining is useful.
- Boolean properties still use the same getter convention, such as
  `get_created()`.

The return type of a derived-class setter should preserve useful chaining. For
example, `app_wnd::set_title()` returns `app_wnd &`, while geometry setters
shared by all windows return `wnd &`.

## The portable cache is authoritative

The public object stores the property's current value using only portable
types. A getter reads this cache and does not ask the operating system or
toolkit for the value.

```cpp
const std::string &app_wnd::get_title() const {
    return _title;
}
```

This gives getters the same cost and semantics on every backend. It also makes
properties available before native resources exist.

## Setter sequence

A setter follows the same sequence everywhere:

1. Validate the requested public value.
2. Store it in the portable cache.
3. If the native resource exists, apply the cached value immediately.
4. Return the object when chaining is supported.

```cpp
app_wnd &app_wnd::set_title(const std::string &title) {
    _title = title;
    if (_created)
        apply_title();
    return *this;
}
```

The cache is updated even when `_created` is false. Creation later applies the
cached value, so the following code must produce the same native window on
every backend:

```cpp
main_window window;
window.set_title("Vision")
      .set_bounds(native::rect(100, 80, 800, 600));

native::app::run(window);
```

## Backend application hooks

Shared setters should not contain native calls. They call a small backend
operation such as `apply_title()`, `apply_position()`, or
`apply_dimensions()` only when a resource exists.

The backend operation reads the already-validated cache and updates the native
resource. Native types remain in the backend implementation, while the public
setter retains identical behavior across platforms.

Creation must apply all cached properties before it reports the object as
ready. Otherwise a value set before `create()` would behave differently from
the same value set afterward.

## Native-originated changes

Some properties can change without a public setter. A user can drag or resize
a window, and a toolkit can report destruction. Backends translate those
events and call a separate shared notification:

```cpp
window->on_native_move(native::point(x, y));
window->on_native_resize(native::size(width, height));
```

These notifications update the cache and any dependent portable state, such
as layout. They do not call the public setter, because doing so would send the
same change back to the backend and could create a resize or move loop.

The direction of updates is therefore explicit:

```text
application setter -> portable cache -> native apply operation
native event       -> portable cache -> layout/event processing
```

## Validation and failure

Validation belongs before the cache changes. If a value is invalid, the setter
should throw an appropriate standard exception and leave both the cache and
native resource unchanged.

Backend failure needs an equally clear policy. A setter must not silently make
the portable cache claim one value while a created native resource retains
another. The implementation should either complete the update or report the
failure according to the public operation's contract.

## Adding a property

When adding a mutable property:

1. Add its portable storage to the public class.
2. Add a documented `const` getter and `set_` setter.
3. Validate and cache the value in shared code.
4. Add a private backend application hook if native state must change.
5. Apply the cache during creation in every backend.
6. Add a shared native-notification path if the toolkit can change it.
7. Test behavior before creation, after creation, and after a native-originated
   change.

Following this pattern prevents hidden native queries and makes an object's
public state predictable throughout its lifecycle.
