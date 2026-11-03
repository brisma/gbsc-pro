// Si5351 evaluation helpers (pure, host-tested): the output divider the library picks, with or without the
// gbs-control-nx rule that keeps the VCO near 648 MHz at 81 and 108 MHz, the PLL frequency it gives, and the
// device status register decoded. Used by the dev.si5351 measurements.
#pragma once
#include <stdint.h>

// Si5351mcu 0.7.1: 900 MHz / freq, halved into R while above 900, rounded down to even. vco648: nx's rule
// (divider 8 -> 6 at >= 100 MHz, divider 10 -> 8 at 75..90 MHz).
uint32_t si5351OutDivider(uint32_t freq, bool vco648, uint8_t* rdiv);
uint32_t si5351VcoHz(uint32_t freq, bool vco648);

struct Si5351Status { bool sysInit, lolB, lolA, los; uint8_t revId; };   // los = LOS_XTAL (Si5351A)
Si5351Status si5351DecodeStatus(uint8_t reg0);
