// Device glue of the WiFi optimization: ICMP
// bursts to the gateway, the SDRAM clock (PLL_MS) writes, the triggers and the log lines around WifiTune
#pragma once
#include <stdint.h>

void wifiTuneLoop();                 // loop(), every pass
void wifiTunePresetLoaded();         // doPostPresetLoadSteps(), before its ResetSDRAM(): records the preset clock and
                                     // writes the session's clock over it (the caller's reset follows)
uint8_t wifiTunePresetClock();       // the PLL_MS the last preset or profile load wrote
uint8_t wifiTuneAppliedClock();      // the PLL_MS the tuner wrote or expects (the preset clock when it wrote none)
bool wifiTuneSlowChannel();          // /api/info wifi.slow: every clock is slow with this picture on this channel
uint8_t wifiTuneIdleClock();         // setResetParameters(): the PLL_MS while there is no signal
const char* wifiTuneClockName(uint8_t clock, char* buf, unsigned n);   // "144 MHz", "FB", else "PLL_MS n" in buf
