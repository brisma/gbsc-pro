// Volume percent -> PT2257 attenuation (pure, host-tested). 100 % = 0 dB, 80 % = -12 dB (the default),
// 1 % about -59 dB, 0 % = silent (the PT2257 mute command): 0.6 dB per percent, rounded to the chip's 1 dB steps
#pragma once
#include <stdint.h>

inline uint8_t pt2257AttenuationDb(uint8_t percent) {
    if (percent > 100) percent = 100;
    return (uint8_t)(((100 - percent) * 6 + 5) / 10);
}

inline bool pt2257Silent(uint8_t percent) { return percent == 0; }

// Attenuation from -> to (dB, 0..79) as two command bytes: the chip plays the mix of old and new between them,
// so the byte whose mix is quieter goes first (tens first gives newTens+oldUnits, units first oldTens+newUnits;
// they sum to from + to, so the quieter one is never louder than both ends: no click crossing a decade)
inline void pt2257StepCommands(uint8_t from, uint8_t to, uint8_t out[2]) {
    if (from > 79) from = 79;
    if (to > 79) to = 79;
    uint8_t tens = (uint8_t)(0xE0 + to / 10), units = (uint8_t)(0xD0 + to % 10);
    uint8_t tensFirst = (uint8_t)((to / 10) * 10 + from % 10), unitsFirst = (uint8_t)((from / 10) * 10 + to % 10);
    out[0] = tensFirst >= unitsFirst ? tens : units;
    out[1] = tensFirst >= unitsFirst ? units : tens;
}
