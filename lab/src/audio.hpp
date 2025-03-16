//
// audio.hpp
// 
// Class for playing sounds.
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 13.06.2021   tstih
// 
#ifndef _AUDIO_HPP
#define _AUDIO_HPP

#include <cstdint>
#include <memory>

#include <wave.hpp>

namespace nice {
//{{BEGIN.DEC}}
    class audio {
    public:
        // Construct an audio class.
        audio() : pimpl_(std::make_unique<native_audio>()) {}
        // Destructs the audio class.
        virtual ~audio() {}
        // Play wave.
        void play_wave_async(const wave& w) {
            pimpl_->play_wave_async(w);
        }
    private:
        std::unique_ptr<native_audio> pimpl_;
    };
//{{END.DEC}}
} // namespace nice

#endif // _AUDIO_HPP