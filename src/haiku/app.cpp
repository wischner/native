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

        std::cout << "Haiku: entering BApplication Run loop." << std::endl;
        haiku::global_app->Run();

        return 0;
    }

} // namespace native
