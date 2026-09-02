// Parses the compact portable menu shortcut notation used after a tab.

#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace native::detail
{
    inline std::string decorate_menu_mnemonic(
        const std::string &label,
        std::size_t index) {
        if (index == std::string::npos || index >= label.size())
            index = std::string::npos;
        std::string result;
        result.reserve(label.size() + 2);
        for (std::size_t current = 0; current < label.size(); ++current) {
            if (current == index)
                result.push_back('&');
            if (label[current] == '&')
                result.push_back('&');
            result.push_back(label[current]);
        }
        return result;
    }

    struct menu_shortcut
    {
        bool control = false;
        bool alt = false;
        bool shift = false;
        bool command = false;
        std::string key;
    };

    inline menu_shortcut parse_menu_shortcut(std::string value) {
        menu_shortcut result;
        std::size_t start = 0;
        while (start <= value.size()) {
            const std::size_t plus = value.find('+', start);
            std::string part = value.substr(
                start, plus == std::string::npos
                           ? std::string::npos : plus-start);
            std::string lower = part;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            if (lower == "ctrl" || lower == "control")
                result.control = true;
            else if (lower == "alt" || lower == "option")
                result.alt = true;
            else if (lower == "shift")
                result.shift = true;
            else if (lower == "cmd" || lower == "command")
                result.command = true;
            else if (!part.empty())
                result.key = std::move(part);
            if (plus == std::string::npos)
                break;
            start = plus + 1;
        }
        return result;
    }
} // namespace native::detail
