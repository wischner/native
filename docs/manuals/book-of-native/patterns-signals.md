# Patterns: Signal And Event Dispatching

This chapter expands Section 3 of the architectural standards. `signal<>` is
the public notification mechanism for Native events. It gives every backend
the same synchronous, typed event contract.

## Public event contract

Signal arguments must be public Native types or other portable C++ types.
Native handles and toolkit event structures must be translated before an event
is emitted.

Use `signal<>` when an event has no payload and use a public value or event
structure when it has related data:

```cpp
signal<> on_wnd_create;
signal<point> on_wnd_move;
signal<wnd_paint_event> on_wnd_paint;
signal<mouse_event> on_mouse_click;
```

Grouping related fields in an event structure keeps a signal readable and
allows all backends to produce the same payload.

## Handlers and propagation

Every handler returns `bool`:

- `true` means the event was consumed and delivery stops.
- `false` means delivery continues to the next handler.

Handlers run synchronously in newest-first order. A newly connected local
handler therefore gets the first opportunity to consume an event before older
fallback handlers.

```cpp
bool handle_move(native::point position) {
    last_position = position;
    return false;
}

int connection = window.on_wnd_move.connect(
    this,
    &main_window::handle_move);
```

`emit()` does not queue work or move it to another thread. When it returns,
all non-skipped handlers have finished. UI signals must normally be connected
and emitted on the UI thread. If code deliberately uses a signal across
threads, that code is responsible for synchronization.

## Connecting and disconnecting

A signal accepts free functions, lambdas, non-const member functions, and
const member functions with the matching argument list. `connect()` returns
an integer identifier local to that signal.

```cpp
int connection = window.on_wnd_move.connect(
    this,
    &main_window::handle_move);

window.on_wnd_move.disconnect(connection);
window.on_wnd_move.disconnect_all();
```

Connection identifiers are not global. An identifier returned by one signal
must not be passed to another signal.

A member-function connection stores a non-owning receiver pointer. It does not
extend the receiver's lifetime. The receiver must outlive the connection, or
the connection must be removed before the receiver is destroyed.

## Do not mutate connections during emission

Handlers must not call `connect()`, `disconnect()`, or `disconnect_all()` on
the signal currently being emitted. Emission is iterating that signal's
connection collection, so mutation can invalidate the active traversal.

If a handler needs to change subscriptions, record that intention and apply it
after `emit()` has returned, or defer it through the backend event mechanism.

## Lazy initialization

A signal can receive an optional initializer:

```cpp
signal<point> on_native_motion([this]() {
    enable_native_motion_events();
});
```

The initializer runs at most once, before the first `connect()` or `emit()`.
This is useful when an event source requires native registration that should
be delayed until the signal is used. The initializer is setup logic, not an
event handler; it does not participate in propagation.

## Backend event translation

A backend handles an event in four stages:

1. Receive the native event or message.
2. Use a private binding to find the corresponding Native object.
3. Convert coordinates, buttons, wheel axes, and other values to public Native
   types.
4. Call the public signal's `emit()` synchronously.

For example, an X11 motion event, an SDL mouse event, and a Windows mouse
message all become the same `signal<point>` notification. Application handlers
never need to know which native event produced it.

## Choosing whether to consume an event

Return `true` when a handler has fully dealt with an event and older handlers
must not see it. Return `false` for observations, state caches, and behavior
that deliberately permits fallback processing.

This convention is especially useful for layered controls: a specialized
handler can consume an input event, while a general window handler remains
available when it does not.
