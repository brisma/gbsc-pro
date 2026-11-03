// Loop watch (pure, host-tested): a loop() pass slower than kSlowMs is reported with its slowest stretch, so a stall
// names its cause in the log; at most one report every kQuietMs, with the slow passes counted since the last one
#pragma once
#include <stdint.h>

class LoopWatch {
public:
    static const uint32_t kSlowMs = 300, kQuietMs = 5000;
    static const uint8_t kRest = 0xFF;   // after the last mark of a pass (early returns end there too)
    struct Report { uint32_t passMs; uint8_t stretch; uint32_t stretchMs; uint16_t slowPasses; };
    // top of loop(): closes the previous pass; true (and r filled) when that pass is to be reported
    bool begin(uint32_t now, Report& r);
    // end of the stretch named id (1..254), started at the previous mark or at begin
    void mark(uint8_t id, uint32_t now);
private:
    bool started_ = false, reported_ = false;
    uint32_t start_ = 0, last_ = 0, reportAt_ = 0, worstMs_ = 0;
    uint8_t worst_ = kRest;
    uint16_t slow_ = 0;
};
