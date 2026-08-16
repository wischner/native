//
// Implements backend-neutral theme lifetime and default-state
// overloads. Appearance and native drawing remain in the selected
// backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/theme.h>

namespace native
{
    theme::theme(gpx &painter)
        : _g(painter) {}

    theme::~theme() = default;

    theme &theme::draw_button(const rect &bounds,
                              const std::string &text) {
        return draw_button(bounds, text, state{});
    }

    theme &theme::draw_menu_title(const rect &bounds,
                                  const std::string &text) {
        return draw_menu_title(bounds, text, state{});
    }

    theme &theme::draw_menu_item(const rect &bounds,
                                 const std::string &text) {
        return draw_menu_item(bounds, text, state{});
    }

    theme &theme::draw_list_item(const rect &bounds,
                                 const std::string &text) {
        return draw_list_item(bounds, text, state{});
    }

    theme &theme::draw_check(const rect &bounds,
                             const std::string &text) {
        return draw_check(bounds, text, state{});
    }

    theme &theme::draw_radio(const rect &bounds,
                             const std::string &text) {
        return draw_radio(bounds, text, state{});
    }

    theme &theme::draw_list(const rect &bounds,
                            const std::vector<std::string> &items,
                            int selected_index) {
        return draw_list(bounds, items, selected_index, state{});
    }
} // namespace native
