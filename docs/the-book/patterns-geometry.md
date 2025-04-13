# Patterns: geometry and type conventions

This chapter describes the design principles behind the geometry types used in the **native** UI library — including how points, sizes, and rectangles are represented, and how coordinates are treated consistently across all platforms and toolkits.

---

## Coordinate type: `coord`

At the core of all geometry is the `coord` type — a type alias that defines the scalar used for screen coordinates, widths, and heights:

```cpp
using coord = int;
```

By using a single alias, the library can easily switch to other types (like `float` or `double`) in the future if needed — or to use fixed-point math in constrained environments. All positions and dimensions are expressed in `coord` units.

---

## `point`

Represents a location on a 2D plane, defined by its `x` and `y` coordinates:

```cpp
struct point
{
    coord x = 0;
    coord y = 0;

    point() = default;
    point(coord x_, coord y_);
};
```

Points are used to specify positions of elements, mouse coordinates, and corners of rectangles.

---

## `size`

Represents a two-dimensional size, defined by width and height:

```cpp
struct size
{
    coord w = 0;
    coord h = 0;

    size() = default;
    size(coord w_, coord h_);
};
```

`size` is used to define dimensions of windows, images, or any drawable object.

---

## `rect`

The rectangle is the most important composite geometry type, built from a `point` (origin) and a `size` (extent):

```cpp
struct rect
{
    point p;
    size d;

    rect() = default;
    rect(point p_, size d_);
    rect(coord x, coord y, coord w, coord h);

    coord x1() const;
    coord y1() const;
    coord x2() const;
    coord y2() const;

    coord width() const;
    coord height() const;

    bool contains(point pt) const;
};
```

The `rect` abstraction makes it easier to manage areas and bounds — and provides utility methods like `contains()` to test whether a point lies inside it.

---

## Accessors and utility

The API design favors accessor methods like `x1()`, `x2()`, `width()`, `height()` for consistency and composability. For example, you can:

- Get the top-left and bottom-right corners
- Calculate width/height even when rect is constructed differently
- Avoid needing direct access to raw fields

---

## Immutability by convention

All geometry types are **value types**, designed to be cheap to copy and passed by value. They have no heap allocations, no virtual functions, and no polymorphism. This ensures performance and correctness in tight loops like rendering, hit-testing, and layout.

---

## Geometry in the graphics stack

The same geometry types are used throughout the graphics system:

- `point` and `rect` are used in `gpx::draw_line`, `draw_text`, `draw_img`
- `rect` defines the clipping region
- `size` is used when creating `img` or defining window dimensions

By using a consistent set of lightweight types, all drawing and layout operations speak the same "language."

---

This geometry layer forms the mathematical backbone of all rendering, event processing, and window layout in **native**. It is simple by design — and powerful in composition.
