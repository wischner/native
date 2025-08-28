#import <Cocoa/Cocoa.h>
#include "globals.h"
#include <native.h>
#include <bindings.h>
#include "NativeWindow.h"

using namespace native;

@interface NativeWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation NativeWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    [mac::global_app terminate:nil];
    return YES;
}
@end

namespace mac {

NativeWindow::NativeWindow(app_wnd* owner, const char* title, int x, int y, int w, int h)
{
    NSRect frame = NSMakeRect(x, y, w, h);
    auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];

    NSString *nsTitle = [NSString stringWithUTF8String:title];
    [win setTitle:nsTitle];
    [win makeKeyAndOrderFront:nil];

    auto delegate = [[NativeWindowDelegate alloc] init];
    [win setDelegate:delegate];

    mac::wnd_bindings.register_pair(win, owner);
}

} // namespace mac
