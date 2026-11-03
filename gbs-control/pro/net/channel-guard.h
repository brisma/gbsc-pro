// Channel guard: some routers put a channel switch announcement in every beacon and never move. The ESP8266 SDK
// obeys about a second later, leaves the router's channel and loses the link with a beacon timeout, again and again.
// The guard tells a real move from a false one: after the first move it puts the radio back; a router that is still
// there announces again and the SDK leaves a second time (false: from then on the radio is put back at once); when
// nothing happens for kChannelProbeMs the router really moved and the radio follows. Pure and host-tested; the device
// glue (channel-guard-device.cpp) reads and sets the radio channel.
#pragma once
#include <stdint.h>

static const uint16_t kChannelProbeMs = 3000;    // back on the old channel: a second move within this is a false one
static const uint16_t kChannelProvenMs = 15000;  // after following: a beacon timeout within this was a false one too
static const uint8_t kWifiReasonBeaconTimeout = 200;

enum : uint8_t { CG_NONE, CG_PROBE, CG_FALSE, CG_FOLLOWED };
struct ChannelGuardStep { uint8_t setChannel; uint8_t event; };   // setChannel 0: leave the radio alone

class ChannelGuard {
public:
    void connected(uint8_t channel);                     // associated: the router's channel
    void dropped(uint8_t reason, uint32_t now);          // the SDK's disconnect reason
    ChannelGuardStep tick(uint8_t radioChannel, uint32_t now);   // every pass while connected and not scanning
    bool distrust() const { return distrust_; }          // a false announcement was seen since power-on
    uint8_t home() const { return home_; }               // 0: not connected
    uint8_t announced() const { return candidate_; }     // the channel of the last announcement
    uint32_t reverts() const { return reverts_; }
private:
    enum : uint8_t { IDLE, PROBING, FOLLOWED };
    uint8_t home_ = 0, candidate_ = 0, phase_ = IDLE;
    bool distrust_ = false;
    uint32_t since_ = 0, reverts_ = 0;
};
