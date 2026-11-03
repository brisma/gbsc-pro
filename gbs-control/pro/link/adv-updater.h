// ESP side of the ADV Controller firmware update: BEGIN -> STATUS until READY -> DATA x n -> COMMIT ->
// wait for the copy and reset -> PING -> INFO build id. Pure: the device glue does the exchanges.
#pragma once
#include <stdint.h>
#include "adv-link.h"

enum AuState : uint8_t { AU_IDLE, AU_BEGIN, AU_WAIT_READY, AU_SEND, AU_COMMIT, AU_REBOOT_WAIT, AU_VERIFY, AU_DONE,
                         AU_FAILED };
typedef void (*ImageRead)(uint32_t offset, uint8_t* out, uint8_t n, void* ctx);

class AdvUpdater {
public:
    void start(uint32_t size, uint32_t crc, const char* version, uint32_t nowMs);
    bool active() const { return st_ != AU_IDLE && st_ != AU_DONE && st_ != AU_FAILED; }
    // The next request to send now (cmd with the reply bit, payload); false = nothing now
    bool next(uint32_t nowMs, uint8_t& cmd, uint8_t* payload, uint8_t& len, ImageRead read, void* ctx);
    void result(bool gotReply, uint8_t replyCmd, const uint8_t* payload, uint8_t len, uint32_t nowMs);
    uint8_t percent() const;
    AuState state() const { return st_; }
    const char* error() const { return err_; }
    void finish() { st_ = AU_IDLE; }                 // after the outcome was reported

    static const uint32_t kStatusEveryMs = 50, kEraseTimeoutMs = 5000, kRebootDelayMs = 1000,
                          kPingEveryMs = 200, kRebootTimeoutMs = 5000;
    static const uint8_t kTries = 3;
    // A missed reply fails a step only after kTries misses spanning kRetryWindowMs (ADV Controller stalls up to ~0.5 s);
    // the erase and reboot timeouts also need kMinPolls requests really sent (a blocked loop pass sends none);
    // kMaxStall replies in a row without progress end the transfer (restart once, then fail).
    static const uint32_t kRetryWindowMs = 1000;
    static const uint8_t kMinPolls = 10, kMaxStall = 8;

private:
    void fail(const char* e) { st_ = AU_FAILED; err_ = e; }
    void restartOrFail();
    bool missed(uint32_t nowMs);                     // one more missed reply: true = give up
    void enter(AuState st, uint32_t nowMs) { st_ = st; phaseAt_ = nowMs; sends_ = 0; tries_ = 0; }
    AuState st_ = AU_IDLE;
    const char* err_ = "";
    uint32_t size_ = 0, crc_ = 0, offset_ = 0, sentOffset_ = 0;
    uint8_t sentLen_ = 0, tries_ = 0, restarts_ = 0, sends_ = 0, stall_ = 0;
    uint32_t phaseAt_ = 0, lastReq_ = 0, missAt_ = 0;
    uint8_t lastCmd_ = 0;
    char version_[12] = {0};
};
