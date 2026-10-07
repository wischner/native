//
// Adapts bounded PCM to privately owned CoreAudio AudioQueue output buffers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../audio_backend.h"
#include <algorithm>
#include <AudioToolbox/AudioToolbox.h>
#include <array>

namespace native::detail
{
    class core_audio_device final : public audio_device
    {
        AudioQueueRef _queue = nullptr;
        audio_out_peer *_owner = nullptr;
        std::array<AudioQueueBufferRef, 3> _buffers{};
        std::array<std::size_t, 3> _previous{};
        static void callback(void *context, AudioQueueRef queue,
                             AudioQueueBufferRef buffer) {
            auto &self = *static_cast<core_audio_device *>(context);
            for (unsigned i = 0; i < self._buffers.size(); ++i) {
                if (self._buffers[i] != buffer) continue;
                const auto samples = buffer->mAudioDataBytesCapacity / 2;
                self._previous[i] = self._owner->render(std::span(
                    static_cast<std::int16_t *>(buffer->mAudioData), samples),
                    self._previous[i]);
                buffer->mAudioDataByteSize = samples * 2;
                if (AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr) != noErr)
                    self._owner->open = false;
                return;
            }
        }
    public:
        ~core_audio_device() override { close(); }
        bool open(audio_out_peer &owner) override {
            _owner = &owner;
            AudioStreamBasicDescription format{};
            format.mSampleRate = owner.rate;
            format.mFormatID = kAudioFormatLinearPCM;
            format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
            format.mBytesPerPacket = format.mBytesPerFrame = owner.channels * 2;
            format.mFramesPerPacket = 1;
            format.mChannelsPerFrame = owner.channels;
            format.mBitsPerChannel = 16;
            if (AudioQueueNewOutput(&format, callback, this, nullptr, nullptr,
                                    0, &_queue) != noErr) return false;
            for (auto &buffer : _buffers)
                if (AudioQueueAllocateBuffer(_queue, std::max(1U, owner.rate / 100)
                    * owner.channels * 2, &buffer) != noErr) return false;
            for (auto buffer : _buffers) callback(this, _queue, buffer);
            return AudioQueueStart(_queue, nullptr) == noErr;
        }
        void close() override {
            if (_queue) { AudioQueueStop(_queue, true); AudioQueueDispose(_queue, true); }
            _queue = nullptr;
        }
    };
    std::unique_ptr<audio_device> create_audio_device() {
        return std::make_unique<core_audio_device>();
    }
}
