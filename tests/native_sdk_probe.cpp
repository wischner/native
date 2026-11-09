//
// Link-check the complete SDK and its relocated CMake consumer interface.
// The executable is never run or included in a release archive.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

// Exercise the portable entry point and window construction at link time.
int program(int, char **) {
    native::app_wnd window("SDK link check");
    return native::app::run(window);
}
