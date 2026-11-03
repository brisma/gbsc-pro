// Slow web connections (pure, host-tested): a port-80 connection with data still in flight (queued or unacked) after
// kSlowMs is reported once, so the device can log its TCP retransmits then (lost packets = the radio); an idle one (a
// browser's spare connection) is never slow
#pragma once
#include <stdint.h>

struct NetConn { uintptr_t id; bool busy; };   // busy: data queued or not acknowledged yet
struct NetSlowHit { uintptr_t id; uint32_t ageMs; };

class NetSlowConns {
public:
    static const uint8_t kSlots = 6;
    static const uint32_t kSlowMs = 5000;   // the whole web app (121 KB) takes 1.3-1.8 s at -62 dBm; the stalls hunted lasted 5+ s
    // the open port-80 connections now (lwIP pcb addresses); true with the first busy one at least kSlowMs old
    bool update(const NetConn* conns, uint8_t n, uint32_t now, NetSlowHit& hit);
private:
    struct Slot { uintptr_t id; uint32_t since; bool reported; };
    Slot slots_[kSlots] = {};
};
