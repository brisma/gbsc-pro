#include <Arduino.h>
#include "loop-watch.h"
#include "loop-watch-device.h"
#include "log.h"

static LoopWatch gWatch;

static const char kNames[] PROGMEM =
    "?\0api\0profile\0link\0menu\0osd\0ota\0audio\0web\0commands\0framelock\0syncwatch\0preset\0input\0wifitune\0detect\0";

static const char* stretchName(uint8_t id) {   // PROGMEM string
    if (id >= LW_COUNT) return PSTR("rest");
    const char* p = kNames;
    for (uint8_t i = 0; i < id; i++) p += strlen_P(p) + 1;
    return p;
}

void loopWatchBegin() {
    LoopWatch::Report r;
    if (!gWatch.begin(millis(), r)) return;
    char name[12];
    strncpy_P(name, stretchName(r.stretch), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    logPrintf(LC_SYS, LL_DEBUG, PSTR("loop slow: %lu ms, most in %s (%lu ms), %u slow passes since the last report"),
              (unsigned long)r.passMs, name, (unsigned long)r.stretchMs, (unsigned)r.slowPasses);
}

void loopWatchMark(uint8_t stretch) { gWatch.mark(stretch, millis()); }
