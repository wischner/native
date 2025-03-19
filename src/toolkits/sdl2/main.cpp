#include <SDL2/SDL.h>

// This is the user-provided entry point function
extern int program(int argc, char **argv);

int main(int argc, char **argv) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("SDL2 App", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    // Create a renderer (optional, for drawing on the window)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Call the user-provided entry point (optional, if you want to initialize something before the loop)
    int result = program(argc, argv);

    // Event loop to keep the window open
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        // Handle events on the queue
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;  // Exit the loop when the window is closed
            }
        }

        // Optionally, render something here (e.g., clear the screen, etc.)
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    // Clean up and quit SDL2
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}
