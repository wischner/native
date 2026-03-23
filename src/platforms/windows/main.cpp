#include <native.h>
#include <windows.h>
#include <cstdlib> // for __argc, __argv, _environ

int main(int argc, char **argv)
{
    native::app::argc = argc;
    native::app::argv = argv;
    native::app::envp = _environ;

    return program(argc, argv);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // Just directly assign the global values
    native::app::argc = __argc;
    native::app::argv = __argv;
    native::app::envp = _environ;

    return program(__argc, __argv);
}
