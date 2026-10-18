# Patterns: Stock and Portable Fonts

This chapter expands Architecture Section 10. The public `font_t` and its
measurements contain portable values and an opaque numeric identifier. Native
font objects and parsed TrueType state remain private. Application drawing
examples are in [Drawing primitives](DRAWING-PRIMITIVES.md) and
[Programming Native, Chapter 10](../programming-native/10-GRAPHICS-IMAGES-FONTS-THEMES.md).

## Two resource paths

`font_t::stock()` returns a borrowed process-owned reference for a semantic
role: system, fixed, icon label, title, small or control. Each backend obtains
its corresponding native resources. Several roles can share a face, and
stock pixels and metrics can differ across machines. SDL2 can use SDL2_ttf;
without it, its stock roles retain the built-in bitmap path. On a scaled
desktop, SDL2 measures with the logical face and draws window text with a
device-size copy of it (see
[Painting](PATTERNS-PAINTING.md)). X11 core fonts are bitmaps, and
server-magnified bitmaps look bold and blocky. So each X11 font keeps its
logical face for image text, and adds the installed, unscaled bitmap face
nearest its pixel size times the factor for window text: the same family
first, then Helvetica or Lucida for proportional faces, and Lucida
Typewriter, Terminus or Fixed for character-cell faces. Metrics and
measurement come from that device face, divided by the factor, so layout
matches the drawn text. GEMix uses its
stock text adapter. Stock font initialization belongs to the initialized
backend and is distinct from portable byte-backed font loading.

`font_t::from_file()` reads the entire selected resource through a C++ stream;
`from_memory()` copies the supplied byte span. Both delegate registration to
`lib/native/portable_font.cpp`, selecting an explicit face index and pixel
size. The implementation bounds font data to 128 MiB and accepts portable
pixel sizes from 1 through 4096. Invalid data, an invalid face or invalid size
returns an invalid font rather than substituting a stock face. A successfully
loaded file font records its filesystem path; a memory font owns its copied
bytes independently of the caller.

## Registration and lifetime

The private portable registry associates IDs starting at `0xf0000000` with
owned byte vectors, `stbtt_fontinfo`, descriptions, metrics and scale. Its
mutex protects registration, lookup, measurement, rasterization and release.
This is a non-window resource registry; window state still belongs to peers.
The registry object itself has process lifetime, while individual faces are
released by `font_t` destruction. Backends check the portable ID range before
releasing native font bindings.

`font_t` is move-only. Its move transfers the identifier without copying a
native resource. `gpx::set_font()` borrows the selected `font_t`; it does not
extend that object's lifetime. Keep the font alive through every measurement
and drawing operation, and restore borrowed graphics state before changing
or destroying a selected font.

## Measurement and drawing

`gpx` delegates font metrics, text measurement and Unicode-character
measurement to its selected font. The common `gpx::draw_text()` checks whether
the font is portable before entering a backend text primitive. Portable text
uses the same UTF-8 decoder, advances, kerning, bounds and `stb_truetype`
rasterizer on every backend, then composes a transparent image through the
normal image graphics path. Stock text uses the backend's native text path.

The portable decoder replaces malformed UTF-8 with the replacement character.
`measure_character()` rejects surrogate code points and values above
`U+10FFFF`. Measurement and rasterization share layout calculations; an empty
string has zero width and advance while retaining the selected line height.
An explicitly selected invalid font draws nothing. A graphics context without
an explicitly selected font uses the stock system role.

## Installed-font discovery and boundaries

`enumerate_installed()` returns family/style descriptions, face indexes and
filesystem paths; it does not convert a discovered host font into a portable
application asset. Supply the exact resource bytes when pixel identity matters.
Shared traversal and stream reads use `std::filesystem` and standard C++ I/O.

Private platform `font_locations.cpp` adapters provide roots and additional
Windows registry entries; core enumeration only traverses/reads them with
standard C++. SDL2 uses optional Fontconfig directly, without a shell or
`fc-match` subprocess. Missing Fontconfig or SDL2_ttf falls back predictably to
bitmap stock text. Portable font resource selection does not use this stock
font fallback chain. See [ADJUST.md](../../ADJUST.md), A10/A14.

## Verification

The window API suite covers public font types and behavior. The terminal
screen suite uses the attributed file-backed font asset, exercising owned
portable data and real text pixels. Backend runtime suites cover native text
and font-dependent control metrics to different extents. These checks do not
establish a complete shaping engine or identical stock text across systems.
Vendored rasterizer provenance and its formatting exemption are recorded in
[the third-party inventory](../../../lib/native/third_party/README.md).
