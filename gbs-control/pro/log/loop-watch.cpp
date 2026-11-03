#include "loop-watch.h"

bool LoopWatch::begin(uint32_t now, Report& r) {
    bool due = false;
    if (started_) {
        mark(kRest, now);
        uint32_t pass = now - start_;
        if (pass >= kSlowMs) {
            slow_++;
            if (!reported_ || now - reportAt_ >= kQuietMs) {
                r.passMs = pass;
                r.stretch = worst_;
                r.stretchMs = worstMs_;
                r.slowPasses = slow_;
                reported_ = true;
                reportAt_ = now;
                slow_ = 0;
                due = true;
            }
        }
    }
    started_ = true;
    start_ = last_ = now;
    worst_ = kRest;
    worstMs_ = 0;
    return due;
}

void LoopWatch::mark(uint8_t id, uint32_t now) {
    uint32_t d = now - last_;
    last_ = now;
    if (d >= worstMs_) { worstMs_ = d; worst_ = id; }
}
