#import <AppKit/AppKit.h>

#include <native.h>

#include "globals.h"

namespace native
{

int app::main_loop()
{
    gnustep::ensure_app_initialized();
    if (!gnustep::global_app)
        return 1;

    gnustep::exit_requested = false;
    [gnustep::global_app activateIgnoringOtherApps:YES];
    [gnustep::global_app run];

    gnustep::wnd_bindings.clear();
    gnustep::view_bindings.clear();
    gnustep::wnd_gpx_bindings.clear();
    gnustep::delegate_bindings.clear();

    return 0;
}

} // namespace native
