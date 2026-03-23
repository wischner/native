#include <native.h>
#include <Application.h>
#include <iostream>
#include "globals.h"

namespace native
{

    int app::main_loop()
    {
        if (!haiku::global_app)
        {
            std::cerr << "Haiku: No BApplication instance available!" << std::endl;
            return 1;
        }

        haiku::global_app->Run();
        delete haiku::global_app;
        haiku::global_app = nullptr;

        return 0;
    }

} // namespace native
