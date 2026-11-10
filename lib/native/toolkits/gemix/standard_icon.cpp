//
// Shares Ted's canonical 16x16 command bitmaps across GEM applications.
// No resampling or alternate drawings; unknown commands have no stock icon.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Tomaz Stih
//
#include <native/standard_icon.h>
#include <native/graphics.h>
#include "../../third_party/ted/toolbar_icons.h"
#include <array>
namespace native {
    std::shared_ptr<const img> standard_icon(standard_icon_id id) {
        constexpr std::array ids{
            standard_icon_id::file_new, standard_icon_id::file_open,
            standard_icon_id::file_save, standard_icon_id::file_save_as,
            standard_icon_id::undo, standard_icon_id::cut,
            standard_icon_id::copy, standard_icon_id::paste};
        static const auto icons = [] {
            std::array<std::shared_ptr<const img>, 8> result;
            for (std::size_t i = 0; i < result.size(); ++i) {
                auto image = std::make_shared<img>(16, 16);
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x)
                        image->pixels()[y * 16 + x] = rgba(0, 0, 0,
                            detail::ted_toolbar_icons[i][y] & (0x8000 >> x)
                                ? 255 : 0);
                result[i] = std::move(image);
            }
            return result;
        }();
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == id) return icons[i];
        return {};
    }
}
