//
// Implements the macOS display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <native.h>
#include <native/screen.h>

namespace native
{
    const std::vector<screen> &screen::detect() {
        _screens.clear();

        NSArray *screens = [NSScreen screens];

        for (NSInteger index = 0; index < [screens count]; ++index) {
            NSScreen *current_screen = [screens objectAtIndex:index];
            NSRect frame = [current_screen frame];
            NSRect visible_frame = [current_screen visibleFrame];

            rect bounds(static_cast<coord>(frame.origin.x),
                        static_cast<coord>(frame.origin.y),
                        static_cast<dim>(frame.size.width),
                        static_cast<dim>(frame.size.height));
            rect work(static_cast<coord>(visible_frame.origin.x),
                      static_cast<coord>(visible_frame.origin.y),
                      static_cast<dim>(visible_frame.size.width),
                      static_cast<dim>(visible_frame.size.height));

            _screens.emplace_back(
                static_cast<int>(index), bounds, work, index == 0);
        }

        normalize();
        return _screens;
    }

} // namespace native
