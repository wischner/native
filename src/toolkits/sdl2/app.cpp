#include "native.h"
#include <SDL2/SDL.h>
#include <iostream>

namespace native
{
    int app::main_loop()
    {
        SDL_Event event;
        bool running = true;

        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                    running = false;
            }

            SDL_Delay(16); // idle
        }

        SDL_Quit();
        return 0;
    }
}
