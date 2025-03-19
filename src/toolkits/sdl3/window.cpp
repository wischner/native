#include <SDL3/SDL.h>
#include <iostream>

// Global pointer to SDL window
static SDL_Window* window = nullptr;

bool sdl_window_init(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init VIDEO Error: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    std::cout << "SDL3 Window created successfully." << std::endl;
    return true;
}

void sdl_window_shutdown() {
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
    std::cout << "SDL3 shutdown completed." << std::endl;
}

SDL_Window* sdl_get_window() {
    return window;
}
