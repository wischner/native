# Patterns: Portable Image Shaders

This chapter expands Architecture Section 21. `shader_view` is a drawing leaf
built on the existing `canvas` host. Every backend composes its output through
its normal image drawing and clipping; it does not present another window or
replace the root's renderer. Menus, native editor siblings, rulers, and tabs
retain their existing paint and input routes.

## A bounded portable executor

The implemented `native-image-1` profile is a small vector instruction
language executed on the CPU. It supports programmable sampling, arithmetic,
color transforms, distortion, multiple ordered passes and retained previous
outputs. It is not GLSL, HLSL, Metal or a fixed list of built-in effects.
`shader_capabilities::accelerated` is false on every backend. No GPU renderer,
shader cross-compiler, device recovery adapter, or GPU texture sharing is
claimed by this implementation.

This differs from Infinity's original three-device GPU proposal. The shared
executor preserves Native's small implementation and existing native
composition while giving GEMix and other hosts the same executable effect
package. Large effects run synchronously on the UI thread and can delay input;
use source-sized passes for small emulator frames rather than requesting
full-screen processing. The CPU path is not a performance substitute for the
proposed GPU system.

## Source ownership and installation

Public setters cache portable values before creation. `set_source()` explicitly
copies the noncopyable `img`'s pixels before returning. Sequence and source time
are application-owned identities; they do not derive from redraw frequency.
Use a scoped latest-value UI delivery for worker frames.

`shader_package` shares immutable, owned validated data and can be decoded on a
worker. File operations use `std::filesystem::path` and C++ streams. One package
is limited to one MiB, sixteen uniquely named parameters, eight passes and 128
instructions per pass. Parsing rejects unknown versions/opcodes, invalid
registers, non-finite constants, invalid parameter defaults/ranges, forward
current-tick dependencies and unretained history references.

`set_effect()` caches an accepted package before creation and reports `pending`.
A live view prepares the candidate with the current source before publishing
its package/default parameters. A rejected replacement leaves the previous
effect and values intact. `get_shader_status()` distinguishes no effect,
pending, active and unavailable independently of displayed fallback pixels.
A view without a submitted source has no image to process.

Compiled descriptors are immutable package data. Outputs, history, diagnostics
and clock baselines belong to the existing opaque window peer. Destroy/recreate
retains cached source/package/properties and discards transient output/history.
No window-to-renderer map or platform type enters a public header.

## Passes and pixels

Inputs name the source, an earlier current-tick output, or a retained output
from the previous completed tick. New pass outputs are built separately;
history commits only after all passes and output conversion succeed. Initially
history is transparent black. `clear_history()`, resizing, source-dimension changes, recreation and time
gaps greater than 250 milliseconds reset it and the presentation time baseline.

Targets use source or logical client extent, optionally reduced by an integer
from one to sixteen with ceiling division. All extents are checked in wider
arithmetic before allocation. Extents are limited to 4096 on either axis;
the conservative per-tick allocation estimate is bounded by 256 MiB and work by 128 Mi vector
instructions per tick. Unsupported extents/budgets produce an explicit error.
Transactional replacement retains old installed output/history during candidate
preparation; that retained storage is additional to the candidate estimate.
Targets use logical pixels; this CPU profile does not query physical GPU
backing extents or expose DPI-dependent device counts to shader authors.

Source pixels are top-to-bottom straight-alpha sRGB `rgba`. The executor
converts RGB to linear and premultiplies once. Intermediate four-float pixels
are linear premultiplied values, including extended range. The last output is
unpremultiplied and encoded to sRGB RGBA8 for the backend image compositor.
A fully transparent output has zero RGB. Normalized texture coordinates have
top-left origin; nearest/linear sampling clamps to the image edges. Division by
zero, invalid arithmetic and overflow have the defined result zero.

`fit`, `fill`, and `stretch` determine the destination rectangle from source
aspect ratio and the full client rectangle. `get_image_viewport()` returns that
local rectangle, including the outside edges of a fill crop. The paint clip
still excludes chrome and siblings. Window rectangles retain Native's existing
16-bit geometry limits; internal allocation arithmetic does not narrow them.

## Scheduling and fallback

`source_updates` caches the completed output until source, parameters, history
or geometry changes. It does not re-execute merely because an expose repaints
the same pixels. Continuous mode uses one shared stop-aware clock, with at most
one pending `app::post()` invalidation per view and a nominal 16 ms interval.
The worker never calls windows, signals or graphics. UI delivery validates the
weak resource generation and visible state before invalidating. Tick pacing
coalesces requests; it does not promise a particular frame rate.

`original_image` is the default fallback. `require_effect` paints an opaque
black unavailable client when no effect output can be used. Runtime execution
failure disables paced retries, emits one virtual `on_native_shader_error()`
diagnostic, and applies the selected fallback. A source/property change allows
another attempt. A failed replacement does not discard a valid prior effect.

The protected `draw_background()` and `draw_image()` stages contain the complete
default drawing and can be overridden. The inherited paint notification follows
them as an overlay, with ordinary canvas chrome after the subscriber. Borrowed
graphics state and clipping are restored before that dispatch. Shader drawing
has no input hit-testing role: the same pointer router handles original pixels,
effects, letterboxing and surrounding chrome.

## Authoring and verification

The standard-library Python tool `scripts/shaders/package.py` parses expression
manifests as data with `ast`; it never executes Python/effect code. It emits the
same package for every backend, without target-specific reflection or compilers.
The package format and application example are in
[Portable image shaders](../programming-native/20-IMAGE-SHADERS.md).

The independently written CRT fixture contains persistence, reduced bloom and
curvature/scanline/tint passes. `vision --mouse-shaders` embeds that package and
places the view beside a native editor. Shared conformance tests exercise real
packages and pixels, transactional failure, owned sources, limits, and resource
recreation. Desktop pointer restoration, platform paint ordering and measured
large-frame performance remain distinct from those portable assertions; see
[the feature matrix](FEATURE-MATRIX.md).

The terminal acceptance mode `vision --retro-terminal` submits a 704×232
application-owned ANSI font raster: an 80×25 grid with 32-pixel horizontal
and 16-pixel vertical black margins to protect edge text from curved glass. Its initial 800×600 CRT
area gives the stretched terminal a 4:3 display, with command controls below.
It loads the external `native-glsl-1` package containing original
cool-retro-term burn-in, terminal frame, static and dynamic shaders and
Qt5Compat FastBlur shaders. Shader bodies execute as GLSL, with interface and
version syntax adapted to an offscreen OpenGL 3.3 context. The eleven-pass graph
includes noise and retained burn-in textures. The library owns programs,
textures and context behind the shader view's peer; public headers expose no
OpenGL handles. SDL2 has displayed this pipeline. Other adapters require
separate validation; previous CPU-profile tests do not prove GLSL support.
External attributed assets travel beside Vision; Qt is not a runtime dependency.
See the asset README for provenance and the programming chapter for the demo.
