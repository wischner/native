//
// Decodes embedded Font Awesome Free tiles for Vision navigation. The
// artwork and its derivatives retain their separate CC BY 4.0 license.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "navigation_icons.h"

namespace
{
#include "assets/navigation/icons.inc"

    template<std::size_t length>
    std::shared_ptr<const native::img> decode(
        const unsigned char (&bytes)[length]) {
        return std::shared_ptr<const native::img>(
            new native::img(native::img::decode(bytes, length)));
    }
}

namespace vision
{
    std::shared_ptr<const native::img> navigation_image(navigation_icon icon) {
        switch (icon) {
        case navigation_icon::settings: return decode(gear_png);
        case navigation_icon::calendar: return decode(calendar_days_png);
        case navigation_icon::clock: return decode(clock_png);
        }
        return {};
    }
}
