//
// Implements UTF-8 text and PNG image clipboard representations with
// the XView Selection package and the X11 CLIPBOARD selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <native.h>
#include <native/app.h>

#include <xview/sel_pkg.h>
#include <xview/xview.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    native::detail::clipboard_payload owned_payload;
    Selection_owner selection_owner = XV_NULL;

    Panel clipboard_panel() {
        native::app_wnd *main = native::app::main_wnd();
        auto *state = main
                          ? linux::openlook::window_state(main)
                          : nullptr;
        if (!state || !state->content) {
            throw std::runtime_error(
                "OpenLook/XView: clipboard requires a window.");
        }
        return state->content;
    }

    bool ascii_text(const std::string &text) {
        return std::all_of(text.begin(), text.end(), [](char value) {
            return static_cast<unsigned char>(value) < 0x80;
        });
    }

    void lose_selection(Selection_owner owner) {
        if (selection_owner == owner) {
            selection_owner = XV_NULL;
            owned_payload = {};
            xv_destroy_safe(owner);
        }
    }

    void add_item(Selection_owner owner,
                  const char *type,
                  const void *data,
                  std::size_t size) {
        Selection_item item = static_cast<Selection_item>(xv_create(
            owner,
            SELECTION_ITEM,
            SEL_TYPE_NAME,
            type,
            SEL_DATA,
            data,
            SEL_LENGTH,
            static_cast<unsigned long>(size),
            SEL_FORMAT,
            8,
            SEL_COPY,
            TRUE,
            nullptr));
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create clipboard item.");
        }
    }

    struct request_result
    {
        bool available = false;
        std::vector<std::uint8_t> bytes;
    };

    request_result request(Panel panel, const char *type) {
        request_result result;
        Selection_requestor requestor =
            static_cast<Selection_requestor>(xv_create(
                panel,
                SELECTION_REQUESTOR,
                SEL_RANK_NAME,
                "CLIPBOARD",
                SEL_TYPE_NAME,
                type,
                nullptr));
        if (!requestor)
            return result;

        unsigned long length = 0;
        int format = 0;
        const auto *data =
            reinterpret_cast<const std::uint8_t *>(xv_get(
                requestor, SEL_DATA, &length, &format));
        if (length != static_cast<unsigned long>(SEL_ERROR) &&
            format == 8 &&
            (length == 0 || data)) {
            if (length)
                result.bytes.assign(data, data + length);
            result.available = true;
        }
        xv_destroy_safe(requestor);
        return result;
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        Panel panel = clipboard_panel();
        if (selection_owner &&
            static_cast<bool>(xv_get(selection_owner, SEL_OWN))) {
            return owned_payload;
        }

        clipboard_payload payload;
        request_result text = request(panel, "UTF8_STRING");
        bool latin1 = false;
        if (!text.available) {
            text = request(panel, "STRING");
            latin1 = text.available;
        }
        if (text.available) {
            payload.text = latin1
                               ? latin1_to_utf8(text.bytes.data(),
                                                text.bytes.size())
                               : std::string(text.bytes.begin(),
                                             text.bytes.end());
            payload.has_text = true;
        }

        request_result image = request(panel, "image/png");
        if (image.available && !image.bytes.empty()) {
            payload.image = std::move(image.bytes);
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        Panel panel = clipboard_panel();
        if (selection_owner) {
            Selection_owner previous = selection_owner;
            selection_owner = XV_NULL;
            owned_payload = {};
            xv_set(previous, SEL_OWN, FALSE, nullptr);
            xv_destroy_safe(previous);
        }

        Selection_owner owner = static_cast<Selection_owner>(xv_create(
            panel,
            SELECTION_OWNER,
            SEL_RANK_NAME,
            "CLIPBOARD",
            SEL_LOSE_PROC,
            lose_selection,
            nullptr));
        if (!owner) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create clipboard owner.");
        }

        try {
            if (payload.has_text) {
                add_item(owner,
                         "UTF8_STRING",
                         payload.text.data(),
                         payload.text.size());
                if (ascii_text(payload.text)) {
                    add_item(owner,
                             "STRING",
                             payload.text.data(),
                             payload.text.size());
                }
            }
            if (payload.has_image) {
                add_item(owner,
                         "image/png",
                         payload.image.data(),
                         payload.image.size());
            }
            owned_payload = payload;
            if (xv_set(owner, SEL_OWN, TRUE, nullptr) != XV_OK) {
                throw std::runtime_error(
                    "OpenLook/XView: unable to own clipboard.");
            }
            selection_owner = owner;
        } catch (...) {
            owned_payload = {};
            xv_destroy_safe(owner);
            throw;
        }
    }
} // namespace native::detail
