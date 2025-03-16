//
// native_start.cpp
// 
// Native start up adapter functions.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 15.05.2021   tstih
// 
#include "nice.hpp"

//{{BEGIN.CRT}}
extern void program();

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    // Store cmd line arguments.
    nice::app::argc = __argc;
    nice::app::argv = __argv;

    // Store application instance.
    nice::app::instance(hInstance);

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // And return return code;
    return nice::app::ret_code;
}
//{{END.CRT}}