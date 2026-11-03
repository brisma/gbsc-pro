// Request admission at header time (pure, host-tested). With 4+ parallel clients the free heap (~16 KB idle)
// runs out and the ESP aborts in operator new (no exceptions on the ESP8266). A request is refused
// before its handler allocates anything: 503 + Retry-After while memory is short (with hysteresis) or too many
// HTTP connections are open; a firmware upload runs alone. The 503 itself is a ~200 B response.
// Connections are counted from the lwIP list by the caller: a per-request counter leaked under load (some
// requests are destroyed without their disconnect callback).
#pragma once
#include <stdint.h>
#include "../update/update-source.h"

enum AdmitVerdict : uint8_t { ADMIT, BUSY_LOAD, BUSY_MEMORY, BUSY_UPDATE, BUSY_STARTING };

class Admission {
public:
    static const uint32_t kHeapLow = 10000, kHeapOk = 12000;     // enter / leave the low-memory state
    // largest free block: the allocations that failed were 136 B and 1.5 KB; TIME-WAIT connections fragment the
    // heap for ~2 min after a burst (14.5 KB free, 5 KB block), so a high block threshold would never clear
    static const uint32_t kBlockLow = 1536, kBlockOk = 2560;
    static const uint32_t kUploadBlock = 5120;    // Updater 4 KB flash buffer + margin (no signature check)
    static const uint8_t kMaxConnections = 3;     // open HTTP connections, the new one included
    // updateBusy: a firmware upload is receiving, verifying or restarting; isUpload: POST /api/system/update;
    // starting: setup() has not mounted LittleFS and read the settings yet (the server answers before that)
    AdmitVerdict decide(uint32_t freeHeap, uint32_t maxBlock, uint8_t connections, bool updateBusy, bool isUpload,
                        bool starting = false);
    bool low() const { return low_; }
private:
    bool low_ = false;
};
// decide()'s updateBusy for a running update: an upload refuses every other request; an OTA pull lets reads (GET)
// through but not changes (settings, actions, restore: scaler I2C and LittleFS writes while flashing)
inline bool admitUpdateBusy(UpdateSource src, bool isGet) { return src == US_UPLOAD || (src == US_PULL && !isGet); }

// TCP accept guard (net-stats.cpp hooks the lwIP accept of port 80): a new connection is reset before anything is
// allocated for it when more than kTcpMaxConnections are open (the new one included) or the heap is nearly gone.
// Every accepted connection costs heap (client, request, buffers) before admission can refuse it: 8 looping
// clients still rebooted the ESP now and then.
const uint8_t kTcpMaxConnections = 6;
const uint32_t kTcpAcceptHeap = 6000;
inline bool admitTcp(uint32_t freeHeap, uint8_t connections) {
    return freeHeap >= kTcpAcceptHeap && connections <= kTcpMaxConnections;
}
