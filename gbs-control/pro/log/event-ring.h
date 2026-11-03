// Status history: the last 20 device
// events in RAM (input found/lost/changed, profile loaded/saved, resolution, update offered/done), read by
// GET /api/events. Text is built by the web app from type/a/b; the profile name is looked up at read time.
// Pure, host-tested (test_event_ring); the device glue is event-device.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum EventType : uint8_t { EV_INPUT_FOUND = 1, EV_INPUT_LOST, EV_INPUT_CHANGED, EV_PROFILE_LOADED, EV_PROFILE_SAVED,
                           EV_RESOLUTION, EV_UPDATE_OFFERED, EV_UPDATE_DONE };

struct Event { uint16_t seq; uint32_t ms; uint8_t type; uint8_t a, b; };   // 12 bytes

class EventRing {
public:
    static const uint8_t kCap = 20;
    // Returns the seq: 1..65535, then 1 again (0 is never used: since=0 always means everything)
    uint16_t add(uint32_t ms, uint8_t type, uint8_t a, uint8_t b);
    // The oldest event with a seq after since (0, or a since ahead of lastSeq() after a restart: the oldest)
    bool readAfter(uint16_t since, Event& out) const;
    uint16_t lastSeq() const { return next_ == 1 ? (count_ ? 65535 : 0) : (uint16_t)(next_ - 1); }
private:
    Event ev_[kCap] = {};
    uint8_t head_ = 0;       // slot of the next event
    uint8_t count_ = 0;
    uint16_t next_ = 1;
};

const char* eventTypeName(uint8_t type);   // "input_found", ...; "unknown" out of range
// {"seq":7,"ms":123456,"type":"input_found","a":1,"b":0}; 0 when it does not fit
size_t eventJson(const Event& e, char* out, size_t cap);

// The "resolution" event only when (preset, input) changed since the last one; *last starts as EVENT_RESOLUTION_NONE
static const uint16_t EVENT_RESOLUTION_NONE = 0xFFFF;
bool eventResolutionChanged(uint16_t* last, uint8_t preset, uint8_t input);

// GET /api/events body: {"boot":B,"events":[...]} with the events after since; returns the length, 0 when it does not fit
static const size_t EVENTS_JSON_MAX = 1536;   // 20 widest records (~71 chars) + the frame
size_t eventsJson(const EventRing& r, uint16_t since, uint16_t boot, char* out, size_t cap);

// Device glue (event-device.cpp): add with millis(), one log line, WebSocket notice "e:<seq>"
void eventAdd(uint8_t type, uint8_t a, uint8_t b);
const EventRing& eventRing();
