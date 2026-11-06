# Resource Packs

`native::resources` is the shared resource container. It reads and writes
version 1 binary packs, compiles JSON manifests, and selects a named variant.
The command-line compiler in `tools/rc/` consumes this public API. This
chapter expands the resource contract in Architecture Section 17.

## Ownership and types

A pack owns a vector of `resource_variant` values. Each value contains a
logical name, a type label, a `resource_context` selector, and owned bytes.
Names such as `assets.icons.platform-icon` are opaque identifiers: the word
`icons` has no parsing significance. All variants of a name must have the
same type. Types are application-defined labels; `icon`, `text`, `font` and
`blob` are conventions, not different C++ payload types.

`get(name)` returns a const reference including the selected type, selector
and bytes. The caller can check the type before decoding. An icon payload
can be passed to `img::decode()`. The pack neither decodes nor converts image
formats, generates monochrome artwork, recolors masks, nor updates controls.
There is no deferred icon reference or image cache in this version.

Returned references can be invalidated by `add`, assignment or destruction.
`load` and `decode` return independent owning containers; embedded input can
expire after decoding. Concurrent const queries are safe when no thread
mutates the same container. An empty pack is valid.

## Selection

`get(name)` uses `current_context()`, which does not initialize a GUI or
open a display. CMake supplies private compiled metadata:

| Build | Platform | Toolkit | Color |
| --- | --- | --- | --- |
| Windows | `windows` | unspecified | `color` |
| macOS | `macos` | unspecified | `color` |
| Haiku | `haiku` | unspecified | `color` |
| X11 | `linux` | `x11` | `monochrome` |
| GEMix, either transport | `linux` | `gemix` | `monochrome` |
| Other Linux backends | `linux` | `sdl2`, `motif`, `openlook`, `wmaker`, `wayland`, or `elementary` | `color` |

X11 and GEMix profiles reflect Native's current monochrome window drawing,
not the host framebuffer depth. Theme and physical pixel size are unspecified.
There is no live display-depth, theme, DPI or per-window detection in v1.
`get(name, context)` permits explicit contexts for appearance-aware callers,
offscreen output and deterministic tests without changing global state.

Selection first excludes variants naming a different platform or toolkit.
It ranks remaining entries lexicographically, in this order:

1. Color: exact nonempty match, unspecified, then mismatch.
2. Toolkit: exact nonempty match, then unspecified.
3. Platform: exact nonempty match, then unspecified.
4. Theme: exact nonempty match, unspecified, then mismatch.
5. Size: with a requested size, specified sizes precede unspecified; without
   a request, unspecified precedes specified.
6. Smallest absolute pixel-size difference; larger wins equal distance.
7. Earlier insertion/manifest order wins an otherwise equal rank.

Thus a generic monochrome representation outranks a full-color
platform-specific representation on a monochrome profile. There is no
numerical concept of a “nearby” operating system. A valid serialized pack
requires one unqualified default per name, ensuring fallback is available.
Applications must choose defaults that their consumers can decode/render.
An unknown name throws `std::out_of_range`; lookup never substitutes another
resource name. Programmatically assembled packs may be incomplete until
encoded; querying an incomplete pack can also fail for lack of an eligible
variant.

## Manifest

The source manifest is UTF-8 JSON, at most one MiB. See the
[application tutorial](../programming-native/12-BUILDING-AND-DISTRIBUTING.md#resource-packs)
for a complete manifest and usage snippets.

The root has exactly `version` (integer 1) and `resources` (array). Each
resource has `name`, `type`, and a nonempty `variants` array. Each variant
has `file` and an optional `when` object. Its allowed fields are `platform`,
`toolkit`, `color`, `theme`, and `size`. Missing/empty strings and zero size
mean unspecified. `color` is `color` or `monochrome`; `theme` is `light` or
`dark`; size is a physical square-icon edge in pixels, from 0 to 65535.
Metadata does not validate the dimensions or format of payload bytes.

Names, types, platform and toolkit labels are case-sensitive ASCII strings
using letters, digits, dot, underscore and hyphen, at most 1024 bytes.
Names and types cannot be empty. Asset paths support Unicode, resolve
relative to the manifest directory, and may be absolute or contain `..`.
Manifests are trusted build inputs, not a sandbox for filesystem reads.

Duplicate JSON keys, unknown fields, duplicate names/selectors, mixed types,
missing defaults, invalid UTF-8/Unicode escapes, trailing data and unsupported
versions are rejected. The parser accepts only the schema's objects, arrays,
strings and unsigned 32-bit integers; booleans, null, negative or fractional
numbers are not manifest values. Recursion is limited to 16 levels, strings
to 4096 UTF-8 bytes and arrays to 4096 elements. Assets are copied byte-for-byte.

## Binary format, version 1

The conventional extension is `.resources`. Readers use the signature,
not the extension; `.resource` works too. All integers are unsigned 32-bit
little-endian. There is no native-struct layout, alignment padding, compression,
checksum, executable content, or archive extraction.

| Field | Encoding |
| --- | --- |
| Signature | Eight bytes: `4e 52 45 53 0d 0a 1a 0a` |
| Version | Integer `1` |
| Variant count | Integer, at most 4096 |
| Records | Exactly count records in insertion order |

Each record contains six length-prefixed strings, in order: name, type,
platform, toolkit, color, theme. A string is its byte length followed by
exactly that many ASCII bytes. Next are size, payload byte length, and payload
bytes. The entire file, including metadata, is at most 64 MiB.

Readers reject truncated fields, oversized lengths, unknown versions,
invalid metadata and trailing bytes. Decoding owns and validates the entries
before returning. Encoding preserves insertion order and produces identical
bytes for identical entries. The writer validates before opening its output,
then overwrites it using standard streams; publication is not atomic and an
I/O failure can leave a partial output. It does not create parent directories.

## Build and embedding

Normal backend builds include CMake target `native_rc`, whose executable is
`build/<backend>/tools/rc/rc` (`rc.exe` on Windows). It defines a conventional
console `main` and links Native's resource implementation without starting
Native's GUI loop. It requires no JSON or archive dependency.

The CLI is:

```text
rc MANIFEST.json -o OUTPUT.resources [--cpp OUTPUT.cpp]
rc --help
```

Successful compilation exits 0, usage errors exit 2, and manifest/I/O errors
exit 1 with an error on stderr. Output files are overwritten. Input manifest,
pack output and optional C++ output must have distinct canonical paths.
Choose output paths separate from all source assets. With `--cpp`, both the
binary pack and C++ file are written; failure of the second write does not
roll back the first.

Generated C++ defines `native_embedded::resource_data()`, returning a span
of a static const byte array. Compile it once and declare the accessor in
application code. One generated source is supported per executable because
the accessor has a fixed name; combine assets in one manifest. Decoding copies
and owns the pack; it is not a zero-copy view. Cross-built target compilers
are not run on the host: compile packs with a hosted SDK's `rc`, then compile
the generated C++ with the destination compiler. Packs are target-independent
and contain every supplied variant; v1 does not filter by target.

See [Build system](BUILD-SYSTEM.md#resource-compiler) for Docker commands and
the registered tests. The existing Vision navigation artwork remains on its
current embedded-PNG path; this resource infrastructure does not migrate its
controls or supply new icon artwork.
