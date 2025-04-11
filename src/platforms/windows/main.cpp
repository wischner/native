#include <native.h>
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // Convert command line into argc/argv
    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
    {
        char buffer[512];
        WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, buffer, sizeof(buffer), nullptr, nullptr);
        args.emplace_back(buffer);
    }

    LocalFree(argv_w);

    std::vector<char*> argv_c;
    for (auto& s : args)
        argv_c.push_back(s.data());
    argv_c.push_back(nullptr); // null-terminated

    return program(argc, argv_c.data());
}
