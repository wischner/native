# Infinity mouse and shader design review

Reviewed 2026-10-07 against Native's architectural and C++ standards,
window peers, input lifetime state, graphics API, emulated hierarchy,
and platform/toolkit build definitions. Source proposals:
[Mouse](../../../infinity/docs/notes/MOUSE.md) and
[Shaders](../../../infinity/docs/notes/SHADERS.md).

The original review preceded implementation. The current public API includes
`mouse_capture` and `shader_view`; the implementation and coverage are described
in the standards, manuals and feature matrix. The corrections below explain
which parts of the original sketches required changes.

## Decisions consistent with Native

Keep cursor policy on `wnd`, including a hidden cursor value appended without
renumbering existing values. Keep mouse cancellation independent of keyboard
cancellation. Keep physical relative deltas independent of effect distortion.
Reuse the existing opaque window peer and resource-generation lifetime; do not
introduce another window/state registry. Native callbacks enter virtual
`on_native_*` hooks before public signals, including boundary, cancellation,
motion and shader-failure notifications.

A shader view is a drawing leaf with explicit child lifecycle. A graphics
device is distinct from a toolkit. Portable package/pass logic belongs in the
core, and native drawable integration belongs in its backend. Image fallback
must be explicitly distinguishable from successful effect execution.

## Required corrections to the sketches

| Proposal issue | Corrected contract |
| --- | --- |
| Capture token calls its query `active()` | Use `get_active() const`, matching Architecture Section 4. |
| Shader setters have no matching getters | Cache and expose animation, fallback, fit, source identity/time and parameter values through `get_...() const`. Return `shader_view &` for chainable property setters. |
| Effect installation before creation is unspecified | Validate and cache an owned package before creation. Device preparation occurs on creation; distinguish package acceptance from live installation and expose installation status. A live failed replacement retains the last valid effect. |
| `require_effect` says both installation failure and unavailable display | A rejected replacement preserves the previous installed effect. The unavailable display applies only when there is no usable installed effect. |
| Device state and portable property ownership are conflated | Portable requested values belong to the C++ control. Compiled programs, textures and surface resources belong to its peer/device owner. |
| `mouse_motion_event` enums and position validity lack defaults | Initialize every field. Relative events explicitly have no valid absolute position; consumers must use the kind discriminator. |
| Cancellation lists destruction as a publicly emitted reason | Destruction silently invalidates leases and clears backend state; never emit into an object undergoing teardown. Retain destruction as an internal cleanup cause. |
| Ordered leave/enter can destroy the proposed next target | Snapshot weak resource generations, clear old hover state before callbacks, then revalidate/re-resolve before entry. Never retain an unchecked borrowed target across a callback. |
| Capability queries describe unsupported and uncreated states ambiguously | Distinguish backend capability from active resource availability. Acquisition still validates current focus, visibility, enabled state and generation. |
| Non-finite motion, times and parameters are unspecified | Reject non-finite caller values and out-of-range parameters before publishing state. Validate extents and allocation arithmetic before allocating. |

## Existing implementation constraints

These observations describe the code inspected before this implementation; the
current fixes and remaining validation limits are recorded in the feature matrix.

`deepest_at()` is a hierarchy helper, not a complete pointer router. It starts
with the supplied root even for an outside point and does not apply input
eligibility. Its callers must first resolve root bounds, ancestor clipping,
native occlusion, client/chrome ownership and menu/modal takeover. A disabled
native child still occludes its parent: do not simply skip it and expose the
parent's hidden cursor underneath.

SDL2 resolves cursor policy after private canvas/split drag handling; those
early returns can bypass cursor refresh. It handles root entry but has no
matching root-leave cursor path. Its canvas focus-loss cleanup currently
fabricates releases at `(-1, -1)`. New cancellation must also reset private
control gesture state, rather than merely adding a public signal alongside
that behavior. Existing control drags and application capture must share
ownership arbitration.

SDL2 uses one root `SDL_Renderer`, accelerated when available and software as
a fallback. Logical children paint into root-owned viewports; menus and
popups are part of the same presentation. An independently presenting shader
child would break this ordering. Select compatible root rendering resources
before creation and keep one presentation per root frame. Per-frame GPU
readback into an `img` is a prototype path, not evidence of the proposed
real-time GPU composition contract.

X11-family cursor attributes belong to native windows. An invisible cursor
attribute alone does not distinguish privately painted chrome from client
pixels. Win32 already distinguishes client cursor messages from non-client
ones, but live policy application must use the same positional resolution.
AppKit's current cursor tracking area covers the whole visible view and
stores a fixed cursor. GEMix uses shared AES cursor state, which needs explicit
ownership and restoration; GPU support cannot be inferred from its viewer.

`img` owns noncopyable pixel storage and permits positive dimensions up to
`coord`'s maximum. Source submission must copy pixels explicitly; physical
drawable extents need their own 32-bit values. Do not widen the public window
geometry vocabulary as a side effect of shader allocation.

## Verification and documentation boundaries

Docker access was verified on this host. Compilation cannot establish outside
cursor restoration, raw relative input, GPU composition, or cross-device
color/history equivalence. Those need actual backend runtime evidence.

Mouse implementation requires Architecture Sections 3 and 5, window and
signal chapters, the painting/input tutorial, canvas/split gesture chapters,
and backend coverage updates. Shader implementation requires the painting and
custom-control standards, implementation and application graphics chapters,
build/distribution dependency documentation, and a justified custom-control
entry in `CUSTOM.md`. Add a dedicated chapter only when its implementation
exists. Keep planned APIs out of the current-behavior books.

The programmable shader proposal is a substantial rendering subsystem:
profile validation, a versioned package format, translation/reflection tooling,
three device adapters, toolkit composition, multipass/history, scheduling and
recovery. A small fixed set of CPU image effects would be a different feature;
it must not be advertised as implementation of arbitrary packaged shaders.


## Implemented shader design adjustment

The implemented programmable `native-image-1` language is bounded vector
instructions executed by shared CPU code. A standard-library Python expression
packager produces one validated package, with typed parameters, sampling,
ordered passes and previous-tick history. The existing canvas host and image
compositor preserve native hierarchy and presentation on every backend.
Capabilities explicitly report acceleration unavailable. This replaces the
original GLSL/SPIR-V translation and OpenGL/Direct3D/Metal proposal; it does not
satisfy that proposal's GPU performance or three-device acceptance gate.
Large CPU effects can block the UI, so the shipped demo processes source-sized
small images. GPU adapters remain absent and are not promised by the books.
