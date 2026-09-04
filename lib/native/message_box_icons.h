//
// Provides decoded message-box icon assets to library-painted backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/graphics.h>
#include <native/message_box.h>

namespace native::detail
{
    // Return the cached PNG image for one semantic message-box icon.
    const img &message_box_icon_image(message_box_icon icon);
} // namespace native::detail
