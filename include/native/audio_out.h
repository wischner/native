//
// Declares bounded push-style default-device signed 16-bit PCM output.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace native
{
    namespace detail { struct audio_out_peer; }
    class audio_out
    {
    public:
        // Configure exact input PCM rate, mono/stereo, and total frame bound.
        // Invalid configuration throws invalid_argument; no device is opened.
        audio_out(unsigned rate, unsigned channels, std::size_t capacity_frames);
        // Stop output and release the device after the producer has stopped.
        ~audio_out();
        audio_out(const audio_out &) = delete;
        audio_out &operator=(const audio_out &) = delete;
        // Open default output; false means unavailable. Idempotent.
        bool open();
        // Return whether the device remains available.
        bool get_open() const;
        // Return configured input frames per second.
        unsigned get_rate() const;
        // Return configured interleaved samples per frame.
        unsigned get_channels() const;
        // Return the bound on accepted pending input frames.
        std::size_t get_capacity_frames() const;
        // Copy a complete block without waiting for space; false rejects all.
        // Malformed sample shape throws invalid_argument. Empty is a no-op.
        bool queue(std::span<const std::int16_t> samples);
        // Return conservative occupancy, including native submitted frames.
        std::size_t get_queued_frames() const;
        // Discard output and stop native callbacks. Serialize against producer.
        void close();
    private:
        std::unique_ptr<detail::audio_out_peer> _peer;
    };
}
