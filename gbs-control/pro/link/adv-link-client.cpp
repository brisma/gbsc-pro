#include "adv-link-client.h"
#include <string.h>

static const uint8_t kPing = ADV_CMD_PING | ADV_WANT_REPLY, kInfo = ADV_CMD_INFO | ADV_WANT_REPLY,
                     kSignal = ADV_CMD_SIGNAL,   // 0x49 has the reply bit
                     kRegRead = ADV_CMD_REG_READ; // 0x4A too

uint8_t AdvLinkClient::start(uint8_t cmd, uint32_t now) {
    pending_ = cmd;
    tries_ = 1;
    lastTry_ = now;
    if (cmd == kPing) lastPing_ = now;
    return cmd;
}

uint8_t AdvLinkClient::due(uint32_t now, bool requestAllowed, bool linkFree) {
    if (!linkFree || !requestAllowed) return 0;
    if (haveFrame_ && now - lastFrame_ < kSilenceMs) return 0;
    if (pending_) {                                   // a retry of the request in progress
        if (now - lastTry_ < kTrySpacingMs) return 0;
        tries_++;
        lastTry_ = now;
        return pending_;
    }
    switch (info_.status) {
        case ADV_CONTROLLER_CHECKING:
            if (infoNow_) { infoNow_ = false; return start(kInfo, now); }
            if (!bootStarted_) { bootStarted_ = true; bootStart_ = now; return start(kPing, now); }
            return now - lastPing_ >= kBootPingEveryMs ? start(kPing, now) : 0;
        case ADV_CONTROLLER_NO_REPLY:
            if (regRead_.state == ADV_REG_PENDING && pending_ != kRegRead) { regRead_.state = ADV_REG_ERROR; regRead_.why = ADV_REG_WHY_CONTROLLER; }   // no answer to wait for
            return now - lastPing_ >= kSlowPingMs ? start(kPing, now) : 0;
        default: {
            if (infoAfterState_ && now - stateAt_ >= kInfoAfterStateMs) { infoAfterState_ = false; return start(kInfo, now); }
            uint32_t period = info_.status == ADV_CONTROLLER_INCOMPATIBLE ? kIncompatiblePeriodMs : kInfoPeriodMs;
            if (now - lastInfo_ >= period) return start(kInfo, now);
            if (regRead_.state == ADV_REG_PENDING) {      // before SIGNAL: someone waits for it
                if (info_.status != ADV_CONTROLLER_OK) { regRead_.state = ADV_REG_ERROR; regRead_.why = ADV_REG_WHY_CONTROLLER; return 0; }
                if (!(info_.info.caps & ADV_INFO_CAP_REG_READ)) { regRead_.state = ADV_REG_UNSUPPORTED; return 0; }
                return start(kRegRead, now);
            }
            // SIGNAL: only an ADV Controller that announces it; often while the decoder is powered (S-Video/Composite)
            if (info_.status != ADV_CONTROLLER_OK || !(info_.info.caps & ADV_INFO_CAP_SIGNAL)) return 0;
            uint32_t every = signalFast_ || (haveSignal_ && (signal_.flags & ADV_SIG_POWERED)) ? kSignalPeriodMs : kSignalIdlePeriodMs;
            if (signalStarted_ && haveSignal_ && now - lastSignal_ < every) return 0;
            if (signalStarted_ && !haveSignal_ && now - lastSignal_ < (signalFast_ ? kSignalPeriodMs : kSignalIdlePeriodMs)) return 0;
            signalStarted_ = true;
            lastSignal_ = now;
            return start(kSignal, now);
        }
    }
}

void AdvLinkClient::result(uint8_t req, bool got, uint8_t cmd, const uint8_t* payload, uint8_t len, uint32_t now) {
    frameSent(now);
    bool ok = got && cmd == (uint8_t)(req | ADV_RESP);
    if (req == kPing) {
        pending_ = 0;                                 // boot and slow pings are single tries on their own cadence
        if (ok && len >= 1 && payload[0] == ADV_PROTOCOL) {
            info_.status = ADV_CONTROLLER_CHECKING;             // PING -> INFO -> OK
            infoNow_ = true;
            bootStarted_ = true;
            return;
        }
        if (ok && len >= 1) {                         // answers with another protocol: another firmware generation
            info_.status = ADV_CONTROLLER_INCOMPATIBLE;
            lastInfo_ = now;
            return;
        }
        if (info_.status == ADV_CONTROLLER_CHECKING && ++bootTries_ >= kBootMinTries && now - bootStart_ >= kBootWindowMs)
            enterNoReply(now);
        return;
    }
    if (req == kRegRead) {                            // own result: never touches failedCycles_ or the status
        uint8_t st, n;
        if (ok && advRegReadReplyUnpack(payload, len, &st, regRead_.v, &n)) {
            regRead_.replyStatus = st;
            regRead_.why = st != ADV_REG_READ_OK ? ADV_REG_WHY_STATUS : n != regRead_.count ? ADV_REG_WHY_COUNT : ADV_REG_WHY_NONE;
            finishRegRead(regRead_.why == ADV_REG_WHY_NONE ? ADV_REG_DONE : ADV_REG_ERROR);
            return;
        }
        if (got) regRead_.badReplies++;               // an answer that is not a register read reply
        if (tries_ < kTries) return;                  // retried by due() after the spacing
        regRead_.why = ADV_REG_WHY_NO_REPLY;
        finishRegRead(ADV_REG_ERROR);
        return;
    }
    if (req == kSignal) {                             // one try; a lost reply never counts towards NO_REPLY
        pending_ = 0;
        AdvSignal g;
        if (ok && advSignalUnpack(payload, len, &g)) { signal_ = g; haveSignal_ = true; signalAt_ = now; }
        return;
    }
    // INFO
    AdvInfo in;
    if (ok && advInfoUnpack(payload, len, &in)) {
        pending_ = 0;
        failedCycles_ = 0;
        checkInfoFails_ = 0;
        lastInfo_ = now;
        info_.info = in;
        info_.haveInfo = true;
        info_.status = in.stateVersion == ADV_STATE_VERSION ? ADV_CONTROLLER_OK : ADV_CONTROLLER_INCOMPATIBLE;
        if (info_.status == ADV_CONTROLLER_OK && haveHash_) {
            if (in.stateHash == sentHash_) info_.notApplied = false;
            else if (resends_ < kMaxResends) { resends_++; resend_ = true; }
            else info_.notApplied = true;
        }
        return;
    }
    if (tries_ < kTries) return;                      // retried by due() after the spacing
    pending_ = 0;
    lastInfo_ = now;
    if (info_.status == ADV_CONTROLLER_CHECKING) {              // INFO after a PING reply failed: back to pinging
        bootTries_++;
        if (++checkInfoFails_ >= kFailedCycles) enterNoReply(now);   // answers PING but never INFO
        return;
    }
    if (info_.status == ADV_CONTROLLER_INCOMPATIBLE && !info_.haveInfo) return;   // known from PING: INFO may be foreign
    if (++failedCycles_ >= kFailedCycles) {
        info_.status = ADV_CONTROLLER_NO_REPLY;
        lastPing_ = now;
    }
}

void AdvLinkClient::restart() {
    bool given = noticeGiven_;
    AdvRegRead read = haveNext_ ? nextRead_ : regRead_;
    *this = AdvLinkClient();
    noticeGiven_ = given;
    if (read.state == ADV_REG_PENDING) read.state = ADV_REG_ERROR;   // its caller must not wait forever
    regRead_ = read;
}

void AdvLinkClient::enterNoReply(uint32_t now) {
    info_.status = ADV_CONTROLLER_NO_REPLY;
    lastPing_ = now;
    checkInfoFails_ = 0;
    if (!noticeGiven_) { notice_ = true; noticeGiven_ = true; }   // the "flash it over USB" toast: once
}

void AdvLinkClient::stateSent(uint16_t hash, uint32_t now) {
    frameSent(now);
    bool changed = !haveHash_ || hash != sentHash_;
    if (changed) { sentHash_ = hash; resends_ = 0; info_.notApplied = false; }
    haveHash_ = true;
    if (changed || resendCheck_) {                    // a keep-alive of the same state needs no INFO check
        infoAfterState_ = true;
        stateAt_ = now;
        resendCheck_ = false;
    }
}

void AdvLinkClient::requestRegRead(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t gen) {
    AdvRegRead& r = pending_ == kRegRead ? nextRead_ : regRead_;   // the one on the wire keeps its payload
    r.state = ADV_REG_PENDING;
    r.dev = dev; r.map = map; r.reg = reg; r.count = count; r.gen = gen;
    r.why = ADV_REG_WHY_NONE; r.replyStatus = 0; r.badReplies = 0;
    if (&r == &nextRead_) haveNext_ = true;
    else if (info_.status == ADV_CONTROLLER_NO_REPLY) { r.state = ADV_REG_ERROR; r.why = ADV_REG_WHY_CONTROLLER; }   // nothing answers: said at once
}

void AdvLinkClient::finishRegRead(uint8_t state) {
    pending_ = 0;
    regRead_.state = state;
    if (haveNext_) { regRead_ = nextRead_; haveNext_ = false; }   // the newer request replaces this result
}

bool AdvLinkClient::requestPayload(uint8_t* out, uint8_t* len) const {
    if (pending_ != kRegRead) { *len = 0; return false; }
    *len = (uint8_t)advRegReadReqPack(regRead_.dev, regRead_.map, regRead_.reg, regRead_.count, out);
    return true;
}
