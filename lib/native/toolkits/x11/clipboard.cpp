//
// Implements X11 CLIPBOARD selection ownership and snapshots for UTF-8
// text and PNG image representations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <X11/Intrinsic.h>
#include <X11/Xatom.h>

#include <native/app.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    native::detail::clipboard_payload owned_payload;
    Widget selection_owner = nullptr;

    Atom atom(const char *name) {
        return XInternAtom(linux::x11::cached_display, name, False);
    }

    Widget clipboard_widget() {
        native::app_wnd *main = native::app::main_wnd();
        Widget widget = main
                            ? linux::x11::main_wnd_bindings
                                  .handle_from_object(main)
                            : nullptr;
        if (!widget || !XtIsRealized(widget)) {
            throw std::runtime_error(
                "X11/Athena: Clipboard requires a realized window.");
        }
        return widget;
    }

    bool ascii_text(const std::string &text) {
        return std::all_of(text.begin(), text.end(), [](char value) {
            return static_cast<unsigned char>(value) < 0x80;
        });
    }

    Boolean convert_selection(Widget,
                              Atom *,
                              Atom *requested,
                              Atom *type,
                              XtPointer *value,
                              unsigned long *length,
                              int *format) {
        const Atom targets = atom("TARGETS");
        const Atom utf8 = atom("UTF8_STRING");
        const Atom png = atom("image/png");
        if (*requested == targets) {
            std::vector<Atom> available{targets};
            if (owned_payload.has_text) {
                available.push_back(utf8);
                if (ascii_text(owned_payload.text))
                    available.push_back(XA_STRING);
            }
            if (owned_payload.has_image)
                available.push_back(png);
            const std::size_t bytes = available.size() * sizeof(Atom);
            *value = XtMalloc(bytes);
            if (!*value)
                return False;
            std::memcpy(*value, available.data(), bytes);
            *type = XA_ATOM;
            *format = 32;
            *length = available.size();
            return True;
        }

        const void *source = nullptr;
        std::size_t count = 0;
        if ((*requested == utf8 || *requested == XA_STRING) &&
            owned_payload.has_text &&
            (*requested != XA_STRING || ascii_text(owned_payload.text))) {
            source = owned_payload.text.data();
            count = owned_payload.text.size();
            *type = *requested;
        } else if (*requested == png && owned_payload.has_image) {
            source = owned_payload.image.data();
            count = owned_payload.image.size();
            *type = png;
        } else {
            return False;
        }

        *value = XtMalloc(std::max<std::size_t>(count, 1));
        if (!*value)
            return False;
        if (count != 0)
            std::memcpy(*value, source, count);
        *format = 8;
        *length = count;
        return True;
    }

    void lose_selection(Widget, Atom *) {
        selection_owner = nullptr;
        owned_payload = {};
    }

    struct selection_result
    {
        bool pending = true;
        bool available = false;
        std::vector<std::uint8_t> bytes;
    };

    void received_selection(Widget,
                            XtPointer client_data,
                            Atom *,
                            Atom *type,
                            XtPointer value,
                            unsigned long *length,
                            int *format) {
        auto *result = static_cast<selection_result *>(client_data);
        if (result && type && *type != None && length && format &&
            *format == 8 && (*length == 0 || value)) {
            if (*length != 0) {
                const auto *first =
                    static_cast<const std::uint8_t *>(value);
                result->bytes.assign(first, first + *length);
            }
            result->available = true;
        }
        if (value)
            XtFree(static_cast<char *>(value));
        if (result)
            result->pending = false;
    }

    selection_result request(Widget widget, Atom target) {
        selection_result result;
        XtGetSelectionValue(widget,
                            atom("CLIPBOARD"),
                            target,
                            received_selection,
                            &result,
                            CurrentTime);
        while (result.pending) {
            XtAppProcessEvent(linux::x11::app_instance, XtIMAll);
        }
        return result;
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        Widget widget = clipboard_widget();
        if (selection_owner &&
            XGetSelectionOwner(XtDisplay(widget), atom("CLIPBOARD")) ==
                XtWindow(selection_owner)) {
            return owned_payload;
        }

        clipboard_payload payload;
        selection_result text = request(widget, atom("UTF8_STRING"));
        bool latin1 = false;
        if (!text.available) {
            text = request(widget, XA_STRING);
            latin1 = text.available;
        }
        if (text.available) {
            payload.text = latin1
                               ? latin1_to_utf8(text.bytes.data(),
                                                text.bytes.size())
                               : std::string(
                                     text.bytes.begin(),
                                     text.bytes.end());
            payload.has_text = true;
        }

        selection_result image = request(widget, atom("image/png"));
        if (image.available && !image.bytes.empty()) {
            payload.image = std::move(image.bytes);
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        Widget widget = clipboard_widget();
        owned_payload = payload;
        if (!XtOwnSelection(widget,
                            atom("CLIPBOARD"),
                            CurrentTime,
                            convert_selection,
                            lose_selection,
                            nullptr)) {
            owned_payload = {};
            throw std::runtime_error(
                "X11/Athena: Unable to own the clipboard selection.");
        }
        selection_owner = widget;
    }
} // namespace native::detail
