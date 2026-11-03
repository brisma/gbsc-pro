// Event ring, device side: millis() stamps, one log line per event, a WebSocket notice "e:<seq>" so open pages
// fetch GET /api/events?since=<their last seq>
#include <Arduino.h>
#include "event-ring.h"
#include "log.h"
#include "../../src/WebSocketsServer.h"

extern WebSocketsServer webSocket;   // gbs-control.ino

static EventRing gEvents;

void eventAdd(uint8_t type, uint8_t a, uint8_t b) {
    uint16_t seq = gEvents.add(millis(), type, a, b);
    logPrintf(LC_SYS, LL_DEBUG, PSTR("event %u: %s %u %u"), (unsigned)seq, eventTypeName(type), (unsigned)a, (unsigned)b);
    char msg[12];
    snprintf(msg, sizeof(msg), "e:%u", (unsigned)seq);
    webSocket.broadcastTXT(msg);
}

const EventRing& eventRing() { return gEvents; }
