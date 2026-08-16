//
// Implements the Haiku application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <Application.h>
#include <iostream>
#include "globals.h"

namespace native
{

    int app::main_loop() {
        if (!haiku::global_app) {
            std::cerr << "Haiku: No BApplication instance available!"
                      << std::endl;
            return 1;
        }

        haiku::global_app->Run();
        delete haiku::global_app;
        haiku::global_app = nullptr;

        return 0;
    }

} // namespace native
