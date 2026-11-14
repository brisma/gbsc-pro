// OLEDDisplay whose display() only counts frames; the 128x64 buffer is the screen under test
#pragma once
#include <string>
#include "oled-display.h"

class FakeOled : public OLEDDisplay {
public:
    FakeOled() { setGeometry(GEOMETRY_128_64); init(); frames = 0; }   // init() already sends one frame
    bool connect() override { return true; }
    void display() override { frames++; }
    int getBufferOffset() override { return 0; }
    void sendCommand(uint8_t c) override { if (c == 0xAE) off = true; else if (c == 0xAF) off = false; }   // SSD1306 display off / on
    bool pixel(int x, int y) const { return (buffer[x + (y / 8) * 128] >> (y & 7)) & 1; }
    std::string pbm() const {
        std::string s = "P1\n128 64\n";
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) s += pixel(x, y) ? '1' : '0';
            s += '\n';
        }
        return s;
    }
    int frames = 0;
    bool off = false;                  // the panel is asleep
};
