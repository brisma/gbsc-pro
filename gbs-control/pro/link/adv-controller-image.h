// The ADV Controller firmware image the ESP offers to the HC32: an uploaded file in LittleFS (/adv-controller.bin,
// removed once the HC32 runs it) when there is a valid one, else the image embedded in the build (ADV_IMAGE_EMBED,
// on by default), else none. Only adv-controller-image.cpp includes the generated data.
#pragma once
#include <stdint.h>
#include <stddef.h>
class AsyncWebServerRequest;

#define ADV_IMAGE_PATH "/adv-controller.bin"
#define ADV_IMAGE_TMP_PATH "/tmp-adv-controller.bin"

uint32_t advControllerImageSize();                                // 0 when there is no image to offer
uint32_t advControllerImageCrc();                                 // build id (image CRC32, header field zeroed)
const char* advControllerImageVersion();
const char* advControllerImageSource();                           // "file" | "embedded" | "none"
void advControllerImageRead(uint32_t offset, uint8_t* out, uint8_t n);   // loop only (a file read)
void advControllerImageClose();                                   // after an update: the file handle goes

// loop(): the first pass checks the stored file (CRC over the whole file), a finished upload replaces it, an
// abandoned one is dropped; the file is removed once the HC32 reports its build (ok + haveInfo, no update running)
void advControllerImageLoop(bool controllerOk, uint32_t runningBuild, bool updating);

// POST /api/system/adv-image (multipart, one file). The chunks are checked as they arrive (header, length, CRC) and
// written to a temp file; finish answers 200 {"ok":1,"version":..,"build":..} (the loop then swaps it in) or
// 400 format|size|crc, 403, 409 busy, 413 space as {"error":..}
void advImageUploadChunk(AsyncWebServerRequest* req, size_t index, const uint8_t* data, size_t len, bool final);
int advImageUploadFinish(AsyncWebServerRequest* req, char* json, size_t n);
bool advImageUploadActive();   // an upload or its swap: no ADV Controller update starts meanwhile
