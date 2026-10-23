//
// Implements owner-centered GEM messages using native AES form buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/message_box.h>
#include "dialog_form.h"
#include "globals.h"
#include "../../message_box_common.h"
#include <algorithm>
#include <string>
#include <vector>

namespace native
{
    message_box_result message_box::show(app_wnd &owner,
                                         const std::string &message,
                                         const std::string &title,
                                         message_box_buttons buttons,
                                         message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        const auto desktop = linux::gemix::desktop_rect();
        const int width = std::min(600, int(desktop.d.w) - 24);
        const int line_height =
            std::max(22, int(linux::gemix::runtime.char_h) + 6);
        const int columns = std::max(
            12,
            (width - 40) /
                std::max(1, int(linux::gemix::runtime.char_w)));
        std::vector<std::string> lines;
        std::string remaining = message;
        while (!remaining.empty()) {
            auto length =
                std::min(remaining.size(), std::size_t(columns));
            const auto newline = remaining.find('\n');
            if (newline < length)
                length = newline;
            else if (length < remaining.size()) {
                const auto space = remaining.rfind(' ', length);
                if (space != std::string::npos && space > 0)
                    length = space;
            }
            lines.push_back(remaining.substr(0, length));
            remaining.erase(0, length);
            if (!remaining.empty() &&
                (remaining.front() == '\n' || remaining.front() == ' '))
                remaining.erase(0, 1);
        }
        if (lines.empty())
            lines.push_back("");
        const int maximum =
            std::max(1, (int(desktop.d.h) - 150) / line_height);
        const int visible = std::min(int(lines.size()), maximum);
        linux::gemix::dialog_form form(
            owner, width, 100 + visible * line_height);
        form.label(title, 20, 10, width - 40, line_height);
        const char *kind =
            icon == message_box_icon::error         ? "Error"
            : icon == message_box_icon::warning     ? "Warning"
            : icon == message_box_icon::question    ? "Question"
            : icon == message_box_icon::information ? "Information"
                                                    : "";
        form.label(kind, 20, 10 + line_height, width - 40, line_height);
        std::vector<int> labels;
        for (int row = 0; row < visible; ++row)
            labels.push_back(form.label(lines[row],
                                        20,
                                        14 + (row + 2) * line_height,
                                        width - 40,
                                        line_height));
        const int count = detail::message_box_button_count(buttons);
        std::vector<int> commands;
        const int button_y = 65 + visible * line_height;
        for (int index = 0; index < count; ++index)
            commands.push_back(form.button(
                detail::message_box_button_label(buttons, index),
                (width - count * 100 + 12) / 2 + index * 100,
                button_y,
                88,
                26,
                index == 0));
        int previous = -1, next = -1;
        if (visible < int(lines.size())) {
            previous = form.button("<", 12, button_y, 28, 26);
            next = form.button(">", width - 40, button_y, 28, 26);
        }
        int first = 0;
        for (;;) {
            if (previous >= 0) {
                form.set_enabled(previous, first > 0);
                form.set_enabled(next,
                                 first + visible < int(lines.size()));
                for (int row = 0; row < visible; ++row)
                    form.set_label(labels[row], lines[first + row]);
            }
            const int result = form.run();
            if (result == previous && previous >= 0) {
                first = std::max(0, first - visible);
                continue;
            }
            if (result == next && next >= 0) {
                first = std::min(int(lines.size()) - visible,
                                 first + visible);
                continue;
            }
            const auto selected =
                std::find(commands.begin(), commands.end(), result);
            return selected == commands.end()
                       ? detail::message_box_dismissed_result(buttons)
                       : detail::message_box_result_for_button(
                             buttons, int(selected - commands.begin()));
        }
    }
} // namespace native
