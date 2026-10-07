//
// Starts the Vision feature demonstration through the portable Native
// application entry point.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"
#include "infinity_window.h"
#include "vision_mouse_shaders.h"
#include "vision_retro_terminal.h"
#include <iostream>
#include <chrono>
#include <thread>

#include <string_view>

#include <native.h>

int program(int argc, char **argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--native-process-child") {
        for (int i = 2; i < argc; ++i) std::cout << argv[i] << "|";
        std::cerr << "helper error stream";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 7;
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--infinity-test") {
            vision::infinity_window window;
            return native::app::run(window);
        }
    }
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--mouse-shaders")
            return vision::run_mouse_shaders();
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--retro-terminal")
            return vision::run_retro_terminal(argc, argv);
    bool open_splitter = false;
    bool open_input_chrome = false;
    bool open_properties = false;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--split-view")
            open_splitter = true;
        else if (std::string_view(argv[index]) == "--input-chrome")
            open_input_chrome = true;
        else if (std::string_view(argv[index]) == "--properties")
            open_properties = true;
    }
    vision::vision_window window(open_splitter, open_input_chrome, open_properties);
    return native::app::run(window);
}
