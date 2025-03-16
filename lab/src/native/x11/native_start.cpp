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

int main(int argc, char* argv[]) {
    // X Windows initialization code.
    nice::app_instance inst;
    inst.display=::XOpenDisplay(NULL);
    nice::app::instance(inst);

    // Copy cmd line arguments.
    nice::app::argc = argc;
    nice::app::argv = argv;

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // Close display.
    ::XCloseDisplay(inst.display);

    // And return return code;
    return nice::app::ret_code;
}
//{{END.CRT}}