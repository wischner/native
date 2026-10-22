# Windows Wine Runtime Note

Earlier Wine runs required the MinGW compiler runtime beside the executable:
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll`.

The maintained runtime and F5 workflow uses the
[native Windows 11 VM](WINDOWS-VM-RUNTIME.md). Its preparation script copies
matching DLLs from `native-windows-winui-sdk:24.04`, stages the Windows App SDK
bootstrap DLL and verifies the installed x64 Windows App Runtime 1.8.
The actual WinUI NavigationView is qualified in that native desktop.

Wine in the Docker SDK runs only Microsoft's build-time C++/WinRT projection
generator. This is not evidence of WinUI application support under Wine.
