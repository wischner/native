//
// Implements the SDL2 backend's foreign X11 clipboard service for
// Unicode text and lossless PNG image targets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "clipboard_x11.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <SDL2/SDL.h>

#include "../../text_util.h"

namespace
{
    constexpr std::size_t maximum_clipboard_bytes =
        512U * 1024U * 1024U;

    struct outgoing_transfer
    {
        Window requestor = None;
        Atom property = None;
        Atom type = None;
        std::vector<std::uint8_t> bytes;
        std::size_t offset = 0;
    };

    struct service_state
    {
        bool attempted = false;
        Display *display = nullptr;
        Window window = None;
        Atom clipboard = None;
        Atom targets = None;
        Atom utf8 = None;
        Atom png = None;
        Atom incr = None;
        Atom property = None;
        native::detail::clipboard_payload payload;
        bool owns = false;
        std::vector<outgoing_transfer> outgoing;
    };

    service_state state;

    void initialize() {
        if (state.attempted)
            return;
        const char *driver = SDL_GetCurrentVideoDriver();
        if (!driver)
            return;
        state.attempted = true;
        if (std::strcmp(driver, "x11") != 0)
            return;
        state.display = XOpenDisplay(nullptr);
        if (!state.display)
            return;
        state.window = XCreateSimpleWindow(
            state.display,
            DefaultRootWindow(state.display),
            0,
            0,
            1,
            1,
            0,
            0,
            0);
        state.clipboard =
            XInternAtom(state.display, "CLIPBOARD", False);
        state.targets =
            XInternAtom(state.display, "TARGETS", False);
        state.utf8 =
            XInternAtom(state.display, "UTF8_STRING", False);
        state.png =
            XInternAtom(state.display, "image/png", False);
        state.incr = XInternAtom(state.display, "INCR", False);
        state.property = XInternAtom(
            state.display, "NATIVE_SDL_CLIPBOARD", False);
        XSelectInput(
            state.display, state.window, PropertyChangeMask);
    }

    bool ascii_text(const std::string &text) {
        return std::all_of(text.begin(), text.end(), [](char value) {
            return static_cast<unsigned char>(value) < 0x80;
        });
    }

    std::size_t transfer_chunk_size() {
        const long request_units = XMaxRequestSize(state.display);
        if (request_units <= 128)
            return 16U * 1024U;
        return std::min<std::size_t>(
            static_cast<std::size_t>(request_units - 128) * 4,
            256U * 1024U);
    }

    void begin_incremental_transfer(
        const XSelectionRequestEvent &request,
        Atom property,
        Atom type,
        const std::uint8_t *bytes,
        std::size_t size) {
        unsigned long advertised = size;
        XChangeProperty(state.display,
                        request.requestor,
                        property,
                        state.incr,
                        32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char *>(
                            &advertised),
                        1);
        XSelectInput(
            state.display, request.requestor, PropertyChangeMask);
        outgoing_transfer transfer;
        transfer.requestor = request.requestor;
        transfer.property = property;
        transfer.type = type;
        transfer.bytes.assign(bytes, bytes + size);
        state.outgoing.push_back(std::move(transfer));
    }

    bool continue_incremental_transfer(
        const XPropertyEvent &property_event) {
        if (property_event.state != PropertyDelete)
            return false;
        for (std::size_t index = 0;
             index < state.outgoing.size();
             ++index) {
            outgoing_transfer &transfer = state.outgoing[index];
            if (transfer.requestor != property_event.window ||
                transfer.property != property_event.atom) {
                continue;
            }
            const std::size_t remaining =
                transfer.bytes.size() - transfer.offset;
            const std::size_t count =
                std::min(remaining, transfer_chunk_size());
            const std::uint8_t empty = 0;
            XChangeProperty(
                state.display,
                transfer.requestor,
                transfer.property,
                transfer.type,
                8,
                PropModeReplace,
                const_cast<unsigned char *>(
                    count == 0
                        ? &empty
                        : transfer.bytes.data() + transfer.offset),
                static_cast<int>(count));
            transfer.offset += count;
            if (count == 0) {
                state.outgoing.erase(state.outgoing.begin() + index);
            }
            XFlush(state.display);
            return true;
        }
        return false;
    }

    void send_selection(const XSelectionRequestEvent &request) {
        XEvent response{};
        response.xselection.type = SelectionNotify;
        response.xselection.display = request.display;
        response.xselection.requestor = request.requestor;
        response.xselection.selection = request.selection;
        response.xselection.target = request.target;
        response.xselection.time = request.time;
        response.xselection.property = None;
        const Atom property = request.property == None
                                  ? request.target
                                  : request.property;

        if (request.target == state.targets) {
            std::vector<Atom> formats{state.targets};
            if (state.payload.has_text) {
                formats.push_back(state.utf8);
                if (ascii_text(state.payload.text))
                    formats.push_back(XA_STRING);
            }
            if (state.payload.has_image)
                formats.push_back(state.png);
            XChangeProperty(state.display,
                            request.requestor,
                            property,
                            XA_ATOM,
                            32,
                            PropModeReplace,
                            reinterpret_cast<unsigned char *>(
                                formats.data()),
                            static_cast<int>(formats.size()));
            response.xselection.property = property;
        } else {
            const std::uint8_t *bytes = nullptr;
            std::size_t size = 0;
            Atom type = None;
            if ((request.target == state.utf8 ||
                 request.target == XA_STRING) &&
                state.payload.has_text &&
                (request.target != XA_STRING ||
                 ascii_text(state.payload.text))) {
                bytes = reinterpret_cast<const std::uint8_t *>(
                    state.payload.text.data());
                size = state.payload.text.size();
                type = request.target;
            } else if (request.target == state.png &&
                       state.payload.has_image) {
                bytes = state.payload.image.data();
                size = state.payload.image.size();
                type = state.png;
            }
            if (type != None) {
                const std::uint8_t empty = 0;
                if (size > transfer_chunk_size()) {
                    begin_incremental_transfer(
                        request, property, type, bytes, size);
                } else {
                    XChangeProperty(
                        state.display,
                        request.requestor,
                        property,
                        type,
                        8,
                        PropModeReplace,
                        const_cast<unsigned char *>(
                            size == 0 ? &empty : bytes),
                        static_cast<int>(size));
                }
                response.xselection.property = property;
            }
        }
        XSendEvent(state.display,
                   request.requestor,
                   False,
                   NoEventMask,
                   &response);
        XFlush(state.display);
    }

    void handle_event(const XEvent &event) {
        if (event.type == SelectionRequest)
            send_selection(event.xselectionrequest);
        else if (event.type == PropertyNotify)
            continue_incremental_transfer(event.xproperty);
        else if (event.type == SelectionClear) {
            state.owns = false;
            state.payload = {};
        }
    }

    struct requested_data
    {
        bool available = false;
        std::vector<std::uint8_t> bytes;
    };

    requested_data request(Atom target) {
        XDeleteProperty(state.display, state.window, state.property);
        XConvertSelection(state.display,
                          state.clipboard,
                          target,
                          state.property,
                          state.window,
                          CurrentTime);
        XFlush(state.display);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            while (XPending(state.display) != 0) {
                XEvent event{};
                XNextEvent(state.display, &event);
                if (event.type != SelectionNotify ||
                    event.xselection.requestor != state.window) {
                    handle_event(event);
                    continue;
                }
                requested_data result;
                if (event.xselection.property == None)
                    return result;
                Atom actual_type = None;
                int format = 0;
                unsigned long count = 0;
                unsigned long remaining = 0;
                unsigned char *probe = nullptr;
                XGetWindowProperty(state.display,
                                   state.window,
                                   state.property,
                                   0,
                                   0,
                                   False,
                                   AnyPropertyType,
                                   &actual_type,
                                   &format,
                                   &count,
                                   &remaining,
                                   &probe);
                if (probe)
                    XFree(probe);
                if (actual_type == state.incr && format == 32) {
                    XDeleteProperty(
                        state.display, state.window, state.property);
                    XFlush(state.display);
                    requested_data incremental;
                    while (std::chrono::steady_clock::now() <
                           deadline) {
                        while (XPending(state.display) != 0) {
                            XEvent chunk_event{};
                            XNextEvent(state.display, &chunk_event);
                            if (chunk_event.type != PropertyNotify ||
                                chunk_event.xproperty.window !=
                                    state.window ||
                                chunk_event.xproperty.atom !=
                                    state.property ||
                                chunk_event.xproperty.state !=
                                    PropertyNewValue) {
                                handle_event(chunk_event);
                                continue;
                            }
                            unsigned char *chunk = nullptr;
                            unsigned long chunk_count = 0;
                            unsigned long chunk_remaining = 0;
                            XGetWindowProperty(
                                state.display,
                                state.window,
                                state.property,
                                0,
                                static_cast<long>(
                                    transfer_chunk_size() / 4 + 1),
                                True,
                                AnyPropertyType,
                                &actual_type,
                                &format,
                                &chunk_count,
                                &chunk_remaining,
                                &chunk);
                            if (format != 8 || chunk_remaining != 0) {
                                if (chunk)
                                    XFree(chunk);
                                return {};
                            }
                            if (chunk_count == 0) {
                                if (chunk)
                                    XFree(chunk);
                                incremental.available = true;
                                return incremental;
                            }
                            if (!chunk ||
                                chunk_count > maximum_clipboard_bytes ||
                                incremental.bytes.size() >
                                maximum_clipboard_bytes -
                                    chunk_count) {
                                if (chunk)
                                    XFree(chunk);
                                throw std::runtime_error(
                                    "SDL2/X11: Clipboard item is too "
                                    "large.");
                            }
                            incremental.bytes.insert(
                                incremental.bytes.end(),
                                chunk,
                                chunk + chunk_count);
                            XFree(chunk);
                        }
                        SDL_Delay(1);
                    }
                    throw std::runtime_error(
                        "SDL2/X11: Incremental clipboard request "
                        "timed out.");
                }
                if (actual_type == None || format != 8 ||
                    remaining > maximum_clipboard_bytes)
                    return result;
                unsigned char *data = nullptr;
                XGetWindowProperty(
                    state.display,
                    state.window,
                    state.property,
                    0,
                    static_cast<long>((remaining + 3) / 4),
                    True,
                    AnyPropertyType,
                    &actual_type,
                    &format,
                    &count,
                    &remaining,
                    &data);
                if (data) {
                    result.bytes.assign(data, data + count);
                    XFree(data);
                }
                result.available = format == 8 && remaining == 0;
                return result;
            }
            SDL_Delay(1);
        }
        throw std::runtime_error(
            "SDL2/X11: Clipboard selection request timed out.");
    }
} // namespace

namespace linux::sdl2::x11_clipboard
{
    bool available() {
        initialize();
        return state.display && state.window != None;
    }

    native::detail::clipboard_payload read() {
        if (!available())
            return {};
        if (state.owns &&
            XGetSelectionOwner(state.display, state.clipboard) ==
                state.window) {
            return state.payload;
        }
        native::detail::clipboard_payload payload;
        requested_data text = request(state.utf8);
        bool latin1 = false;
        if (!text.available) {
            text = request(XA_STRING);
            latin1 = text.available;
        }
        if (text.available) {
            payload.text = latin1
                               ? native::detail::latin1_to_utf8(
                                     text.bytes.data(), text.bytes.size())
                               : std::string(
                                     text.bytes.begin(),
                                     text.bytes.end());
            payload.has_text = true;
        }
        requested_data image = request(state.png);
        if (image.available && !image.bytes.empty()) {
            payload.image = std::move(image.bytes);
            payload.has_image = true;
        }
        return payload;
    }

    void write(const native::detail::clipboard_payload &payload) {
        if (!available())
            throw std::runtime_error(
                "SDL2/X11: Clipboard service is unavailable.");
        if (payload.text.size() > maximum_clipboard_bytes ||
            payload.image.size() > maximum_clipboard_bytes) {
            throw std::runtime_error(
                "SDL2/X11: Clipboard item is too large.");
        }
        state.payload = payload;
        XSetSelectionOwner(state.display,
                           state.clipboard,
                           state.window,
                           CurrentTime);
        XFlush(state.display);
        if (XGetSelectionOwner(state.display, state.clipboard) !=
            state.window) {
            state.payload = {};
            throw std::runtime_error(
                "SDL2/X11: Unable to own the clipboard selection.");
        }
        state.owns = true;
    }

    void service() {
        if (!available())
            return;
        while (XPending(state.display) != 0) {
            XEvent event{};
            XNextEvent(state.display, &event);
            handle_event(event);
        }
    }

    void shutdown() {
        if (state.display) {
            if (state.window != None)
                XDestroyWindow(state.display, state.window);
            XCloseDisplay(state.display);
        }
        state = {};
    }
} // namespace linux::sdl2::x11_clipboard
