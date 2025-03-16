//
// native_start.cpp
// 
// Native start up adapter functions.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
// 
#include "nice.hpp"

//{{BEGIN.CRT}}
extern void program();

int main(int argc, char* argv[]) {

    // Init SDL.
    if (::SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) {
        throw_ex(nice::nice_exception,"SDL could not initialize.");
    }

    // TODO: Initialize application.
    nice::app_instance inst=0;
    nice::app::instance(inst);

    // Copy cmd line arguments.
    nice::app::argc = argc;
    nice::app::argv = argv;

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // Exit SDL.
    ::SDL_Quit();

    // And return return code;
    return nice::app::ret_code;
}
//{{END.CRT}}