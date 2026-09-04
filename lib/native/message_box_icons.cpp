//
// Decodes the embedded message-box PNG assets on first use.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "message_box_icons.h"

#include <stdexcept>

#include "message_box_icon_data.h"

namespace native::detail
{
    const img &message_box_icon_image(message_box_icon icon) {
        using namespace message_box_icon_data;

        switch (icon) {
        case message_box_icon::information: {
            static const img image =
                img::decode(information_png, information_png_size);
            return image;
        }
        case message_box_icon::warning: {
            static const img image =
                img::decode(warning_png, warning_png_size);
            return image;
        }
        case message_box_icon::error: {
            static const img image =
                img::decode(error_png, error_png_size);
            return image;
        }
        case message_box_icon::question: {
            static const img image =
                img::decode(question_png, question_png_size);
            return image;
        }
        case message_box_icon::none:
            break;
        }
        throw std::invalid_argument(
            "message_box_icon_image: icon must not be none");
    }
} // namespace native::detail
