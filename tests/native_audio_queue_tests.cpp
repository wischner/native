//
// Tests production PCM queue arithmetic with a deterministic private device consumer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/audio_out.h>
#include "audio_backend.h"
#include <array>
#include <atomic>
#include <thread>
#include <iostream>
#include <stdexcept>

namespace
{
    native::detail::audio_out_peer *consumer = nullptr;
    bool device_available = true;
    void require(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
    class deterministic_device final : public native::detail::audio_device
    {
        bool open(native::detail::audio_out_peer &owner) override {
            consumer = &owner; return device_available;
        }
        void close() override { consumer = nullptr; }
    };
}
namespace native::detail
{
    // Resolves the factory in this executable instead of pulling an OS device
    // object from the static archive; only the production shared ring is tested.
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<deterministic_device>();
    }
}
int main() {
    try {
        bool invalid = false;
        try { native::audio_out bad(0, 3, 0); }
        catch (const std::invalid_argument &) { invalid = true; }
        require(invalid, "Configuration validation.");
        native::audio_out output(44100, 2, 3);
        std::array<std::int16_t, 4> samples{1, 2, 3, 4};
        require(!output.queue(samples), "Closed admission.");
        require(output.open() && output.open(), "Idempotent open.");
        require(output.queue(samples), "Initial block.");
        samples.fill(99);
        require(!output.queue(samples), "Atomic whole-block rejection.");
        std::array<std::int16_t, 6> played{};
        require(consumer->render(played, 0) == 2, "Render count.");
        require(played[0] == 1 && played[3] == 4 && played[4] == 0,
            "Copied samples, channel order and silence.");
        require(output.get_queued_frames() == 2, "Hardware frames omitted.");
        require(!output.queue(samples), "Submitted capacity omitted.");
        consumer->complete(2);
        require(output.queue(samples), "Admission after playback.");
        consumer->render(played, 0);
        require(played[0] == 99, "Ring wrap/order.");
        consumer->render(played, 2);
        require(output.get_queued_frames() == 0 && played[0] == 0, "Underflow.");
        invalid = false;
        try { output.queue(std::span(samples.data(), 3)); }
        catch (const std::invalid_argument &) { invalid = true; }
        require(invalid, "Partial frame accepted.");
        output.close(); output.close();
        device_available = false;
        require(!output.open() && !output.get_open(), "Failed open availability.");
        device_available = true;
        require(output.open() && output.get_queued_frames() == 0, "Reopen.");
        require(output.open(), "Concurrent reopen.");
        std::atomic<bool> done{false}, bad{false};
        unsigned played_count = 0;
        std::jthread playback([&] {
            std::array<std::int16_t, 2> frame{};
            std::size_t previous = 0;
            while (!done || output.get_queued_frames()) {
                const auto real = consumer->render(frame, previous);
                previous = real;
                if (real) {
                    ++played_count;
                    if (frame[0] != static_cast<int>(played_count) ||
                        frame[1] != static_cast<int>(played_count + 5000)) bad = true;
                }
                if (output.get_queued_frames() > output.get_capacity_frames()) bad = true;
                std::this_thread::yield();
            }
        });
        for (int i = 1; i <= 1000; ++i) {
            const std::array<std::int16_t, 2> frame{
                static_cast<std::int16_t>(i), static_cast<std::int16_t>(i + 5000)};
            while (!output.queue(frame)) std::this_thread::yield();
        }
        done = true;
        playback.join();
        require(!bad && played_count == 1000, "Concurrent order and admission bound.");
        consumer->open = false;
        require(!output.queue(samples) && !output.get_open(), "Device failure admission.");
        std::cout << "Bounded PCM contracts passed.\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
