#include "adv-updater.h"
#include <string.h>

void AdvUpdater::start(uint32_t size, uint32_t crc, const char* version, uint32_t now) {
    size_ = size;
    crc_ = crc;
    strncpy(version_, version ? version : "", sizeof(version_) - 1);
    offset_ = 0;
    restarts_ = stall_ = 0;
    err_ = "";
    enter(AU_BEGIN, now);
}

bool AdvUpdater::missed(uint32_t now) {
    if (tries_ == 0) missAt_ = now;
    return ++tries_ >= kTries && now - missAt_ >= kRetryWindowMs;
}

void AdvUpdater::restartOrFail() {
    if (restarts_++ >= 1) { fail("transfer"); return; }
    offset_ = 0;
    stall_ = 0;
    st_ = AU_BEGIN;
    tries_ = 0;
}

uint8_t AdvUpdater::percent() const {
    if (st_ == AU_SEND) return (uint8_t)(size_ ? (uint64_t)offset_ * 100 / size_ : 0);
    if (st_ >= AU_COMMIT && st_ <= AU_DONE) return 100;
    return 0;
}

bool AdvUpdater::next(uint32_t now, uint8_t& cmd, uint8_t* payload, uint8_t& len, ImageRead read, void* ctx) {
    len = 0;
    switch (st_) {
        case AU_BEGIN:
            cmd = ADV_CMD_UPD_BEGIN;
            len = (uint8_t)advUpdBeginPack(size_, crc_, version_, payload);
            break;
        case AU_WAIT_READY:
            if (now - phaseAt_ >= kEraseTimeoutMs && sends_ >= kMinPolls) { fail("erase"); return false; }
            if (now - lastReq_ < kStatusEveryMs) return false;
            cmd = ADV_CMD_UPD_STATUS;
            break;
        case AU_SEND: {
            uint8_t n = (uint8_t)(size_ - offset_ < ADV_UPD_CHUNK ? size_ - offset_ : ADV_UPD_CHUNK);
            uint8_t chunk[ADV_UPD_CHUNK];
            read(offset_, chunk, n, ctx);
            cmd = ADV_CMD_UPD_DATA;
            len = (uint8_t)advUpdDataPack(offset_, chunk, n, payload);
            sentOffset_ = offset_;
            sentLen_ = n;
            break;
        }
        case AU_COMMIT:
            cmd = ADV_CMD_UPD_COMMIT;
            break;
        case AU_REBOOT_WAIT:
            if (now - phaseAt_ >= kRebootDelayMs + kRebootTimeoutMs && sends_ >= kMinPolls) { fail("reboot"); return false; }
            if (now - phaseAt_ < kRebootDelayMs || now - lastReq_ < kPingEveryMs) return false;
            cmd = ADV_CMD_PING | ADV_WANT_REPLY;
            break;
        case AU_VERIFY:
            cmd = ADV_CMD_INFO | ADV_WANT_REPLY;
            break;
        default:
            return false;
    }
    lastCmd_ = cmd;
    lastReq_ = now;
    if (sends_ < 255) sends_++;
    return true;
}

void AdvUpdater::result(bool got, uint8_t replyCmd, const uint8_t* p, uint8_t len, uint32_t now) {
    bool ok = got && replyCmd == (uint8_t)(lastCmd_ | ADV_RESP);
    uint8_t st = 0;
    uint32_t next = 0;
    bool upd = ok && advUpdReplyUnpack(p, len, &st, &next);
    switch (st_) {
        case AU_BEGIN:
            if (upd && st == ADV_UPD_OK) { enter(AU_WAIT_READY, now); lastReq_ = now; }
            else if (upd && st == ADV_UPD_TOO_BIG) fail("begin");
            else if (missed(now)) fail("begin");
            return;
        case AU_WAIT_READY:                               // a missing reply while erasing is normal
            if (!upd) return;
            if (st == ADV_UST_READY || st == ADV_UST_RECEIVING) { enter(AU_SEND, now); offset_ = next; stall_ = 0; }
            else if (st == ADV_UST_ERROR || st == ADV_UST_IDLE) fail("erase");
            return;
        case AU_SEND: {
            if (!upd) { if (missed(now)) fail("transfer"); return; }
            tries_ = 0;
            uint32_t before = offset_;
            if (st == ADV_UPD_OK && next == sentOffset_ + sentLen_) offset_ = next;
            else if (st == ADV_UPD_RESUME && next >= offset_ && next <= size_) offset_ = next;
            else { restartOrFail(); return; }             // NO_SESSION, or the ADV Controller went backwards
            if (offset_ > before) stall_ = 0;
            else if (++stall_ >= kMaxStall) { restartOrFail(); return; }   // RESUME at the same offset forever
            if (offset_ >= size_) enter(AU_COMMIT, now);
            return;
        }
        case AU_COMMIT:
            if (!upd) {                                   // the copy may already run: the build id decides
                if (missed(now)) { enter(AU_REBOOT_WAIT, now); lastReq_ = 0; }
                return;
            }
            if (st == ADV_COMMIT_CRC_OK) { enter(AU_REBOOT_WAIT, now); lastReq_ = 0; }
            else if (st == ADV_COMMIT_CRC_BAD) fail("crc");
            else if (st == ADV_COMMIT_HEADER_BAD) fail("header");
            else if (st == ADV_COMMIT_NOT_COMPLETE && next < size_) { enter(AU_SEND, now); offset_ = next; stall_ = 0; }
            else restartOrFail();
            return;
        case AU_REBOOT_WAIT:
            if (ok && len >= 1 && p[0] == ADV_PROTOCOL) enter(AU_VERIFY, now);
            return;
        case AU_VERIFY: {
            AdvInfo i;
            if (ok && advInfoUnpack(p, len, &i)) {
                if (i.buildId == crc_) st_ = AU_DONE;
                else fail("build");
            } else if (missed(now)) {
                fail("reboot");
            }
            return;
        }
        default:
            return;
    }
}
