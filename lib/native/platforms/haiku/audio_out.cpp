//
// Feeds signed PCM through a privately owned Haiku media-kit SoundPlayer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../audio_backend.h"
#include <algorithm>
#include <SoundPlayer.h>
#include <MediaDefs.h>
#include <memory>

namespace native::detail
{
    class haiku_audio_device final : public audio_device
    {
        std::unique_ptr<BSoundPlayer> _player;
        audio_out_peer *_owner = nullptr;
        std::size_t _previous = 0;
        static void callback(void *context, void *buffer, size_t bytes,
                             const media_raw_audio_format &) {
            auto &self = *static_cast<haiku_audio_device *>(context);
            self._previous = self._owner->render(std::span(
                static_cast<std::int16_t *>(buffer), bytes / 2), self._previous);
        }
    public:
        ~haiku_audio_device() override { close(); }
        bool open(audio_out_peer &owner) override {
            _owner = &owner;
            media_raw_audio_format format = media_raw_audio_format::wildcard;
            format.frame_rate = owner.rate;
            format.channel_count = owner.channels;
            format.format = media_raw_audio_format::B_AUDIO_SHORT;
            format.byte_order = B_MEDIA_HOST_ENDIAN;
            format.buffer_size = std::max(1U, owner.rate / 100) * owner.channels * 2;
            _player = std::make_unique<BSoundPlayer>(&format, "Native PCM",
                callback, nullptr, this);
            if (_player->InitCheck() != B_OK || _player->Start() != B_OK) return false;
            _player->SetHasData(true);
            return true;
        }
        void close() override {
            if (_player) { _player->Stop(true, true); _player.reset(); }
        }
    };
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<haiku_audio_device>();
    }
}
