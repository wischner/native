//
// Resolves established command artwork supplied by the current backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <memory>
namespace native {
    class img;
    enum class standard_icon_id {
        file_new, file_open, file_save, file_save_as, file_close,
        cut, copy, paste, undo, redo, print, find
    };
    // Return unscaled immutable artwork, or null if the backend has no
    // established icon for this operation. Callers may supply a fallback.
    std::shared_ptr<const img> standard_icon(standard_icon_id id);
}
