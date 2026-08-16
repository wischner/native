//
// Implements SDL clipboard text transfer and preserves the PNG
// representation for SDL processes that have no native image API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <stdexcept>
#include <string>

#include <SDL2/SDL.h>

#include "clipboard_x11.h"

namespace
{
    std::vector<std::uint8_t> owned_image;
    bool owned_has_text = false;
    std::string image_text_marker;
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        if (linux::sdl2::x11_clipboard::available())
            return linux::sdl2::x11_clipboard::read();
        clipboard_payload payload;
        char *text = SDL_GetClipboardText();
        if (!text) {
            throw std::runtime_error(
                std::string("SDL2: Unable to read clipboard: ") +
                SDL_GetError());
        }
        const std::string current_text = text;
        SDL_free(text);
        if (SDL_HasClipboardText() == SDL_TRUE ||
            (owned_has_text && current_text == image_text_marker)) {
            payload.text = current_text;
            payload.has_text = true;
        }
        if (!owned_image.empty() &&
            (!payload.has_text || payload.text == image_text_marker)) {
            payload.image = owned_image;
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        if (linux::sdl2::x11_clipboard::available()) {
            linux::sdl2::x11_clipboard::write(payload);
            return;
        }
        const std::string text = payload.has_text ? payload.text : "";
        if (SDL_SetClipboardText(text.c_str()) != 0) {
            throw std::runtime_error(
                std::string("SDL2: Unable to write clipboard: ") +
                SDL_GetError());
        }
        owned_image = payload.has_image
                          ? payload.image
                          : std::vector<std::uint8_t>();
        owned_has_text = payload.has_text;
        image_text_marker = text;
    }
} // namespace native::detail
