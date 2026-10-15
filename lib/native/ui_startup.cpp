//
// Supplies no-op startup exclusion for backends with one native UI thread.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "ui_startup.h"
namespace native::detail
{
    void begin_ui_startup() {}
    void end_ui_startup() {}
} // namespace native::detail
