#import <AppKit/AppKit.h>
#include <native.h>
#include <iostream>
#include "globals.h"

extern "C" void GSInitializeProcess(int argc, char **argv, char **envp);

namespace native {

int app::main_loop()
{
    // Required for GNUstep apps — this must come before any NS classes are used
    GSInitializeProcess(argc, argv, envp);

    gnustep::global_app = [NSApplication sharedApplication];
    if (!gnustep::global_app)
    {
        std::cerr << "GNUstep: NSApplication is null\n";
        return 1;
    }

    // Some backends don't support setActivationPolicy
    if ([gnustep::global_app respondsToSelector:@selector(setActivationPolicy:)])
    {
        [gnustep::global_app setActivationPolicy:NSApplicationActivationPolicyRegular];
    }

    std::cout << "GNUstep: Entering main loop.\n";
    [gnustep::global_app run];
    std::cout << "GNUstep: Exiting main loop.\n";
    return 0;
}

} // namespace native
