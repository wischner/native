//
// Starts the Vision feature demonstration through the portable Native
// application entry point.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <native.h>

int program(int, char **) {
    vision::vision_window window;
    return native::app::run(window);
}
