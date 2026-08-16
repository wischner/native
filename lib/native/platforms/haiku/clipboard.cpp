//
// Implements Haiku BClipboard snapshots and commits for UTF-8 text and
// lossless PNG image data stored as standard MIME fields.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <stdexcept>

#include <Clipboard.h>
#include <Message.h>

namespace
{
    // Unlock BClipboard on every exit path.
    class clipboard_lock
    {
    public:
        clipboard_lock() {
            if (!be_clipboard || !be_clipboard->Lock())
                throw std::runtime_error(
                    "Haiku: Unable to lock the clipboard.");
        }

        clipboard_lock(const clipboard_lock &) = delete;
        clipboard_lock &operator=(const clipboard_lock &) = delete;

        ~clipboard_lock() {
            be_clipboard->Unlock();
        }
    };
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        clipboard_lock lock;
        clipboard_payload payload;
        BMessage *data = be_clipboard->Data();
        if (!data)
            return payload;

        const void *value = nullptr;
        ssize_t size = 0;
        if (data->FindData("text/plain",
                           B_MIME_TYPE,
                           &value,
                           &size) == B_OK &&
            value && size > 0) {
            const auto *text = static_cast<const char *>(value);
            const std::size_t count =
                text[size - 1] == '\0'
                    ? static_cast<std::size_t>(size - 1)
                    : static_cast<std::size_t>(size);
            payload.text.assign(text, count);
            payload.has_text = true;
        }

        value = nullptr;
        size = 0;
        if (data->FindData("image/png",
                           B_MIME_TYPE,
                           &value,
                           &size) == B_OK &&
            value && size > 0) {
            const auto *first =
                static_cast<const std::uint8_t *>(value);
            payload.image.assign(first, first + size);
            payload.has_image = true;
        }
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        clipboard_lock lock;
        be_clipboard->Clear();
        BMessage *data = be_clipboard->Data();
        if (!data)
            throw std::runtime_error(
                "Haiku: Clipboard has no data message.");

        if (payload.has_text &&
            data->AddData("text/plain",
                          B_MIME_TYPE,
                          payload.text.c_str(),
                          payload.text.size() + 1) != B_OK) {
            throw std::runtime_error(
                "Haiku: Unable to stage clipboard text.");
        }
        if (payload.has_image &&
            data->AddData("image/png",
                          B_MIME_TYPE,
                          payload.image.data(),
                          payload.image.size()) != B_OK) {
            throw std::runtime_error(
                "Haiku: Unable to stage clipboard image.");
        }
        if (be_clipboard->Commit() != B_OK)
            throw std::runtime_error(
                "Haiku: Unable to commit clipboard content.");
    }
} // namespace native::detail
