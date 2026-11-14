// WebSocket slot watch (pure, host-tested): the slot state read once a second gives a line when a slot opens and one
// when it closes, with how long it stayed open. Read from the state, not from library events: in the async build the
// TCP layer closes a vanished client (no ack of the status sent every second within 5 s) without an event
#pragma once
#include <stdint.h>

enum : uint8_t { WSW_NONE, WSW_OPENED, WSW_CLOSED };

class WsSlotWatch {
public:
    uint8_t update(bool open, uint32_t now);           // WSW_*: what to log
    uint32_t openForMs() const { return openFor_; }    // the stretch that WSW_CLOSED ended
private:
    bool open_ = false;
    uint32_t since_ = 0, openFor_ = 0;
};
