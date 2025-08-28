#import <Cocoa/Cocoa.h>
#include "globals.h"

__attribute__((constructor))
static void init_mac_app()
{
    mac::global_app = [NSApplication sharedApplication];
}
