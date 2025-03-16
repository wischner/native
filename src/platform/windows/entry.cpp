// src/platform/win32/entry.cpp
#ifdef _WIN32
#include <windows.h>

// Declare the user's main
extern "C" int main(int argc, char** argv);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc = __argc;
    char** argv = __argv;

    return main(argc, argv);
}
#endif