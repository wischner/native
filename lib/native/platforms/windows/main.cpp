//
// Implements the Windows process-entry backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdlib> // for __argc, __argv, _environ

#include <windows.h>

#include <native.h>

// Initialize the public argument state for console-subsystem builds.
int main(int argc, char **argv) {
    native::app::argc = argc;
    native::app::argv = argv;
    native::app::envp = _environ;

    return program(argc, argv);
}

// Initialize the public argument state for GUI-subsystem builds.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    native::app::argc = __argc;
    native::app::argv = __argv;
    native::app::envp = _environ;

    return program(__argc, __argv);
}
