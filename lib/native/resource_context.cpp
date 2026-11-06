//
// Supplies CMake-selected backend identity for automatic resource lookup.
// Rendering profile constants describe Native's current drawing policy.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/resources.h>

namespace native
{
    resource_context resources::current_context() {
        return {NATIVE_RESOURCE_PLATFORM, NATIVE_RESOURCE_TOOLKIT,
                NATIVE_RESOURCE_COLOR, "", 0};
    }
}
