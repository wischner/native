//
// native_audio.hpp
//
// Audio class implementation for SDL.
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
//
// 13.06.2021   tstih
//
#include <audio.hpp>

#include "nice.hpp"

namespace nice
{
//{{BEGIN.DEF}}

    native_audio::native_audio()
    {
    }

    native_audio::~native_audio()
    {
    }

    void native_audio::play_wave_async(const wave& w)
    {
        // Make SDL_RWops pointer from our wave memory
        // so that it can be treated as a stream. 
        SDL_RWops *rw_ops = SDL_RWFromMem(w.raw(), w.len());
        if (rw_ops == nullptr)
            return; // Something went wrong. 
        // We'll map wave to SDL_AudioSpec struct. 
        SDL_AudioSpec wav_spec;
        Uint32 wav_length;
        Uint8 *wav_buffer;
        // Now load these structures with data from wave. 
        // 1 means rw_ops will be released after this
        if (!SDL_LoadWAV_RW(rw_ops, 1, &wav_spec, &wav_buffer, &wav_length))
            return;
        // Grab our default audio device.
        SDL_AudioDeviceID did = SDL_OpenAudioDevice(
            NULL,
            0,
            &wav_spec,
            NULL, // Not interested in the changes you've made to play. 
            SDL_AUDIO_ALLOW_ANY_CHANGE);

        int success = SDL_QueueAudio(did, wav_buffer, wav_length);
        SDL_PauseAudioDevice(did, 0);

        // Sync.
        SDL_Delay(1000 * w.duration_in_seconds());

        // And now ...
        SDL_CloseAudioDevice(did);
        SDL_FreeWAV(wav_buffer);
    }

//{{END.DEF}}
} // namespace nice