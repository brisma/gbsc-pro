#include "api-admission.h"

AdmitVerdict Admission::decide(uint32_t freeHeap, uint32_t maxBlock, uint8_t connections, bool updateBusy, bool isUpload,
                               bool starting) {
    if (starting) return BUSY_STARTING;
    if (updateBusy) return BUSY_UPDATE;
    if (low_) low_ = !(freeHeap >= kHeapOk && maxBlock >= kBlockOk);
    else low_ = freeHeap < kHeapLow || maxBlock < kBlockLow;
    if (low_) return BUSY_MEMORY;
    if (isUpload) {
        if (maxBlock < kUploadBlock) return BUSY_MEMORY;
        if (connections > 2) return BUSY_LOAD;   // the upload needs most of the heap: at most one other connection
    } else if (connections > kMaxConnections) {
        return BUSY_LOAD;
    }
    return ADMIT;
}
