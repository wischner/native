# Patterns: Signal And Event Dispatching

This chapter describes the event delivery model used by the library.

## `signal<>`

The public header defines a small `signal<>` template used throughout the UI
types.

Signals are used for events such as:

- window creation
- move
- resize
- paint
- mouse motion
- mouse button actions
- mouse wheel

This gives all tested backends the same user-facing event contract.

## Connection model

Slots can be connected as:

- free functions or lambdas
- member functions on objects
- const member functions on objects

Connections receive an integer ID that can be used to disconnect later.

## Dispatch behavior

Signals are stored internally in a `std::map`.
They are dispatched in reverse order of registration.

If a slot returns `true`, dispatch stops.
That gives event handlers a built-in short-circuit mechanism.

## Concept sample: handler precedence

Because dispatch is reverse-registration order, the most recently connected
handler runs first. This is useful for layered behavior:

1. local handler decides whether to consume
2. fallback handler runs only if not consumed

## Lazy initialization

Signals can be created with an initializer callback.
That callback is run on first use.

This is part of the current implementation and allows backend work to be delayed
until a signal is actually connected or emitted.

## Window events

The `wnd` class exposes the current event surface through signals.

That means backend code can translate native messages into shared event payloads
without requiring user code to know anything about the native event system.

## Event translation

Backends are responsible for:

1. receiving native events
2. finding the owning `wnd`
3. converting native data into shared `native` types
4. emitting the corresponding `signal<>`

This keeps backend-specific event details out of the shared API and gives the
library a consistent event model. It is small, explicit, and effective.
