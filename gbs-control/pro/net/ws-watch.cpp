#include "ws-watch.h"

uint8_t WsSlotWatch::update(bool open, uint32_t now) {
    if (open == open_) return WSW_NONE;
    open_ = open;
    if (open) {
        since_ = now;
        return WSW_OPENED;
    }
    openFor_ = now - since_;
    return WSW_CLOSED;
}
