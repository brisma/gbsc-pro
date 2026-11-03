#include "si5351-lab.h"

static const uint32_t kVcoMaxHz = 900000000UL;

uint32_t si5351OutDivider(uint32_t freq, bool vco648, uint8_t* rdiv) {
    uint8_t r = 1;
    if (!freq) freq = 1;
    uint32_t d = kVcoMaxHz / freq;
    while (d > 900) {                          // the R divider takes what the multisynth cannot
        r = (uint8_t)(r * 2);
        d /= 2;
    }
    if (d % 2) d--;                            // even dividers only
    if (vco648) {                              // gbs-control-nx lib/si5351mcu (commit 58f1d83)
        if (d == 8 && freq >= 100000000UL) d = 6;
        if (d == 10 && freq >= 75000000UL && freq <= 90000000UL) d = 8;
    }
    if (rdiv) *rdiv = r;
    return d;
}

uint32_t si5351VcoHz(uint32_t freq, bool vco648) {
    uint8_t r = 1;
    uint32_t d = si5351OutDivider(freq, vco648, &r);
    return d * r * freq;
}

Si5351Status si5351DecodeStatus(uint8_t reg0) {
    Si5351Status s;
    s.sysInit = reg0 & 0x80;
    s.lolB = reg0 & 0x40;
    s.lolA = reg0 & 0x20;
    s.los = reg0 & 0x08;                     // LOS_XTAL (bit 4 is LOS_CLKIN, Si5351C only; AN619 register 0)
    s.revId = reg0 & 0x03;
    return s;
}
