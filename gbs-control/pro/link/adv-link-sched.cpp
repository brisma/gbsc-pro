#include "adv-link-sched.h"
#include <string.h>

bool AdvLinkSched::tick(const AdvState& s, uint32_t now) {
    bool differs = !sent_ || memcmp(&s, &last_, sizeof(s)) != 0;
    if (differs) {
        if (!pending_) { pending_ = true; firstChange_ = now; lastChange_ = now; candidate_ = s; }
        else if (memcmp(&s, &candidate_, sizeof(s)) != 0) { lastChange_ = now; candidate_ = s; }
    } else {
        pending_ = false;                          // back to what the ADV Controller already has
    }
    bool send = false;
    if (!sent_ || kicked_) send = true;
    else if (pending_ && (now - lastChange_ >= kCoalesceMs || now - firstChange_ >= kMaxLatencyMs)) send = true;
    else if (!pending_ && now - lastSend_ >= kKeepAliveMs) send = true;
    if (!send) return false;
    // The ADV Controller re-initialises on a planned input step (decoder on/off, S-Video <-> CVBS, another input with
    // the decoder off); a resend may carry one the ADV Controller never received. RGB SCART listening <-> Composite
    // keeps the decoder: no quiet
    uint8_t steps = sent_ ? advPlan(&last_, &s, 1) : (uint8_t)ADV_STEP_INPUT;
    if ((steps & ADV_STEP_INPUT) || resendQuiet_) {
        quiet_ = true;
        inputSentAt_ = now;
        quietMs_ = advDecoderOn(&s) ? kDecoderQuietMs : kInputQuietMs;   // the whole decoder init runs
    }
    resendQuiet_ = false;
    last_ = s;
    sent_ = true;
    pending_ = kicked_ = false;
    lastSend_ = now;
    return true;
}

bool AdvLinkSched::requestAllowed(uint32_t now) const { return !quiet_ || now - inputSentAt_ >= quietMs_; }
