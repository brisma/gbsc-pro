#include "i2p-guard.h"

bool I2pGuard::update(const I2pGuardIn& in, uint32_t now) {
    if (in.input != input_) {                             // another input: another source, I2P first
        input_ = in.input;
        inhibit_ = waiting_ = false;
    }
    if (!(in.i2p && in.decoder && in.locked && in.progressive && in.hz50)) {
        inhibit_ = waiting_ = false;
        return false;
    }
    if (inhibit_) return true;
    if (in.shown) {
        waiting_ = false;
        return false;
    }
    if (!waiting_) {
        waiting_ = true;
        since_ = now;
    }
    if (now - since_ >= kEngageMs) inhibit_ = true;
    return inhibit_;
}
