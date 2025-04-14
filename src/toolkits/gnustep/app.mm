#import <AppKit/AppKit.h>
#import <GNUstepBase/GSObjCRuntime.h>
#include <native.h>
#include <iostream>
#include "globals.h"

namespace native
{

int app::main_loop(int argc, char** argv, char** envp)
{
    // Correct call to GNUstep runtime setup
    GSInitializeProcess(argc, argv, envp);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    std::cout << "GNUstep: Creating NSApplication instance...\n";

    gnustep::global_app = [NSApplication sharedApplication];

    // Ignore this warning if it doesn't support setActivationPolicy on GNUstep
    //[gnustep::global_app setActivationPolicy:NSApplicationActivationPolicyRegular];

    std::cout << "GNUstep: Running application...\n";
    [gnustep::global_app run];

    [pool release];

    std::cout << "GNUstep: Application exited.\n";
    return 0;
}

} // namespace native