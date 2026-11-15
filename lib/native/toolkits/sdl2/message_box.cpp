//
// Implements SDL message dialogs with the library theme and event routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <native/app_wnd.h>
#include <native/button.h>
#include <native/font.h>
#include <native/message_box.h>
#include <native/modal_wnd.h>
#include <native/text_edit.h>
#include <native/screen.h>
#include "../../input_state.h"

#include "../../message_box_common.h"
#include "../../message_box_icons.h"
#include "globals.h"
#include "scale.h"
#include "window_position.h"

namespace
{
    // Preserve explicit paragraphs and wrap at UTF-8 boundaries. The read-only
    // editor supplies scrolling when a message exceeds the display height.
    std::string wrap_message(const std::string &message, const native::font_t &font,
                             int width) {
        std::string result, line;
        for (std::size_t at = 0; at < message.size();) {
            std::size_t end = at + 1;
            while (end < message.size() &&
                   (static_cast<unsigned char>(message[end]) & 0xc0) == 0x80) ++end;
            const std::string character = message.substr(at, end - at);
            at = end;
            if (character == "\n") {
                result += line + "\n"; line.clear(); continue;
            }
            if (!line.empty() && font.measure_text(line + character).width > width) {
                const auto space = line.rfind(' ');
                if (space != std::string::npos) {
                    result += line.substr(0, space) + "\n";
                    line.erase(0, space + 1);
                } else {
                    result += line + "\n"; line.clear();
                }
            }
            line += character;
        }
        return result + line;
    }

    // Process events for a synchronous library-owned SDL modal window.
    void run_modal_loop(native::modal_wnd &dialog,
                        const std::function<void()> &accept,
                        const std::function<void()> &cancel) {
        bool repost_quit = false;
        while (dialog.get_created()) {
            SDL_Event event{};
            while (dialog.get_created() &&
                   linux::sdl2::poll_event(event)) {
                if (event.type == SDL_QUIT) {
                    cancel();
                    repost_quit = true;
                    break;
                }

                native::wnd *target =
                    event.window.windowID
                        ? linux::sdl2::wnd_bindings.object_from_handle(
                              SDL_GetWindowFromID(event.window.windowID))
                        : nullptr;
                if (target != &dialog)
                    continue;

                switch (event.type) {
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_TAB) {
                        auto *focused = native::detail::input_target(dialog);
                        if (!focused) focused = &dialog;
                        focused->on_native_tab({native::key_code::tab,
                            native::key_action::press, false,
                            (event.key.keysym.mod & KMOD_SHIFT)
                                ? native::key_modifiers::shift : native::key_modifiers::none});
                        dialog.invalidate();
                        break;
                    }
                    if ((event.key.keysym.sym == SDLK_RETURN ||
                         event.key.keysym.sym == SDLK_KP_ENTER ||
                         event.key.keysym.sym == SDLK_SPACE)) {
                        if (auto *button = dynamic_cast<native::button *>(
                                native::detail::input_target(dialog))) {
                            button->on_native_click();
                            break;
                        }
                    }
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        cancel();
                    } else if (event.key.keysym.sym == SDLK_RETURN ||
                               event.key.keysym.sym == SDLK_KP_ENTER) {
                        accept();
                    } else {
                        linux::sdl2::handle_text_edit_key(&dialog, event.key);
                    }
                    break;

                case SDL_MOUSEMOTION:
                    linux::sdl2::update_mouse_cursor(
                        &dialog,
                        native::point(event.motion.x, event.motion.y));
                    linux::sdl2::handle_button_motion(
                        &dialog, event.motion.x, event.motion.y);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button != SDL_BUTTON_LEFT)
                        break;
                    SDL_CaptureMouse(
                        event.type == SDL_MOUSEBUTTONDOWN
                            ? SDL_TRUE
                            : SDL_FALSE);
                    linux::sdl2::handle_button_mouse(
                        &dialog,
                        event.button.x,
                        event.button.y,
                        event.type == SDL_MOUSEBUTTONDOWN,
                        event.type == SDL_MOUSEBUTTONUP);
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        cancel();
                    } else if (event.window.event ==
                               SDL_WINDOWEVENT_EXPOSED) {
                        dialog.invalidate();
                    } else if (event.window.event ==
                               SDL_WINDOWEVENT_RESIZED) {
                        dialog.on_native_resize(native::size(
                            static_cast<native::dim>(
                                std::max(1, event.window.data1)),
                            static_cast<native::dim>(
                                std::max(1, event.window.data2))));
                    } else if (event.window.event ==
                               SDL_WINDOWEVENT_MOVED) {
                        dialog.on_native_move(native::point(
                            static_cast<native::coord>(event.window.data1),
                            static_cast<native::coord>(event.window.data2)));
                    }
                    break;

                default:
                    break;
                }
            }

            if (dialog.get_created())
                linux::sdl2::render_window_if_needed(&dialog);
            SDL_Delay(1);
        }

        SDL_CaptureMouse(SDL_FALSE);
        if (repost_quit) {
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
    }
} // namespace

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);

        const font_t &font = font_t::stock(font_role::control);
        const int measured = font.measure_text(message).width;
        const bool has_icon = icon != message_box_icon::none;
        const int width = std::clamp(
            measured + (has_icon ? 122 : 64), 380, 640);
        const int message_left = has_icon ? 88 : 24;
        const std::string wrapped = wrap_message(message, font, width - message_left - 40);
        const int lines = 1 + std::count(wrapped.begin(), wrapped.end(), '\n');
        const auto &screens = screen::detect();
        const auto *display = screens.empty() ? nullptr : &screens.front();
        int available_height = 600;
        if (display) {
            const auto area = display->work_area();
            const auto origin = linux::sdl2::constrain_window_position(nullptr, area.p, area.d);
            available_height = int(area.h()) - std::max(0, int(origin.y) - int(area.y1()));
        }
        const int height = std::min(available_height,
            std::max(170, lines * (font.measure_text("Ag").height + 2) + 100));
        const int count = detail::message_box_button_count(buttons);

        modal_wnd dialog(
            owner, title, 0, 0,
            static_cast<dim>(width), static_cast<dim>(height));
        dialog.center_to_parent();
        if (display) {
            const auto area = display->work_area();
            const auto bounds = dialog.get_bounds();
            dialog.set_position(point(
                std::clamp(int(bounds.x1()), int(area.x1()),
                    std::max(int(area.x1()), int(area.x2()) - width)),
                std::clamp(int(bounds.y1()), int(area.y1()),
                    std::max(int(area.y1()), int(area.y2()) - height))));
        }
        text_edit text(wrapped, text_edit_mode::multi_line);
        text.set_read_only(true);
        text.set_tab_order(1);

        message_box_result result =
            detail::message_box_dismissed_result(buttons);
        std::vector<std::unique_ptr<button>> controls;
        controls.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            controls.push_back(std::make_unique<button>(
                detail::message_box_button_label(buttons, index),
                0, 0, 92, 30));
            controls.back()->set_tab_order(static_cast<unsigned>(index + 2));
            controls.back()->on_click.connect([&, index] {
                result = detail::message_box_result_for_button(
                    buttons, index);
                if (dialog.get_created()) {
                    dialog.close(result == message_box_result::cancel
                                     ? dialog_result::cancelled
                                     : dialog_result::accepted);
                }
                return true;
            });
        }

        const auto layout_buttons = [&](size dimensions) {
            constexpr int button_width = 92;
            constexpr int button_height = 30;
            constexpr int gap = 12;
            const int total = count * button_width +
                              std::max(0, count - 1) * gap;
            int x = (static_cast<int>(dimensions.w) - total) / 2;
            const int y = std::max(
                0, static_cast<int>(dimensions.h) - button_height - 18);
            for (auto &control : controls) {
                control->set_bounds(rect(
                    static_cast<coord>(x), static_cast<coord>(y),
                    button_width, button_height));
                x += button_width + gap;
            }
            const int text_width = std::max(1, int(dimensions.w) - message_left - 24);
            text.set_bounds(rect(message_left, 22, text_width,
                                 std::max(1, y - 34)));
            text.set_text(wrap_message(message, font, std::max(1, text_width - 16)));
            return true;
        };

        dialog.on_wnd_resize.connect(layout_buttons);
        dialog.on_wnd_paint.connect([&](wnd_paint_event event) {
            if (has_icon) {
                event.g.draw_img(
                    detail::message_box_icon_image(icon),
                    point(26, 38));
            }
            return true;
        });

        dialog.create();
        layout_buttons(dialog.get_dimensions());
        text.set_parent(&dialog);
        text.create();
        text.show();
        for (auto &control : controls) {
            control->set_parent(&dialog);
            control->create();
            control->show();
        }
        dialog.show();
        if (!controls.empty()) controls.front()->focus();

        const auto accept = [&] {
            if (!controls.empty())
                controls.front()->on_native_click();
        };
        const auto cancel = [&] {
            result = detail::message_box_dismissed_result(buttons);
            if (dialog.get_created())
                dialog.close(dialog_result::cancelled);
        };
        run_modal_loop(dialog, accept, cancel);
        linux::sdl2::restore_window_focus(&owner);
        return result;
    }
} // namespace native
