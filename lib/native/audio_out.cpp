//
// Implements exact-format bounded PCM admission and shared playback accounting.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/audio_out.h>
#include "audio_backend.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace native::detail
{
    audio_out_peer::audio_out_peer(unsigned input_rate, unsigned input_channels,
                                  std::size_t input_capacity)
        : rate(input_rate), channels(input_channels), capacity(input_capacity) {}

    std::size_t audio_out_peer::render(std::span<std::int16_t> output,
                                      std::size_t completed_frames) {
        std::fill(output.begin(), output.end(), 0);
        std::lock_guard guard(mutex);
        submitted -= std::min(submitted, completed_frames);
        const std::size_t count = std::min(stored, output.size() / channels);
        for (std::size_t i = 0; i < count * channels; ++i) {
            output[i] = ring[read];
            read = (read + 1) % ring.size();
        }
        stored -= count;
        submitted += count;
        return count;
    }
    void audio_out_peer::complete(std::size_t frames) {
        std::lock_guard guard(mutex);
        submitted -= std::min(submitted, frames);
    }
}

namespace native
{
    audio_out::audio_out(unsigned rate, unsigned channels, std::size_t capacity)
        : _peer(std::make_unique<detail::audio_out_peer>(rate, channels, capacity)) {
        if (!rate || rate > 384000 || (channels != 1 && channels != 2) ||
            !capacity || capacity > std::numeric_limits<std::size_t>::max() /
                channels / sizeof(std::int16_t))
            throw std::invalid_argument("Invalid PCM configuration.");
    }
    audio_out::~audio_out() { close(); }
    bool audio_out::open() {
        if (get_open()) return true;
        close();
        _peer->ring.resize(_peer->capacity * _peer->channels);
        _peer->device = detail::create_audio_device();
        // Adapters may begin callbacks during open, so the ring is ready first.
        _peer->open = true;
        if (!_peer->device || !_peer->device->open(*_peer)) {
            close();
            return false;
        }
        return true;
    }
    bool audio_out::get_open() const {
        if (_peer->open && _peer->device && !_peer->device->available())
            _peer->open = false;
        return _peer->open.load();
    }
    unsigned audio_out::get_rate() const { return _peer->rate; }
    unsigned audio_out::get_channels() const { return _peer->channels; }
    std::size_t audio_out::get_capacity_frames() const { return _peer->capacity; }
    bool audio_out::queue(std::span<const std::int16_t> samples) {
        if (samples.size() % _peer->channels)
            throw std::invalid_argument("Incomplete PCM frame.");
        if (samples.empty()) return true;
        if (!get_open()) return false;
        std::lock_guard guard(_peer->mutex);
        const std::size_t frames = samples.size() / _peer->channels;
        if (!_peer->open || frames > _peer->capacity -
            _peer->stored - _peer->submitted) return false;
        std::size_t write = (_peer->read + _peer->stored * _peer->channels)
            % _peer->ring.size();
        for (auto sample : samples) {
            _peer->ring[write] = sample;
            write = (write + 1) % _peer->ring.size();
        }
        _peer->stored += frames;
        return true;
    }
    std::size_t audio_out::get_queued_frames() const {
        std::lock_guard guard(_peer->mutex);
        return _peer->stored + _peer->submitted;
    }
    void audio_out::close() {
        _peer->open = false;
        if (_peer->device) _peer->device->close();
        _peer->device.reset();
        std::lock_guard guard(_peer->mutex);
        _peer->read = _peer->stored = _peer->submitted = 0;
        _peer->ring.clear();
    }
}
