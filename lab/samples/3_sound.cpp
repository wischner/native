#include <cstdint>

#include "nice.hpp"

using namespace nice;

// From resources/power_on_wav.cpp
extern uint8_t power_on_wav[];

wave wav_ { power_on_wav };
audio audio_;

void program()
{
    audio_.play_wave_async(wav_);
}