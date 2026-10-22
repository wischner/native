# Vision navigation icons

The settings navigator uses the **gear**, **calendar-days** and **clock**
solid icons from [Font Awesome Free 6.7.2](https://github.com/FortAwesome/Font-Awesome/tree/6.7.2/svgs/solid),
Copyright 2024 Fonticons, Inc. The SVG files retain their upstream attribution.
The icons are licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/);
[LICENSE.txt](LICENSE.txt) contains the upstream license statement.

The derived 32×32 PNGs add purple, red and blue rounded tiles and recolor the
original glyphs white. These modifications preserve the icon shapes. Their
license remains CC BY 4.0. The General page also displays the attribution and
license link. No Font Awesome font or runtime SVG renderer is required.

`icons.inc` embeds those PNG bytes into Vision through `navigation_icons.cpp`.
All backends receive the same ordinary Native image descriptors. Text-only
native list adapters retain their existing text presentation. AppKit presents
the icons at 24 logical pixels in 32-pixel navigation rows; normal selected-row
painting leaves the full-color tile visible in light and dark appearances.

To regenerate the PNGs and embedded bytes from the checked-in SVGs:

```bash
python3 scripts/assets/navigation-icons.py
```

The generator uses Python's standard library and the host's librsvg, Cairo and
GLib shared libraries. Normal CMake/Docker builds consume its checked-in output
and need none of those generation tools.
