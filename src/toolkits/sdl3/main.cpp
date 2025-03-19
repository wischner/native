#include <SDL3/SDL.h>
#include <iostream>

// User-defined program()
extern int program(int argc, char **argv);

int main(int argc, char **argv) {
    std::cout << "Starting SDL3 toolkit main()" << std::endl;

    int count = SDL_GetNumVideoDrivers();
    std::cout << "Available video drivers: " << count << std::endl;
    for (int i = 0; i < count; ++i) {
        const char* driver = SDL_GetVideoDriver(i);
        std::cout << "Video driver [" << i << "]: " << driver << std::endl;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL3 Initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Calling user program()" << std::endl;
    int result = program(argc, argv);

    std::cout << "program() returned " << result << std::endl;

    SDL_Quit();

    return result;
}
