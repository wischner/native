#include <native.h>

#import <Cocoa/Cocoa.h>

#include "globals.h"

namespace native
{

    int app::main_loop()
    {
        if (!mac::global_app)
            mac::global_app = [NSApplication sharedApplication];

        [mac::global_app run];
        return 0;
    }

} // namespace native
