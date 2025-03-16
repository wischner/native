//
// wave.hpp
// 
// Class for wave files.
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 13.06.2021   tstih
// 
#ifndef _WAVE_HPP
#define _WAVE_HPP

#include <cstdint>
#include <memory>

namespace nice {
//{{BEGIN.DEC}}
    class wave {
    public:
        // Construct an audio class.
        wave(const uint8_t *wav) {
            // Get wave header.
            phdr = (header_s *)wav;
        }
        // Duration in seconds.
        float duration_in_seconds() const {
            return (float)phdr->overall_size / (float)phdr->byterate;
        }
        // Get overall size.
        uint32_t len() const {
            return phdr->overall_size;
        }
        // Get raw wave.
        void* raw() const { return (void*)phdr; };

    private:
        // WAVE file header format. We are not the owner of the resource
        // so we'll just store the pointer.
        struct header_s {
            uint8_t riff[4];                // RIFF string
            uint32_t overall_size;		    // overall size of file in bytes
            uint8_t wave[4];			    // WAVE string
            uint8_t fmt_chunk_marker[4];    // fmt string with trailing null char
            uint32_t length_of_fmt;		    // length of the format data
            uint16_t format_type;		    // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
            uint16_t channels;			    // no.of channels
            uint32_t sample_rate;		    // sampling rate (blocks per second)
            uint32_t byterate;			    // SampleRate * NumChannels * BitsPerSample/8
            uint16_t block_align;		    // NumChannels * BitsPerSample/8
            uint16_t bits_per_sample;	    // bits per sample, 8- 8bits, 16- 16 bits etc
            uint8_t data_chunk_header [4];	// DATA string or FLLR string
            uint32_t data_size;				// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
            uint8_t data[0];                // Pointer to data.
        } *phdr;
    };
//{{END.DEC}}
} // namespace nice

#endif // _WAVE_HPP