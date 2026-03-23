#import <AppKit/AppKit.h>

#include <native.h>

#include "globals.h"

namespace native
{

const std::vector<screen> &screen::detect()
{
    gnustep::ensure_app_initialized();

    _screens.clear();

    NSScreen *primary = [NSScreen mainScreen];
    NSArray *all_screens = [NSScreen screens];

    for (NSInteger i = 0; i < [all_screens count]; ++i)
    {
        NSScreen *s = [all_screens objectAtIndex:i];
        NSRect f = [s frame];
        NSRect v = [s visibleFrame];

        rect bounds(static_cast<coord>(f.origin.x),
                    static_cast<coord>(f.origin.y),
                    static_cast<dim>(f.size.width),
                    static_cast<dim>(f.size.height));
        rect work(static_cast<coord>(v.origin.x),
                  static_cast<coord>(v.origin.y),
                  static_cast<dim>(v.size.width),
                  static_cast<dim>(v.size.height));

        _screens.emplace_back(static_cast<int>(i), bounds, work, s == primary);
    }

    return _screens;
}

} // namespace native
