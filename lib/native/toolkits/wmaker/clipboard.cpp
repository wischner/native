//
// Implements the X11 CLIPBOARD selection through WINGs selection
// handlers.
// UTF-8 text and lossless PNG image representations are supported.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <X11/Xatom.h>
#include <WINGs/WINGs.h>
#include <WINGs/WUtil.h>

#include <native/app.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    native::detail::clipboard_payload owned_payload;
    WMView *selection_view = nullptr;

    Atom atom(const char *name) {
        return XInternAtom(linux::wmaker::display, name, False);
    }

    WMView *clipboard_view() {
        native::app_wnd *main = native::app::main_wnd();
        WMWidget *widget = main
                               ? linux::wmaker::wnd_bindings
                                     .handle_from_object(main)
                               : nullptr;
        if (!widget || WMWidgetXID(widget) == None) {
            throw std::runtime_error(
                "Window Maker/WINGs: clipboard needs a realized "
                "window.");
        }
        return WMWidgetView(widget);
    }

    bool ascii_text(const std::string &text) {
        return std::all_of(text.begin(), text.end(), [](char value) {
            return static_cast<unsigned char>(value) < 0x80;
        });
    }

    WMData *selection_data(const void *bytes,
                           std::size_t size,
                           unsigned int format) {
        if (size > UINT_MAX)
            return nullptr;
        WMData *data = WMCreateDataWithBytes(
            bytes, static_cast<unsigned int>(size));
        if (data)
            WMSetDataFormat(data, format);
        return data;
    }

    WMData *convert_selection(WMView *,
                              Atom,
                              Atom requested,
                              void *,
                              Atom *type) {
        const Atom targets = atom("TARGETS");
        const Atom utf8 = atom("UTF8_STRING");
        const Atom png = atom("image/png");
        if (requested == targets) {
            std::vector<Atom> available{targets};
            if (owned_payload.has_text) {
                available.push_back(utf8);
                if (ascii_text(owned_payload.text))
                    available.push_back(XA_STRING);
            }
            if (owned_payload.has_image)
                available.push_back(png);
            *type = XA_ATOM;
            return selection_data(
                available.data(),
                available.size() * sizeof(Atom),
                32);
        }
        if ((requested == utf8 || requested == XA_STRING) &&
            owned_payload.has_text &&
            (requested != XA_STRING ||
             ascii_text(owned_payload.text))) {
            *type = requested;
            return selection_data(owned_payload.text.data(),
                                  owned_payload.text.size(),
                                  8);
        }
        if (requested == png && owned_payload.has_image) {
            *type = png;
            return selection_data(owned_payload.image.data(),
                                  owned_payload.image.size(),
                                  8);
        }
        return nullptr;
    }

    void selection_lost(WMView *, Atom, void *) {
        selection_view = nullptr;
        owned_payload = {};
    }

    WMSelectionProcs selection_procs = {
        convert_selection, selection_lost, nullptr};

    struct request_result
    {
        bool pending = true;
        bool available = false;
        std::vector<std::uint8_t> bytes;
    };

    void received_selection(WMView *,
                            Atom,
                            Atom,
                            Time,
                            void *client_data,
                            WMData *data) {
        auto *result = static_cast<request_result *>(client_data);
        if (!result)
            return;
        if (data) {
            const auto *first = static_cast<const std::uint8_t *>(
                WMDataBytes(data));
            const std::size_t length = WMGetDataLength(data);
            if (first && length)
                result->bytes.assign(first, first + length);
            result->available = true;
        }
        result->pending = false;
    }

    request_result request(WMView *view, Atom target) {
        request_result result;
        if (!WMRequestSelection(view,
                                atom("CLIPBOARD"),
                                target,
                                CurrentTime,
                                received_selection,
                                &result)) {
            result.pending = false;
            return result;
        }
        while (result.pending) {
            XEvent event = {};
            WMNextEvent(linux::wmaker::display, &event);
            WMHandleEvent(&event);
            linux::wmaker::dispatch_deferred();
        }
        return result;
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        WMView *view = clipboard_view();
        if (selection_view == view &&
            XGetSelectionOwner(linux::wmaker::display,
                               atom("CLIPBOARD")) ==
                WMViewXID(view)) {
            return owned_payload;
        }

        clipboard_payload payload;
        request_result text = request(view, atom("UTF8_STRING"));
        bool latin1 = false;
        if (!text.available) {
            text = request(view, XA_STRING);
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
        request_result image = request(view, atom("image/png"));
        if (image.available && !image.bytes.empty()) {
            payload.image = std::move(image.bytes);
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        WMView *view = clipboard_view();
        const clipboard_payload previous = owned_payload;
        WMView *previous_view = selection_view;
        if (selection_view) {
            WMDeleteSelectionHandler(selection_view,
                                     atom("CLIPBOARD"),
                                     CurrentTime);
        }
        owned_payload = payload;
        if (!WMCreateSelectionHandler(view,
                                      atom("CLIPBOARD"),
                                      CurrentTime,
                                      &selection_procs,
                                      nullptr)) {
            owned_payload = previous;
            selection_view = previous_view;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to own clipboard.");
        }
        selection_view = view;
    }
} // namespace native::detail
