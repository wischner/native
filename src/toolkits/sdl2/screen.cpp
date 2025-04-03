#include <native.h>
#include <SDL2/SDL.h>
#include <iostream>

namespace native
{

    extern std::vector<screen> screens;

    void screen::detect()
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL2: Failed to initialize video subsystem: "
                      << SDL_GetError() << std::endl;
            return;
        }

        screens.clear();
        int display_count = SDL_GetNumVideoDisplays();
        if (display_count < 1)
        {
            std::cerr << "SDL2: No displays found: " << SDL_GetError() << std::endl;
            return;
        }

        for (int i = 0; i < display_count; ++i)
        {
            SDL_Rect bounds;
            SDL_Rect usable;
            if (SDL_GetDisplayBounds(i, &bounds) != 0 ||
                SDL_GetDisplayUsableBounds(i, &usable) != 0)
            {
                std::cerr << "SDL2: Failed to get display bounds: "
                          << SDL_GetError() << std::endl;
                continue;
            }

            rect screen_bounds(bounds.x, bounds.y, bounds.w, bounds.h);
            rect work_area(usable.x, usable.y, usable.w, usable.h);

            bool is_primary = (i == 0); // SDL doesn't give primary explicitly

            screens.emplace_back(i, screen_bounds, work_area, is_primary);
        }
    }

} // namespace native
