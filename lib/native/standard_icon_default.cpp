//
// Leaves command artwork to the application on backends without a catalog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/standard_icon.h>
namespace native {
    std::shared_ptr<const img> standard_icon(standard_icon_id) { return {}; }
}
