// Host stand-in for the parts of Arduino.h the ThingPulse OLEDDisplay uses (native tests only)
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
inline void yield() {}
inline void delay(unsigned long) {}
using std::min;
using std::max;
class String {
public:
    String(const char* s = "") : s_(s ? s : "") {}
    unsigned int length() const { return (unsigned int)strlen(s_); }
    const char* c_str() const { return s_; }
    void toCharArray(char* buf, unsigned int size, unsigned int index = 0) const {
        if (!size) return;
        unsigned int n = length() > index ? length() - index : 0;
        if (n > size - 1) n = size - 1;
        memcpy(buf, s_ + index, n); buf[n] = 0;
    }
private:
    const char* s_;
};
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
};
