// OSD chip bus: the STV9426 address space (RAM 0x000-0x3FF, ROM 0x2000-0x32FF, registers 0x3FF0-0x3FFF)
#pragma once
#include <stdint.h>

class OsdBus {
public:
    virtual ~OsdBus() {}
    virtual bool write(uint16_t addr, const uint8_t* data, uint8_t len) = 0;   // auto-increment; false on I2C error
    virtual void read(uint16_t addr, uint8_t* out, uint8_t len) = 0;
};
