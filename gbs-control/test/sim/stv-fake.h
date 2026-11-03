// Host fake of the STV9426: 1 KB RAM + registers, and a decoder written from the datasheet (not from the
// driver) that turns the RAM image back into the screen the chip would display
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "stv9426-bus.h"

class FakeStv : public OsdBus {
public:
    uint8_t ram[1024];
    uint8_t reg[16];                    // 0x3FF0..0x3FFF
    std::vector<std::string> log;       // "R F3=86" register writes, "W 1C0+62" RAM bursts, in order
    int failWrites = 0;                 // > 0: the next N writes fail (NACK) and change nothing
    FakeStv();
    bool write(uint16_t addr, const uint8_t* data, uint8_t len) override;
    void read(uint16_t addr, uint8_t* out, uint8_t len) override;
    std::string decode(unsigned frameLines) const;   // descriptors walked from the page until frameLines
    std::string udcArt(uint8_t n) const;             // 18 slices x 12 pixels, '#' = foreground
};
