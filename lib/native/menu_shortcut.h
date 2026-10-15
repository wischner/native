//
// Declares portable menu shortcut notation and mnemonic decoration.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <string>
namespace native::detail
{
    struct menu_shortcut
    {
        bool control = false;
        bool alt = false;
        bool shift = false;
        bool command = false;
        std::string key;
    };

    // Escape a label and mark its mnemonic for native menus.
    std::string decorate_menu_mnemonic(const std::string &label,
                                       std::size_t index);
    // Parse the modifiers and key in a portable shortcut label.
    menu_shortcut parse_menu_shortcut(std::string value);
} // namespace native::detail
