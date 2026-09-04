# Bootstrap Icons raster assets

The PNG files in `message-box/` are derived from Bootstrap Icons at commit
`6945b7006285d444cc17ff2e22c7691719229526`:

- `information.png`: `circle-fill.svg` and `info-circle-fill.svg`
- `warning.png`: `triangle-fill.svg` and `exclamation-triangle-fill.svg`
- `error.png`: `circle-fill.svg` and `x-circle-fill.svg`
- `question.png`: `circle-fill.svg` and `question-circle-fill.svg`

The upstream SVGs were recolored and composited at high resolution, then
downsampled to 44 pixels and centered on a transparent 48 by 48 pixel canvas.
A matching white base silhouette keeps each symbol white on any window
background.

Bootstrap Icons is Copyright (c) 2019-2024 The Bootstrap Authors and is
distributed under the MIT license reproduced in `LICENSE`.

The generic filesystem PNGs under `filesystem/` use the same pinned source:

- `file.png`: `file-earmark-fill.svg`, recolored slate blue
- `folder.png`: `folder-fill.svg`, recolored blue

Both SVGs were rasterized at high resolution, proportionally reduced into a
transparent 64 by 64 pixel canvas, and are embedded into the Native static
library at CMake configure time. Backends use them only when the operating
system supplies no native file or directory icon.
