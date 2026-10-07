# Portable Image Shaders

`native::shader_view` displays an owned image through a programmable,
portable CPU image-effect package. It is an ordinary canvas drawing leaf:
assign a created parent, create it, show it, and use the existing layout and
pointer signals. Every backend uses the same instruction profile; GPU
acceleration is not implemented.

```cpp
native::shader_view display(native::rect(0, 0, 512, 384));
display.set_parent(&window);
auto effect = native::shader_package::load("display.nshader");
std::string error;
if (!display.set_effect(effect, error))
    throw std::runtime_error(error);
display.set_source(frame, sequence, source_seconds);
display.set_fit(native::shader_fit::fit);
display.create();
display.show();
```

`set_source()` copies pixels; the caller can immediately reuse the image.
Source identity/time are independent of presentation time. Submit worker frames
through `ui_sender::post_latest()` on the UI thread. Packages can be decoded on
workers; installation, window operations and property changes use the UI thread.

Properties have matching getters. Parameters require the package's exact
`shader_value` alternative: an `int`, `float`, or two/three/four-float array.
Unknown names, wrong types, non-finite values and out-of-range components throw
`std::invalid_argument`. A failed live effect replacement returns false and
preserves the previous effect. `get_shader_status()` distinguishes accepted
pre-create packages (`pending`) from active and unavailable resources.

```cpp
display.set_parameter("scanlines", 0.4f);
display.set_parameter("tint", std::array<float, 4>{1, 0.65f, 0.2f, 1});
display.set_animation(native::shader_animation::continuous);
display.clear_history(); // Reset phosphor state after discontinuous content.
```

`source_updates` processes changed source/properties and caches the image for
ordinary exposures. `continuous` requests paced, coalesced invalidations for
time-based effects. A shared clock does not guarantee that an expensive CPU
program will achieve 60 FPS. Effects execute synchronously on the UI thread;
prefer processing the original small image and letting normal drawing scale it.
History resets on geometry/source-dimension changes and gaps over 250 ms. Default failure policy
shows original pixels; `require_effect` shows black when there is no usable
effect output. Observe `on_shader_error` and status rather than interpreting
fallback pixels as shader success.

## Author one portable effect

The developer tool needs only Python 3. It parses expressions as data; arbitrary
Python statements, imports, calls and loops are rejected. An identity manifest:

```json
{
  "profile": "native-image-1",
  "parameters": [],
  "passes": [{
    "name": "display",
    "program": [["color", "sample(source, uv)"]],
    "output": "color"
  }]
}
```

Compile it once and distribute the resulting file with the application:

```bash
python3 scripts/shaders/package.py effect.json build/effect.nshader
```

There are no runtime shader/compiler dependencies. The executable package is
portable instructions, not a `.qsb`, SPIR-V, GLSL, HLSL or Metal binary. This
profile intentionally uses CPU execution on Linux, Windows, Haiku and macOS.
It differs from the GPU design discussed in Infinity's original proposal.

A manifest supports up to sixteen parameters with `name`, `type` (`int`,
`float`, `float2`, `float3`, `float4`), `range: [minimum, maximum]` and `default`.
Passes have a unique `name`, `extent` (`source` default or `viewport`), integer
`reduction` (1..16), boolean `history`, expression `program` and output name.
`sample(source, uv)` samples original pixels; a previous pass name samples its
current output; `history_name` samples a retained previous-tick output from a
pass declared with `history: true`, including the current pass itself.

Built-in registers are four-float vectors:

| Name | Components |
| --- | --- |
| `uv` | Pixel-center normalized x/y and reciprocal current-pass width/height |
| `source_info` | Source width/height, sequence converted to float, source time |
| `clock` | Presentation elapsed/delta seconds, logical client width/height |
| Parameter name | Exact scalar/vector components; scalars broadcast to four components |

Sequence getters retain the original uint64 identity; shader math uses float
and cannot distinguish every large sequence value. Texture orientation is
always top-left, and nearest/linear sampling clamps edges. Working RGB is
linear and premultiplied; the compositor receives encoded straight-alpha pixels.
Include alpha deliberately in constants and color transforms.

Expressions support `+`, `-`, `*`, `/`, unary negation, numeric `vec4(...)`,
component swizzles (`xyzw`/`rgba`), `min`, `max`, `pow`, `dot`, `step`, `sin`, `cos`,
`exp`, `floor`, `abs`, `sqrt`, `clamp` (0..1), `mix`, `sample` and `linear`.
There is no executable callback, scripting runtime, dynamic branch, unbounded
loop, arbitrary memory access or native graphics handle. Up to 44 writable
vector registers make authoring work bounded; excessive expressions fail
packaging. The Native loader independently validates the emitted program.

## Package format version 1

Packages are bounded ASCII text. They start with `native-image-1 1`, followed by
`parameters N` and N `parameter name type minimum maximum defaults...` records.
Type codes 0..4 correspond to the parameter alternatives above. Integer defaults
retain their exact int32 value; execution converts integer uniforms to float. Then `passes N`
and N `pass name source|viewport reduction history output_register instructions`
records precede their instruction lines; `end` terminates the package. Trailing
records, unknown opcodes and unsupported versions are errors.

Registers 0..2 contain built-ins, 3..18 contain declared parameters, and 20..63
are writable. Every instruction starts with opcode and destination. Constants
have four literals; arithmetic takes input register indices; `mix` takes three;
`swizzle` takes an input and four component indices. `sample`/`linear` take a
texture index and coordinate register. Texture -1 is source, nonnegative indices
are earlier passes, and `-(index+2)` is that pass's retained previous output.
The loader validates register initialization and pass/history dependencies.

Resource limits are 4096 pixels per extent, eight passes, 128 instructions per
pass, a one-MiB package, conservative 256-MiB per-view allocation and 128-Mi
vector instructions per tick. Exceeding limits reports unavailable execution
and uses the chosen fallback. These are limits, not throughput guarantees.

## Mouse behavior and acceptance

Set `display.set_cursor(native::mouse_cursor::hidden)` to hide over its eligible
client. It does not change guest sensitivity or apply shader distortion to
pointer coordinates. Letterbox-specific hiding can be implemented by overriding
`get_cursor_at()` and consulting `get_image_viewport()`. Chrome and siblings
retain their own policy. Entry/leave update the motion baseline; cancellation
clears unreliable held state. See [Painting and input](02-PAINTING-AND-INPUT.md)
for a capture example and capabilities.

Run `vision --mouse-shaders` for the independent three-pass CRT package,
color/amber/green styles, a native editor, original-image mode, hover hiding,
relative-mode acquisition and Escape/menu release. Click the display before
acquiring focus-dependent capture. Test outside movement, menus/dialogs,
stationary toggles, two views, resizing, focus loss and destruction on the
actual target desktop. Unit tests or a dummy driver cannot prove those host
behaviors. See [Feature Matrix](../book-of-native/FEATURE-MATRIX.md) for current
build and test evidence.

## Terminal acceptance application

Run `vision --retro-terminal` for the CRT shader monitor demo. It combines a
fixed 80×25 ANSI screen, an 8×8 portable font raster, a shader view and optional
native command controls. The 640×200 glyph grid has 32-pixel horizontal and 16-pixel vertical
black margins, producing a 704×232 source raster that keeps edge text clear
of the curved glass. This padded raster is stretched to a 4:3 initial
CRT area: the initial client size is 800×636, including a 36-pixel command row.
The Display menu selects tint, original pixels, animation and cursor hiding.
Terminal text is a fixture for inspecting effects; command execution is not
required for the shader demo.

The demo loads `terminal-glsl.nshader`, which executes the original
cool-retro-term burn-in, frame, static and dynamic GLSL shader bodies and the
original Qt5Compat FastBlur passes. The eleven-pass graph carries the noise
texture and previous burn-in image. The OpenGL adapter changes version and
interface declarations for compatibility, while preserving the effect code.
The SDL2 preview has run this original pipeline; validation of other platform
adapters remains separate from earlier CPU-profile acceptance.

Shader and font provenance is in [the asset README](../../../src/assets/retro-terminal/README.md).
The GPL shader and CC-BY-SA font notices accompany copied executables. Native's
library does not embed these assets. Qt is not required at runtime; an OpenGL
3.3 context is required for this GLSL profile.


`help`, `colors`, `clear` and `echo TEXT` are portable built-ins. Other commands
execute through `/bin/sh -c` on POSIX hosts or Windows PowerShell `-Command` on Windows, using
Native's public process API. The application deliberately runs user-entered
command-shell text; Native's process API itself still launches without shell
expansion. Native text entry supplies platform text editing; Run also works on
GEMix where physical-key Enter delivery is unavailable.

This is a line-oriented command terminal, not a PTY/full VT emulator. Input is
disconnected for children, so interactive shell programs such as Vim, passwords
and REPLs cannot be used. The parser supports ASCII, sixteen SGR colors,
CR/LF/backspace/tab, cursor addressing/movement, save/restore, erase and OSC
suppression. Non-ASCII glyphs display as `?`; unsupported sequences are ignored.
There is no scrollback, terminal resize protocol, or session-persistent shell
state. Each command has independent working-directory/environment state.

Captured output is limited to 32 KiB per stream. A worker posts cumulative
snapshots through scoped latest-value delivery, so coalescing does not lose
captured bytes; stdout/stderr cross-stream ordering is not preserved. Cancel
requests child cancellation. Closing invalidates delivery before stopping and
joining the worker. The existing process contract owns the shell child and does
not promise descendant-tree cancellation.

`vision --retro-terminal --terminal-smoke` renders an ANSI fixture and exits
successfully only after a real host-shell echo command returns its expected
output and the view reports active effect output. Missing assets,
font/package errors and shader failures are failures. On Linux this is a CTest
alongside deterministic ANSI/parser/pixel tests; X11-family runs use Xvfb.
Windows, Haiku and macOS can run the same command on the target desktop. Copy
the complete `retro-terminal/` asset directory next to the executable; when
launching by an unresolved PATH name, use `--terminal-assets PATH` explicitly.
Use a Release Docker build for interactive CPU effect performance.
