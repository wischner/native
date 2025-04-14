#import <AppKit/AppKit.h> // includes NSScreen
#include <native.h>

namespace native {

const std::vector<screen>& screen::detect()
{
    _screens.clear();

    NSScreen *primary = [NSScreen mainScreen];
    NSArray *screens = [NSScreen screens];

    for (NSInteger i = 0; i < [screens count]; ++i)
    {
        NSScreen *s = [screens objectAtIndex:i];
        NSRect f = [s frame];
        NSRect v = [s visibleFrame];

        rect bounds(f.origin.x, f.origin.y, f.size.width, f.size.height);
        rect work(v.origin.x, v.origin.y, v.size.width, v.size.height);

        bool is_primary = (s == primary);
        _screens.emplace_back((int)i, bounds, work, is_primary);
    }

    return _screens;
}

} // namespace native
