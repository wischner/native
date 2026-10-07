//
// Loads optional ALSA PCM output privately for all non-SDL Linux toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../audio_backend.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <deque>
#include <dlfcn.h>
#include <thread>

namespace native::detail
{
    // ALSA is runtime optional. Keep its ABI and all handles in this adapter.
    struct _snd_pcm;
    class alsa_audio_device final : public audio_device
    {
        void *_library = nullptr;
        _snd_pcm *_pcm = nullptr;
        audio_out_peer *_owner = nullptr;
        std::jthread _worker;
        int (*_open)(_snd_pcm **, const char *, int, int) = nullptr;
        int (*_close)(_snd_pcm *) = nullptr;
        int (*_params)(_snd_pcm *, int, int, unsigned, unsigned, int, unsigned) = nullptr;
        int (*_format)(const char *) = nullptr;
        long (*_write)(_snd_pcm *, const void *, unsigned long) = nullptr;
        int (*_delay)(_snd_pcm *, long *) = nullptr;
        int (*_recover)(_snd_pcm *, int, int) = nullptr;
        int (*_drop)(_snd_pcm *) = nullptr;

        template<class T> bool load(T &function, const char *name) {
            function = reinterpret_cast<T>(dlsym(_library, name));
            return function != nullptr;
        }
        void run(std::stop_token stop) {
            const std::size_t frames = std::max(1U, _owner->rate / 100);
            std::vector<std::int16_t> buffer(frames * _owner->channels);
            struct packet { std::size_t end, real; };
            std::deque<packet> packets;
            std::size_t written = 0;
            while (!stop.stop_requested()) {
                long delay = 0;
                if (_delay(_pcm, &delay) < 0) delay = 0;
                const auto pending = static_cast<std::size_t>(std::max(0L, delay));
                const auto played = written - std::min(written, pending);
                while (!packets.empty() && packets.front().end <= played) {
                    _owner->complete(packets.front().real);
                    packets.pop_front();
                }
                const std::size_t real = _owner->render(buffer, 0);
                std::size_t offset = 0;
                while (offset < frames && !stop.stop_requested()) {
                    const long count = _write(_pcm,
                        buffer.data() + offset * _owner->channels, frames - offset);
                    if (count > 0) { offset += count; written += count; }
                    else if (count == -11 || count == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    } else if (_recover(_pcm, static_cast<int>(count), 1) < 0) {
                        _owner->open = false;
                        return;
                    } else {
                        // Recovery discards device packets; free their real frames.
                        for (auto item : packets) _owner->complete(item.real);
                        packets.clear();
                        written = 0;
                    }
                }
                packets.push_back({written, real});
            }
            _drop(_pcm);
        }
    public:
        ~alsa_audio_device() override { close(); }
        bool open(audio_out_peer &owner) override {
            _owner = &owner;
            _library = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
            if (!_library || !load(_open, "snd_pcm_open") ||
                !load(_close, "snd_pcm_close") || !load(_params, "snd_pcm_set_params") ||
                !load(_format, "snd_pcm_format_value") || !load(_write, "snd_pcm_writei") ||
                !load(_delay, "snd_pcm_delay") || !load(_recover, "snd_pcm_recover") ||
                !load(_drop, "snd_pcm_drop")) return false;
            // Playback=0, nonblocking=1, RW_INTERLEAVED=3 in the stable ALSA ABI.
            if (_open(&_pcm, "default", 0, 1) < 0) return false;
            const int format = _format(std::endian::native == std::endian::little
                ? "S16_LE" : "S16_BE");
            if (_params(_pcm, format, 3, owner.channels, owner.rate, 0, 20000) < 0)
                return false;
            _worker = std::jthread([this](std::stop_token stop) {
                try { run(stop); }
                catch (...) { _owner->open = false; _drop(_pcm); }
            });
            return true;
        }
        void close() override {
            _worker.request_stop();
            if (_worker.joinable()) _worker.join();
            if (_pcm && _close) _close(_pcm);
            _pcm = nullptr;
            if (_library) dlclose(_library);
            _library = nullptr;
        }
    };
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<alsa_audio_device>();
    }
}
