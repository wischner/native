//
// Adapts bounded PCM to an exact-format privately owned SDL audio callback.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../audio_backend.h"
#include <SDL.h>
#include <algorithm>
#include <cstring>

namespace native::detail
{
    class sdl_audio_device final : public audio_device
    {
        SDL_AudioDeviceID _device = 0;
        audio_out_peer *_owner = nullptr;
        std::size_t _previous = 0;
        bool _initialized = false;
        static void callback(void *context, Uint8 *bytes, int length) {
            auto &self = *static_cast<sdl_audio_device *>(context);
            auto samples = std::span(reinterpret_cast<std::int16_t *>(bytes),
                static_cast<std::size_t>(length) / sizeof(std::int16_t));
            self._previous = self._owner->render(samples, self._previous);
        }
    public:
        ~sdl_audio_device() override { close(); }
        bool open(audio_out_peer &owner) override {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
            _initialized = true;
            _owner = &owner;
            SDL_AudioSpec wanted{};
            wanted.freq = static_cast<int>(owner.rate);
            wanted.channels = static_cast<Uint8>(owner.channels);
            wanted.format = AUDIO_S16SYS;
            wanted.samples = 256;
            wanted.callback = callback;
            wanted.userdata = this;
            _device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
            if (!_device) return false;
            SDL_PauseAudioDevice(_device, 0);
            return true;
        }
        bool available() const override {
            return _device && SDL_GetAudioDeviceStatus(_device) != SDL_AUDIO_STOPPED;
        }
        void close() override {
            if (_device) SDL_CloseAudioDevice(_device);
            _device = 0;
            if (_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
            _initialized = false;
        }
    };
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<sdl_audio_device>();
    }
}
