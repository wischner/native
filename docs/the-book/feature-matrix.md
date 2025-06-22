# Feature Support Matrix

This chapter tracks the current state of feature implementation across all supported platforms. It helps identify missing functionality and plan future development.

---

## Platforms

| Platform    | Status     | Notes                                                       |
| ----------- | ---------- | ----------------------------------------------------------- |
| Windows     | ✅ Stable  | Fully functional with MSVC.                                 |
| Linux (X11) | ⚠️ Partial | Core event handling works; advanced features pending.       |
| Haiku       | ⚠️ Partial | Basic functionality implemented; incomplete event handling. |

---

## Core Features

| Feature               | Windows | Linux (X11) | Haiku | Notes               |
| --------------------- | ------- | ----------- | ----- | ------------------- |
| Window creation       | ✅      | ✅          | ✅    |                     |
| Native handle access  | ✅      | ✅          | ✅    |                     |
| Event dispatch system | ✅      | ✅          | ✅    | Via `signal<>`.     |
| Main loop integration | ✅      | ✅          | ✅    | Uses `native::app`. |

---

## Input Events

| Feature            | Windows | Linux (X11) | Haiku | Notes                                       |
| ------------------ | ------- | ----------- | ----- | ------------------------------------------- |
| Mouse click events | ✅      | ✅          | ✅    |                                             |
| Mouse move events  | ✅      | ✅          | ✅    |                                             |
| Mouse wheel events | ✅      | ✅          | ⬜    | Not implemented on Haiku.                   |
| Keyboard events    | ✅      | ⚠️ Partial  | ⬜    | Limited support on Linux, pending on Haiku. |

---

## Rendering and Graphics

| Feature                | Windows | Linux (X11) | Haiku | Notes                           |
| ---------------------- | ------- | ----------- | ----- | ------------------------------- |
| Basic drawing API      | ✅      | ✅          | ✅    | `gpx` class.                    |
| Draw rect              | ✅      | ✅          | ✅    |                                 |
| Draw lines             | ✅      | ✅          | ✅    |                                 |
| Clipping (`clip_rect`) | ✅      | ⬜          | ⬜    | Not yet working on Linux/Haiku. |
| Image buffer (`img`)   | ✅      | ✅          | ✅    | Always 32bpp RGBA.              |
| Double buffering       | ✅      | ⚠️ Flickers | ⬜    | Linux needs proper buffering.   |
| DPI awareness          | ⬜      | ⬜          | ⬜    | Not implemented.                |

---

## Window Management

| Feature               | Windows | Linux (X11) | Haiku | Notes                       |
| --------------------- | ------- | ----------- | ----- | --------------------------- |
| Resizing              | ✅      | ✅          | ✅    |                             |
| Minimizing/Maximizing | ✅      | ⬜          | ⬜    | Missing on Linux and Haiku. |
| Custom cursors        | ✅      | ⬜          | ⬜    | Pending.                    |
| Clipboard access      | ⬜      | ⬜          | ⬜    | Not yet implemented.        |
| Drag & drop           | ⬜      | ⬜          | ⬜    | Not yet implemented.        |

---

✅ **Legend**

- ✅ Fully implemented and tested
- ⚠️ Partially implemented or unstable
- ⬜ Not yet implemented
