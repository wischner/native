#include <native.h>
#include <windows.h>

namespace native {

int app::main_loop()
{
    MSG msg;
    BOOL ret;

    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (ret == -1)
        {
            // Handle error if needed
            return -1;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

} // namespace native
