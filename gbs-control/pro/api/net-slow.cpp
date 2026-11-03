#include "net-slow.h"

bool NetSlowConns::update(const NetConn* conns, uint8_t n, uint32_t now, NetSlowHit& hit) {
    for (Slot& s : slots_) {                             // closed connections free their slot
        if (!s.id) continue;
        bool open = false;
        for (uint8_t i = 0; i < n && !open; i++) open = conns[i].id == s.id;
        if (!open) s = Slot();
    }
    for (uint8_t i = 0; i < n; i++) {                    // new ones take a free slot (the rest are not tracked)
        bool known = false;
        for (const Slot& s : slots_) known = known || s.id == conns[i].id;
        if (known) continue;
        for (Slot& s : slots_) {
            if (s.id) continue;
            s = {conns[i].id, now, false};
            break;
        }
    }
    for (Slot& s : slots_) {
        if (!s.id || s.reported || now - s.since < kSlowMs) continue;
        bool busy = false;
        for (uint8_t i = 0; i < n && !busy; i++) busy = conns[i].id == s.id && conns[i].busy;
        if (!busy) continue;
        s.reported = true;
        hit = {s.id, now - s.since};
        return true;
    }
    return false;
}
