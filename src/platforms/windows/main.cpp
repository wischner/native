#include <native.h>
#include <windows.h>
#include <cstdlib> // for __argc, __argv, _environ

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // Just directly assign the global values
    native::app::argc = __argc;
    native::app::argv = __argv;
    native::app::envp = _environ;

    return program(__argc, __argv);
}
