//
// Feeds exact input PCM through a private COM-owned WASAPI rendering worker.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../audio_backend.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <future>
#include <thread>

namespace native::detail
{
    class wasapi_audio_device final : public audio_device
    {
        std::jthread _worker;
        void run(audio_out_peer &owner, std::stop_token stop,
                 std::promise<bool> ready) {
            const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IMMDeviceEnumerator *enumerator = nullptr;
            IMMDevice *device = nullptr;
            IAudioClient *client = nullptr;
            IAudioRenderClient *render = nullptr;
            bool started = false, announced = false;
            const auto cleanup = [&] {
                if (started) client->Stop();
                if (render) render->Release();
                if (client) client->Release();
                if (device) device->Release();
                if (enumerator) enumerator->Release();
                if (SUCCEEDED(apartment)) CoUninitialize();
            };
            try {
                WAVEFORMATEX format{};
                format.wFormatTag = WAVE_FORMAT_PCM;
                format.nChannels = static_cast<WORD>(owner.channels);
                format.nSamplesPerSec = owner.rate;
                format.wBitsPerSample = 16;
                format.nBlockAlign = format.nChannels * 2;
                format.nAvgBytesPerSec = owner.rate * format.nBlockAlign;
                const auto success = [](HRESULT value) {
                    if (FAILED(value)) throw value;
                };
                success(apartment);
                success(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                    CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                    reinterpret_cast<void **>(&enumerator)));
                success(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
                success(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                    nullptr, reinterpret_cast<void **>(&client)));
                // Windows 7 flags omitted by older MinGW headers.
                constexpr DWORD convert_pcm = 0x80000000;
                constexpr DWORD default_quality = 0x08000000;
                success(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                    convert_pcm | default_quality,
                    200000, 0, &format, nullptr));
                success(client->GetService(__uuidof(IAudioRenderClient),
                    reinterpret_cast<void **>(&render)));
                UINT32 capacity = 0;
                success(client->GetBufferSize(&capacity));
                std::vector<std::int16_t> samples(capacity * owner.channels);
                success(client->Start());
                started = true;
                ready.set_value(true);
                announced = true;
                struct packet { std::size_t end, real; };
                std::deque<packet> packets;
                std::size_t submitted = 0;
                while (!stop.stop_requested()) {
                    UINT32 padding = 0;
                    success(client->GetCurrentPadding(&padding));
                    const auto played = submitted - std::min<std::size_t>(submitted, padding);
                    while (!packets.empty() && packets.front().end <= played) {
                        owner.complete(packets.front().real);
                        packets.pop_front();
                    }
                    const UINT32 frames = capacity - padding;
                    if (frames) {
                        BYTE *bytes = nullptr;
                        success(render->GetBuffer(frames, &bytes));
                        const auto real = owner.render(std::span(samples.data(),
                            frames * owner.channels), 0);
                        std::copy_n(samples.data(), frames * owner.channels,
                            reinterpret_cast<std::int16_t *>(bytes));
                        success(render->ReleaseBuffer(frames, 0));
                        submitted += frames;
                        packets.push_back({submitted, real});
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            } catch (...) {
                owner.open = false;
                if (!announced) ready.set_value(false);
            }
            cleanup();
        }
    public:
        ~wasapi_audio_device() override { close(); }
        bool open(audio_out_peer &owner) override {
            std::promise<bool> ready;
            auto opened = ready.get_future();
            _worker = std::jthread([this, &owner, ready = std::move(ready)]
                (std::stop_token stop) mutable { run(owner, stop, std::move(ready)); });
            return opened.get();
        }
        void close() override {
            _worker.request_stop();
            if (_worker.joinable()) _worker.join();
        }
    };
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<wasapi_audio_device>();
    }
}
