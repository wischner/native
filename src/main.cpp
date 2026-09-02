//
// Starts the Vision feature demonstration through the portable Native
// application entry point.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <string_view>

#include <native.h>

int program(int argc, char **argv) {
    bool open_docking = false;
    bool open_input_chrome = false;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--docking")
            open_docking = true;
        else if (std::string_view(argv[index]) == "--input-chrome")
            open_input_chrome = true;
    }
    vision::vision_window window(open_docking, open_input_chrome);
    return native::app::run(window);
}
