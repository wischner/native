//
// Defines the private bounded PCM ring and OS device adapter interface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace native::detail
{
    struct audio_out_peer;
    class audio_device
    {
    public:
        virtual ~audio_device() = default;
        virtual bool open(audio_out_peer &owner) = 0;
        virtual void close() = 0;
        // Callback-driven adapters may additionally detect a stopped device.
        virtual bool available() const { return true; }
    };
    struct audio_out_peer
    {
        unsigned rate, channels;
        std::size_t capacity;
        std::atomic<bool> open{false};
        mutable std::mutex mutex;
        std::vector<std::int16_t> ring;
        std::size_t read = 0, stored = 0, submitted = 0;
        std::unique_ptr<audio_device> device;

        audio_out_peer(unsigned rate, unsigned channels, std::size_t capacity);
        // Release completed hardware frames, then copy one bounded input batch.
        std::size_t render(std::span<std::int16_t> output,
                           std::size_t completed_frames);
        // Release played frames without generating another buffer.
        void complete(std::size_t frames);
    };
    std::unique_ptr<audio_device> create_audio_device();
}
