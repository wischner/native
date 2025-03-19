#include <SDL3/SDL.h>
#include <iostream>

// Get the window from sdl_window.cpp
extern SDL_Window* sdl_get_window();

static SDL_Renderer* renderer = nullptr;

bool sdl_renderer_init() {
    SDL_Window* window = sdl_get_window();
    if (!window) {
        std::cerr << "Renderer init failed: window is null!" << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL3 Renderer created successfully." << std::endl;
    return true;
}

void sdl_renderer_render() {
    if (!renderer) return;

    SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255); // Cornflower Blue
    SDL_RenderClear(renderer);

    // TODO: Add more rendering here!

    SDL_RenderPresent(renderer);
}

void sdl_renderer_shutdown() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
}
