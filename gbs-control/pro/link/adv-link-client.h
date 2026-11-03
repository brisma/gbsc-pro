// Requests the ESP sends to the ADV Controller over the half-duplex link (PING, INFO) and what their replies mean.
// Pure: the device glue does the exchange.
#pragma once
#include <stdint.h>
#include "adv-link.h"

enum AdvControllerStatus : uint8_t { ADV_CONTROLLER_CHECKING, ADV_CONTROLLER_OK, ADV_CONTROLLER_NO_REPLY, ADV_CONTROLLER_INCOMPATIBLE };
struct AdvControllerInfo {
    AdvControllerStatus status;
    bool haveInfo;
    bool notApplied;     // the ADV Controller kept reporting another state hash after 3 resends
    AdvInfo info;
};

// Register read (ADV_CMD_REG_READ): one request at a time, its own result; failures never count towards NO_REPLY
enum : uint8_t { ADV_REG_IDLE, ADV_REG_PENDING, ADV_REG_DONE, ADV_REG_ERROR, ADV_REG_UNSUPPORTED };
// Why a read ended in ADV_REG_ERROR (for the log): the reply's status, fewer values than asked, no usable reply
enum : uint8_t { ADV_REG_WHY_NONE, ADV_REG_WHY_STATUS, ADV_REG_WHY_COUNT, ADV_REG_WHY_NO_REPLY, ADV_REG_WHY_CONTROLLER };
struct AdvRegRead {
    uint8_t state; uint8_t dev, map, reg, count, gen; uint8_t v[ADV_REG_READ_MAX];
    uint8_t why;           // ADV_REG_WHY_*
    uint8_t replyStatus;   // ADV_REG_READ_* of the last reply
    uint8_t badReplies;    // replies that did not unpack (wrong command or length)
};

class AdvLinkClient {
public:
    // Request to send now (PING or INFO, with the reply bit), 0 = none. requestAllowed: AdvLinkSched quiet
    // window; linkFree: no ESP OTA upload and no ADV Controller update owning the link
    uint8_t due(uint32_t nowMs, bool requestAllowed, bool linkFree);
    // Outcome of the exchange for reqCmd: gotReply with the accepted frame's cmd/payload, or none
    void result(uint8_t reqCmd, bool gotReply, uint8_t replyCmd, const uint8_t* payload, uint8_t len, uint32_t nowMs);
    void frameSent(uint32_t nowMs) { lastFrame_ = nowMs; haveFrame_ = true; }   // any outgoing frame
    void restart();                                  // after an ADV Controller update: the boot check again (notice kept)
    void stateSent(uint16_t hash, uint32_t nowMs);                             // after every STATE frame
    bool wantStateResend() { bool w = resend_; resend_ = false; if (w) resendCheck_ = true; return w; }
    bool stateBlocked() const { return info_.status == ADV_CONTROLLER_INCOMPATIBLE; }
    bool noReplyNotice() { bool n = notice_; notice_ = false; return n; }
    const AdvControllerInfo& advController() const { return info_; }
    // Signal report: the last SIGNAL reply, nullptr before one (or from an ADV Controller without ADV_INFO_CAP_SIGNAL)
    const AdvSignal* signal() const { return haveSignal_ ? &signal_ : nullptr; }
    uint32_t signalAtMs() const { return signalAt_; }
    void setSignalFast(bool on) { signalFast_ = on; }   // auto input scan: SIGNAL every 500 ms even with the decoder off
    // A new read replaces one not sent yet (one on the wire finishes first); gen: the caller's id for the result
    void requestRegRead(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t gen);
    const AdvRegRead& regRead() const { return regRead_; }
    bool requestPayload(uint8_t* out, uint8_t* len) const;   // the payload of the pending request (REG_READ only)
    static bool acceptReply(uint8_t reqCmd, uint8_t reqSeq, uint8_t cmd, uint8_t seq) {
        return cmd != ADV_CMD_NAK && cmd == (uint8_t)(reqCmd | ADV_RESP) && seq == reqSeq;
    }

    static const uint32_t kSilenceMs = 50, kTrySpacingMs = 100, kBootPingEveryMs = 200, kBootWindowMs = 2000,
                          kInfoAfterStateMs = 50, kInfoPeriodMs = 10000, kIncompatiblePeriodMs = 30000,
                          kSlowPingMs = 15000, kSignalPeriodMs = 500, kSignalIdlePeriodMs = 2000;
    static const uint8_t kTries = 3, kBootMinTries = 8, kFailedCycles = 3, kMaxResends = 3;

private:
    bool signalFast_ = false;
    uint8_t start(uint8_t cmd, uint32_t now);
    AdvControllerInfo info_ = {ADV_CONTROLLER_CHECKING, false, false, {}};
    bool bootStarted_ = false, haveFrame_ = false, infoNow_ = false, infoAfterState_ = false;
    bool resend_ = false, notice_ = false, noticeGiven_ = false, haveHash_ = false, resendCheck_ = false;
    void enterNoReply(uint32_t now);
    uint32_t bootStart_ = 0, lastFrame_ = 0, lastPing_ = 0, lastInfo_ = 0, stateAt_ = 0, lastTry_ = 0;
    uint8_t bootTries_ = 0, pending_ = 0, tries_ = 0, failedCycles_ = 0, resends_ = 0, checkInfoFails_ = 0;
    uint16_t sentHash_ = 0;
    AdvSignal signal_ = {};
    bool haveSignal_ = false;
    uint32_t lastSignal_ = 0, signalAt_ = 0;
    bool signalStarted_ = false;
    AdvRegRead regRead_ = {};
    AdvRegRead nextRead_ = {};                       // asked while one was on the wire: sent after it
    bool haveNext_ = false;
    void finishRegRead(uint8_t state);
};
