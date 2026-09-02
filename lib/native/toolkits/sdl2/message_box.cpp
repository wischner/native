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
#include "../../message_box_common.h"

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);
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
        const int count = detail::message_box_button_count(buttons);
        for (int index = 0; index < count; ++index) {
            data[index].flags = index == 0
                ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : 0;
            data[index].buttonid = index;
            data[index].text =
                detail::message_box_button_label(buttons, index);
        }
        SDL_Window *window = linux::sdl2::wnd_bindings
                                 .handle_from_object(&owner);
        SDL_MessageBoxData box{flags, window, title.c_str(),
                               message.c_str(), count, data, nullptr};
        int index = -1;
        if (SDL_ShowMessageBox(&box, &index) != 0)
            return message_box_result::none;
        return index >= 0 && index < count
            ? detail::message_box_result_for_button(buttons, index)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native
