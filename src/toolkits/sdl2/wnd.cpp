#include "native.h"
#include <SDL2/SDL.h>
#include <iostream>

namespace native
{
    static SDL_Window *s_window = nullptr;

    void wnd::create() const
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            return;
        }

        s_window = SDL_CreateWindow(
            "SDL Window",
            x(), y(), width(), height(),
            SDL_WINDOW_SHOWN); // we could change this to 0 and defer show()

        if (!s_window)
        {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
            SDL_Quit();
        }
    }

    void wnd::show() const
    {
        if (!s_window)
        {
            std::cerr << "SDL window not created!" << std::endl;
            return;
        }

        // SDL doesn't have a real 'show' after creation,
        // so we just run the loop here

        bool running = true;
        SDL_Event event;
        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN)
                    running = false;
            }

            SDL_Delay(16);
        }

        SDL_DestroyWindow(s_window);
        SDL_Quit();
    }
}
