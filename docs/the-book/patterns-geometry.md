# Patterns: Geometry And Type Conventions

This chapter describes the basic value types used throughout the codebase.

## Coordinate and dimension types

The public header defines:

```cpp
using coord = int16_t;
using dim = uint16_t;
```

These types are used across:

- geometry
- window bounds
- image dimensions
- input events

This keeps all backends on the same coordinate vocabulary.

## `point`

`point` represents a position:

```cpp
struct point
{
    coord x = 0;
    coord y = 0;
};
```

It is used for window positions, mouse coordinates, and drawing endpoints.

## `size`

`size` represents dimensions:

```cpp
struct size
{
    dim w = 0;
    dim h = 0;
};
```

It is used for window sizes, image sizes, and resize events.

## `rect`

`rect` combines a position and dimensions:

```cpp
struct rect
{
    point p;
    size d;
};
```

The implementation provides helper methods such as:

- `x1()`
- `y1()`
- `x2()`
- `y2()`
- `w()`
- `h()`
- `contains()`
- `intersect()`

These are used by painting code, clipping, screen work areas, and hit testing.

## Concept sample: half-open rectangles

`rect::contains()` uses half-open bounds:

```cpp
pt.x >= x1() && pt.x < x2()
pt.y >= y1() && pt.y < y2()
```

This avoids fencepost ambiguity and matches common graphics practice.

## `line`

`line` is a small value type built from two points.
It currently provides a `contains()` helper for simple geometric tests.

## `rgba`

Color values are represented by the `rgba` union in the public header.

It provides:

- channel access through `r`, `g`, `b`, `a`
- packed 32-bit storage through `value`

The graphics layer uses `rgba` consistently for ink, paper, image pixels, and
background clears.

## Why these types matter

These small value types give every layer of the library the same vocabulary.

The same `point`, `size`, `rect`, and `rgba` types appear in:

- event payloads
- screen metadata
- window bounds
- graphics APIs
- image manipulation

That consistency keeps the API predictable and makes backend work easier.
