/*
 * PT2257 Electronic Volume Controller Driver
 * 2-channel I2C audio attenuator (0 to -79 dB)
 *
 * Datasheet: https://www.princeton.com.tw/Portals/0/Product/PT2257.pdf
 * Original author: Karabanov Aleksandr
 */

#ifndef PT2257_H_
#define PT2257_H_
#include "pt2257-map.h"

#include <stdint.h>
#include "../platform/platform-caps.h"

// I2C address (7-bit)
#define PT2257_ADDR 0x44

// Attenuation command bases

// Mute control
#define PT2257_MUTE_ON   0x79
#define PT2257_MUTE_OFF  0x78

// Set attenuation level (0-79 dB, both channels)
// Attenuation 0 = 0 dB (max), Attenuation 79 = -79 dB (min)
// The last level written (79 at power-up: the first write takes the quiet path as well)
inline uint8_t& PT2257_lastDb() { static uint8_t db = 79; return db; }

inline void PT2257_setAttenuation(uint8_t dB) {
    if (!hwHas(CAP_AUDIO)) return;
    if (dB > 79) dB = 79;
    uint8_t cmd[2];
    pt2257StepCommands(PT2257_lastDb(), dB, cmd);   // tens/units order: never louder in between
    Wire.beginTransmission(PT2257_ADDR);
    Wire.write(cmd[0]);
    Wire.write(cmd[1]);
    if (Wire.endTransmission() == 0) PT2257_lastDb() = dB;   // a NACK leaves the chip at the old level
}

// Set volume in percent (pt2257-map.h); 0 % is silenced by the caller with PT2257_mute
inline void PT2257_setVolume(uint8_t percent) {
    PT2257_setAttenuation(pt2257AttenuationDb(percent));
}

// Mute/unmute audio
inline void PT2257_mute(bool mute) {
    if (!hwHas(CAP_AUDIO)) return;
    Wire.beginTransmission(PT2257_ADDR);
    Wire.write(mute ? PT2257_MUTE_ON : PT2257_MUTE_OFF);
    Wire.endTransmission();
}

#endif // PT2257_H_
