#include "ap-retry.h"

bool ApRetry::due(uint32_t now, bool apOn, bool joined, bool busy) {
    if (apOn != ap_) {
        ap_ = apOn;
        since_ = now;
        return false;
    }
    if (!apOn || joined || busy || now - since_ < kEveryMs) return false;
    since_ = now;   // one try; if the unit is still an access point a minute later, another one
    return true;
}
