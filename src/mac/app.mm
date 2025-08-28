#include <native.h>
#include <iostream>
#include <ApplicationServices/ApplicationServices.h>

namespace native
{

    int app::main_loop()
    {
        // macOS main loop is usually handled by NSApplication,
        // but for now, we’ll simulate it using CFRunLoop.
        std::cout << "macOS: Entering main loop.\n";

        // Run the Core Foundation run loop
        CFRunLoopRun();

        std::cout << "macOS: Exiting main loop.\n";
        return 0;
    }

} // namespace native
