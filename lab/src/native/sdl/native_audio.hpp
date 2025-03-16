//
// native_audio.hpp
// 
// Native class for playing sounds on SDL.
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 20.06.2021   tstih
// 
#ifndef _NATIVE_AUDIO_HPP
#define _NATIVE_AUDIO_HPP

#include <cstdint>
#include <memory>

#include <wave.hpp>

namespace nice {
//{{BEGIN.DEC}}
    class native_audio {
    public:
        // Construct an audio class.
        native_audio();
        // Destructs the audio class.
        virtual ~native_audio();
        // Play wave.
        void play_wave_async(const wave& w);
    };
//{{END.DEC}}
} // namespace nice

#endif // _NATIVE_AUDIO_HPP