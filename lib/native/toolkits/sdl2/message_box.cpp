//
// Implements SDL standard message dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        Uint32 flags = 0;
        switch (icon) {
        case message_box_icon::information:
            flags = SDL_MESSAGEBOX_INFORMATION; break;
        case message_box_icon::warning:
        case message_box_icon::question:
            flags = SDL_MESSAGEBOX_WARNING; break;
        case message_box_icon::error:
            flags = SDL_MESSAGEBOX_ERROR; break;
        default: break;
        }
        SDL_MessageBoxButtonData data[3]{};
        int count = 0;
        auto add = [&](const char *label, int id, bool primary = false) {
            data[count].flags = primary
                ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : 0;
            data[count].buttonid = id;
            data[count].text = label;
            ++count;
        };
        switch (buttons) {
        case message_box_buttons::ok:
            add("OK", 1, true); break;
        case message_box_buttons::ok_cancel:
            add("OK", 1, true); add("Cancel", 2); break;
        case message_box_buttons::yes_no:
            add("Yes", 3, true); add("No", 4); break;
        case message_box_buttons::yes_no_cancel:
            add("Yes", 3, true); add("No", 4); add("Cancel", 2); break;
        }
        SDL_Window *window = linux::sdl2::wnd_bindings
                                 .handle_from_object(&owner);
        SDL_MessageBoxData box{flags, window, title.c_str(),
                               message.c_str(), count, data, nullptr};
        int id = -1;
        if (SDL_ShowMessageBox(&box, &id) != 0)
            return message_box_result::none;
        switch (id) {
        case 1: return message_box_result::ok;
        case 2: return message_box_result::cancel;
        case 3: return message_box_result::yes;
        case 4: return message_box_result::no;
        default: return message_box_result::none;
        }
    }
} // namespace native
