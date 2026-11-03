// OTA over plain HTTP, the pure parts (host-tested):
// channels, URLs, the recommended and manual offers, the daily clock, the pull sequencer and its verdicts
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "update-manifest.h"

enum OtaChannel : uint8_t { OTA_STABLE = 0, OTA_BETA = 1 };
static const uint8_t OTA_CHANNELS = 2;

const char* otaChannelName(uint8_t ch);    // "stable" | "beta" (URL and API), "" when out of range
const char* otaChannelLabel(uint8_t ch);   // "Stable" | "Beta" (menus)
int otaChannelParse(const char* s);        // the name back to the channel, -1 when unknown

// "<base><channel>/manifest.txt" and "<base><file>"; false when out does not hold it or the channel is unknown
bool otaManifestUrl(const char* base, uint8_t ch, char* out, size_t n);
bool otaImageUrl(const char* base, const char* file, char* out, size_t n);
// What a check reads: the build channel first, then each more stable one (Beta: beta, stable)
uint8_t otaChannelsToRead(uint8_t buildChannel, uint8_t out[OTA_CHANNELS]);

struct OtaOffer {
    bool any;        // an install is offered
    uint8_t channel; // the channel the build comes from
    bool older;      // lower than the running version (manual offers only)
    bool moves;      // another channel than the running build's: the device follows it after the install
    Manifest m;      // what the install downloads and checks
};
// Recommended update: feed each manifest read, then finish (offered only when newer than the running version)
void otaConsider(OtaOffer& best, uint8_t channel, const Manifest& m);
void otaFinishRecommended(OtaOffer& best, const char* running, uint8_t buildChannel);
// The offer after a check: a check that read no manifest at all (an outage) keeps the previous one
inline OtaOffer otaAfterCheck(const OtaOffer& previous, const OtaOffer& fresh, uint8_t manifestsRead) {
    return manifestsRead ? fresh : previous;
}
// Other channels: any build that differs from the running one, flagged older/moves
OtaOffer otaManual(uint8_t channel, const Manifest& m, const char* running, uint8_t buildChannel);

// Daily check: first 2 min after boot, then every 24 h; a check that could not run (Wi-Fi down, busy, server error)
// retries after 10 min, doubling up to 2 h. millis()-wrap safe
struct OtaAutoClock {
    uint32_t next = 120000;
    uint8_t fails = 0;
    // off: the clock follows now (the first check comes 2 min after it is turned on, even after > 24.9 days off)
    bool due(uint32_t now, bool enabled) {
        if (!enabled) { next = now + 120000UL; return false; }
        return (int32_t)(now - next) >= 0;
    }
    void ran(uint32_t now, bool ok);
};

// One HTTP fetch, driven by the loop: 5 s to resolve and connect, then 10 s at most without data
enum OtaStep : uint8_t { OS_IDLE, OS_RESOLVE, OS_CONNECT, OS_BODY, OS_DONE, OS_FAILED };
struct OtaPull {
    static const uint32_t kConnectMs = 5000, kIdleMs = 10000;
    void start(uint32_t now) { state_ = OS_RESOLVE; startAt_ = lastAt_ = now; reason_ = ""; }
    // parser: 0 more, > 0 complete, < 0 failed (the HTTP answer was wrong)
    OtaStep step(uint32_t now, bool resolved, bool connected, bool gotData, bool closed, int parser);
    OtaStep state() const { return state_; }
    const char* reason() const { return reason_; }   // "timeout", "short", "http" when OS_FAILED
private:
    OtaStep state_ = OS_IDLE;
    uint32_t startAt_ = 0, lastAt_ = 0;
    const char* reason_ = "";
};

// ESPAsyncTCP's AsyncClient::connect() takes a new lwIP pcb and loses it when tcp_connect() fails: no route
// (ERR_RTE, Wi-Fi gone during the DNS step) or no memory for the SYN (ERR_MEM). The fetch rules both out first:
// the failure reason ("connect", "memory"), or nullptr to connect
static const uint32_t OTA_CONNECT_MIN_BLOCK = 2048;   // largest free heap block; the SYN needs a few hundred bytes
const char* otaConnectBlocked(bool haveRoute, uint32_t maxFreeBlock);

// Lifetime of the one AsyncClient. ESPAsyncTCP sets the client's pcb only once connected, so a client that has not
// reported its connect or an error must not be deleted: lwIP still calls it back. It becomes a zombie, and is
// deleted after its late connect (closed by the caller) or error
struct OtaClientLife {
    void created() { exists_ = true; connected_ = ended_ = zombie_ = false; }
    void connected() { connected_ = true; }
    void ended() { ended_ = true; }                           // error, disconnect or timeout reported
    bool finish() { if (connected_ || ended_) return true; zombie_ = true; return false; }   // true: delete now
    bool zombie() const { return zombie_; }
    bool deletable() const { return exists_ && (connected_ || ended_); }
    void deleted() { exists_ = connected_ = ended_ = zombie_ = false; }
private:
    bool exists_ = false, connected_ = false, ended_ = false, zombie_ = false;
};

// Image checks after the body: nullptr = good, else "format" (not a gzip image), "size", "sha"
const char* otaVerdict(uint32_t contentLength, uint32_t expectSize, const uint8_t* sha, const uint8_t* expectSha,
                       bool gzipKind);
// A reason code ("no_wifi", "timeout", ...) as words the TV OSD ROM can draw, at most 14 characters
const char* otaReasonWords(const char* code);
// "Not installed: <words>" (<= 28, OSD ROM characters) for a refused or failed install: the old firmware runs on
void otaInstallFailText(const char* code, char* out, size_t n);
// System > Update > Last result from the stored result ("" / "none" / a version / "error:<code>"): "Not checked",
// "Up to date", the offered version or the reason words with a capital (<= 14, OSD ROM characters), as the web app
void otaLastResultText(const char* last, char* out, size_t n);
// Menu texts (OSD characters): the Offer row "<Label>[ older][ switch]" (<= 24) and the Install confirm question
// "Install an older version?" / "Move to the <Label> channel?" (<= 28; "" = the node's own question)
void otaOfferText(const OtaOffer& o, char* out, size_t n);
void otaConfirmText(const OtaOffer& o, char* out, size_t n);
// A check result goes to the TV only when the user asked, or when an update was found (the daily check stays quiet)
inline bool otaToastWanted(bool fromUser, bool available) { return fromUser || available; }
