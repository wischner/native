# Retro terminal test assets

The terminal source raster includes 32-pixel horizontal and 16-pixel vertical
black margins around its 80×25 text grid, keeping glyphs clear of curved edges.
The upstream shaders are unchanged by this application-side padding.

The SDL2 demo executes the original cool-retro-term GLSL shader bodies through
Native's `native-glsl-1` profile. The pass graph uses the upstream burn-in,
terminal frame, static terminal and dynamic terminal shaders, together with
Qt5Compat's original FastBlur shaders. Qt is not a runtime dependency.

Pinned cool-retro-term source: [fork-vault/cool-retro-term](https://github.com/fork-vault/cool-retro-term)
commit `1394ce82fa53d2a87d5adc4d99f9eeba598ea53d`.
The unmodified shaders and noise image are in `upstream/`. The runtime adapts
GLSL version and interface declarations for OpenGL 3.3; effect expressions,
texture operations and shader control flow remain upstream code.

`terminal-glsl.json` describes the pass graph and uniform bindings;
`terminal-glsl.nshader` contains its shader sources and RGBA noise texture.
The graph implements mode-1 rasterization and a selected set of upstream
appearance settings. This is a shader demo, not the complete Qt application.
The earlier `terminal.json` and `terminal.nshader` CPU approximation are not
used by the demo and do not establish acceptance of the original renderer.

Retain `GPL-3.0.txt` and the upstream attribution when redistributing these
assets. The associated upstream QML notices credit Filippo Scognamiglio
(2013–2021) and specify GPL-3.0-or-later. Qt5Compat FastBlur sources in
`upstream/qt/` are pinned to commit
`1cd988e734fc27fbbcd0915ff5af309f43783183`; retain their copyright and SPDX
notices and the accompanying LGPL text. These assets are external data;
Native's independently written C++ implementation remains MIT licensed.

`terminal.ttf` is the unmodified `PxPlus_IBM_EGA_8x8.ttf` from the same
cool-retro-term revision's `app/qml/fonts/oldschool-pc-fonts/`, part of the
Oldschool PC Font Pack by VileR. It is licensed CC-BY-SA-4.0; retain this
attribution and `FONT-LICENSE.txt`. Native loads it through the portable
TrueType rasterizer, including when SDL2_ttf is absent.

Regenerate the original GLSL package from the repository root:

```bash
python3 scripts/shaders/package_glsl.py src/assets/retro-terminal/terminal-glsl.json src/assets/retro-terminal/terminal-glsl.nshader
```

Python and Pillow are authoring dependencies. The SDL2 renderer requires an
EGL implementation supporting offscreen OpenGL 3.3. CMake copies this entire
directory beside Vision. Keep it beside deployed executables or supply
`--terminal-assets PATH`. Runtime validation of the original pipeline on
other platforms remains separate from the earlier CPU-profile tests.
