# Production Source TODOs

This note inventories TODOs in the production code paths under `include/` and
`src/`.

## Current TODOs

| File | Issue |
| --- | --- |
| `include/native.h` | `rgba` still has a TODO for endian handling. |
| `src/platforms/haiku/screen.cpp` | Haiku screen detection still has a TODO to detect usable work area instead of using full screen bounds. |

## Notes

- This inventory is intentionally small because the current production tree has
  very few explicit TODO markers.
- When a production TODO is added or removed, this note should be updated in the
  same commit.
