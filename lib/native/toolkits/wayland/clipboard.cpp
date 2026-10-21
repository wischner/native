//
// Transfers UTF-8 and PNG MIME representations through the Wayland
// clipboard.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../clipboard_backend.h"
#include <stdexcept>
namespace native::detail
{
    clipboard_payload read_clipboard() {
        linux::wayland::initialize();
        clipboard_payload result;
        auto *clipboard =
            gdk_display_get_clipboard(gdk_display_get_default());
        struct read_state
        {
            bool done = false;
            GInputStream *stream = nullptr;
            GError *error = nullptr;
        };
        for (const char *mime :
             {"text/plain;charset=utf-8", "image/png"}) {
            if (!gdk_content_formats_contain_mime_type(
                    gdk_clipboard_get_formats(clipboard), mime))
                continue;
            read_state state;
            const char *types[] = {mime, nullptr};
            gdk_clipboard_read_async(
                clipboard,
                types,
                G_PRIORITY_DEFAULT,
                nullptr,
                +[](GObject *object,
                    GAsyncResult *result,
                    gpointer data) {
                    auto &state = *static_cast<read_state *>(data);
                    state.stream =
                        gdk_clipboard_read_finish(GDK_CLIPBOARD(object),
                                                  result,
                                                  nullptr,
                                                  &state.error);
                    state.done = true;
                },
                &state);
            while (!state.done)
                g_main_context_iteration(nullptr, true);
            if (state.error) {
                g_error_free(state.error);
                continue;
            }
            if (!state.stream)
                continue;
            std::vector<std::uint8_t> bytes;
            std::uint8_t chunk[8192];
            for (;;) {
                const auto size = g_input_stream_read(state.stream,
                                                      chunk,
                                                      sizeof(chunk),
                                                      nullptr,
                                                      &state.error);
                if (size <= 0)
                    break;
                bytes.insert(bytes.end(), chunk, chunk + size);
            }
            g_object_unref(state.stream);
            if (state.error) {
                g_error_free(state.error);
                continue;
            }
            if (mime[0] == 't') {
                result.has_text = true;
                result.text.assign(bytes.begin(), bytes.end());
            } else {
                result.has_image = true;
                result.image = std::move(bytes);
            }
        }
        return result;
    }
    void write_clipboard(const clipboard_payload &payload) {
        linux::wayland::initialize();
        std::vector<GdkContentProvider *> providers;
        auto add =
            [&](const char *mime, const void *data, std::size_t size) {
                auto *bytes = g_bytes_new(data, size);
                providers.push_back(
                    gdk_content_provider_new_for_bytes(mime, bytes));
                g_bytes_unref(bytes);
            };
        if (payload.has_text)
            add("text/plain;charset=utf-8",
                payload.text.data(),
                payload.text.size());
        if (payload.has_image)
            add("image/png",
                payload.image.data(),
                payload.image.size());
        auto *provider = providers.empty()
                             ? nullptr
                             : gdk_content_provider_new_union(
                                   providers.data(), providers.size());
        const bool accepted = gdk_clipboard_set_content(
            gdk_display_get_clipboard(gdk_display_get_default()),
            provider);
        if (provider)
            g_object_unref(provider);
        if (!accepted)
            throw std::runtime_error(
                "Wayland clipboard ownership was rejected.");
    }
} // namespace native::detail
