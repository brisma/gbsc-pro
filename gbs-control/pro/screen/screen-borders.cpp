#include "screen-borders.h"

bool screenBorderMove(uint16_t blankStart, uint16_t blankStop, uint16_t total, int16_t delta, uint16_t margin,
                      uint16_t& outStart, uint16_t& outStop) {
    if (blankStart <= blankStop) return false;
    int32_t st = (int32_t)blankStart + delta, sp = (int32_t)blankStop + delta;
    if (delta < 0 && sp < (int32_t)margin) return false;                       // only the edge that moves
    if (delta > 0 && st > (int32_t)total) return false;                        // towards its limit counts; presets
                                                                               // themselves end the window at total
    outStart = (uint16_t)st;
    outStop = (uint16_t)sp;
    return true;
}
