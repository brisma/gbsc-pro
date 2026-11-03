// ADV presets and macros stored on the device (pure, host-tested): ADV Manager .yaml texts on LittleFS,
// /adv/presets/pNN.yaml (16, <= 8 KB) and /adv/macros/mNN.yaml (32, <= 2 KB). The device glue is adv-store-web.cpp.
// Text rules: UTF-8 allowed, no control characters but \n \r \t, lines <= 127 bytes, no line starting with '['
// (a backup section), a non-empty "name:" line.
#pragma once
#include <stdint.h>
#include <stddef.h>

static const uint8_t ADV_PRESET_MAX = 16;
static const uint8_t ADV_MACRO_MAX = 32;
static const size_t ADV_PRESET_MAX_BYTES = 8192;
static const size_t ADV_MACRO_MAX_BYTES = 2048;
static const size_t ADV_LINE_MAX = 127;
static const size_t ADV_NAME_MAX = 32;
static const size_t ADV_DESC_MAX = 64;

enum AdvKind : uint8_t { ADV_PRESET = 0, ADV_MACRO = 1 };

// Incremental check of one file (any chunking); name, description and the register or command count on the way
struct AdvYamlCheck {
    size_t max, size;
    char line[ADV_LINE_MAX + 1];
    size_t lineLen;
    bool bad;
    const char* why;              // "control", "line", "section", "size", "name" (nullptr while fine)
    char name[ADV_NAME_MAX + 1];
    char desc[ADV_DESC_MAX + 1];
    uint16_t count;               // preset register lines ('0xNN': ...) plus macro commands (- ...)
};
void advYamlBegin(AdvYamlCheck& c, size_t max);
bool advYamlFeed(AdvYamlCheck& c, const uint8_t* p, size_t n);   // false once the text breaks a rule
bool advYamlFinish(AdvYamlCheck& c);                             // the last line, then the name rule

// A save of fileSize bytes fits when the profile reserve is still free after it (whole blocks)
bool advSpaceOk(size_t freeBytes, size_t blockSize, size_t fileSize, size_t profileReserve);

inline uint8_t advMax(AdvKind k) { return k == ADV_PRESET ? ADV_PRESET_MAX : ADV_MACRO_MAX; }
inline size_t advMaxBytes(AdvKind k) { return k == ADV_PRESET ? ADV_PRESET_MAX_BYTES : ADV_MACRO_MAX_BYTES; }
int advParseId(const char* id, AdvKind k);                 // "p03" / "m12" -> index, -1 when not one
void advId(AdvKind k, uint8_t i, char out[4]);
void advPath(AdvKind k, uint8_t i, char* out, size_t n);   // /adv/presets/p03.yaml
// The upload's temp file, in the target folder: LittleFS rename creates no folder and remove drops an empty one
void advTempPath(AdvKind k, char* out, size_t n);          // /adv/presets/upload.tmp
// One upload at a time (requests by address): index 0 takes the gate, the request's end or its client leaving frees it
struct AdvUploadGate { uintptr_t owner; };
bool advGateBegin(AdvUploadGate& g, uintptr_t req);   // false while another request owns it
inline bool advGateOwns(const AdvUploadGate& g, uintptr_t req) { return g.owner && g.owner == req; }
inline bool advGateBusy(const AdvUploadGate& g) { return g.owner != 0; }
void advGateEnd(AdvUploadGate& g, uintptr_t req);     // only the owner frees it

// {"id","name","description","count","bytes"}; 0 when it does not fit
size_t advListEntryJson(const char* id, const AdvYamlCheck& c, uint32_t bytes, char* out, size_t n);
