// Access point fallback (pure, host-tested): when the saved network did not answer at power-on (a router that starts
// slower than the GBSC after a power cut), the unit stays an access point; the saved network is tried again every
// kEveryMs of access point time for kTryS seconds, never while a phone or a computer is joined to the access point
// (it may be setting up the WiFi) or an update runs
#pragma once
#include <stdint.h>

class ApRetry {
public:
    static const uint32_t kEveryMs = 60000;   // access point time before a try
    static const uint8_t kTryS = 15;          // a known router answers within a few seconds
    // apOn: the unit is an access point; joined: a station is on it; busy: an upload or an OTA pull
    bool due(uint32_t now, bool apOn, bool joined, bool busy);
private:
    bool ap_ = false;
    uint32_t since_ = 0;
};
