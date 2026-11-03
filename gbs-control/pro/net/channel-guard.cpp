#include "channel-guard.h"

void ChannelGuard::connected(uint8_t channel) {
    home_ = channel;
    phase_ = IDLE;
}

void ChannelGuard::dropped(uint8_t reason, uint32_t now) {
    if (phase_ == FOLLOWED && reason == kWifiReasonBeaconTimeout && now - since_ < kChannelProvenMs) distrust_ = true;
    home_ = 0;
    phase_ = IDLE;
}

ChannelGuardStep ChannelGuard::tick(uint8_t radioChannel, uint32_t now) {
    ChannelGuardStep none = {0, CG_NONE};
    if (!home_) return none;
    if (phase_ == FOLLOWED && now - since_ >= kChannelProvenMs) phase_ = IDLE;
    if (radioChannel == home_) {
        if (phase_ != PROBING || now - since_ < kChannelProbeMs) return none;
        home_ = candidate_;                              // silence on the old channel: the router is on the new one
        phase_ = FOLLOWED;
        since_ = now;
        return {home_, CG_FOLLOWED};
    }
    reverts_++;
    if (distrust_) return {home_, CG_NONE};
    if (phase_ == PROBING) {                             // left again: the router still announces from the old channel
        distrust_ = true;
        phase_ = IDLE;
        return {home_, CG_FALSE};
    }
    candidate_ = radioChannel;
    phase_ = PROBING;
    since_ = now;
    return {home_, CG_PROBE};
}
