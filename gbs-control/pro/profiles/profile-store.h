// Named profiles on LittleFS: /profiles/pNN.cfg (device only; loop context only)
#pragma once
#include <stdint.h>
#include "profile-list.h"
#include "profile-file.h"
#include "profile-names.h"

void profileStoreInit();                         // mkdir, recover tmp files, build the list, read lastused
const ProfileList& profileList();
int profileActiveIndex();                        // -1 when no profile is active
bool profileModified();
// The PROFILE_REG_BYTES register buffer of a profile save (capture) or a Custom preset load, on the heap only while
// that runs (432 bytes not held for ever); p is null when not wanted or when the heap could not give it
struct ProfileRegBuf {
    uint8_t* p;
    explicit ProfileRegBuf(bool want = true);
    ~ProfileRegBuf();
    ProfileRegBuf(const ProfileRegBuf&) = delete;
    ProfileRegBuf& operator=(const ProfileRegBuf&) = delete;
};
bool profileModifiedNow();   // recomputed now when the settings changed (profileModified lags up to 1 s)

// Operations: nullptr on success, otherwise an error code ("storage_full", "limit", "io", "unknown"...)
const char* profileLoad(uint8_t index);
// applyPresets() for the running mode (a PAL source under Force 60Hz reloads as PAL), then keeps that detected mode:
// Force 60Hz's remap (1 for a PAL source) would make the syncwatcher see a format change and load it a second time
void profileApplyPresetsKeepMode();
const char* profileSave(uint8_t index);                          // overwrite with the working state
// name already sanitised; made unique here; region = profileRegionName() index written into the header
const char* profileSaveAs(const char* name, int* newIndex, uint8_t region = PROFILE_REGION_NONE);
const char* profileRename(uint8_t index, const char* name);
const char* profileDelete(uint8_t index);
const char* profileDuplicate(uint8_t index, int* newIndex);
const char* profileClearFingerprint(uint8_t index);
const char* profileForgetMode(uint8_t index);   // POP_FORGET_MODE
const char* profileDefaults();                   // POP_DEFAULTS: factory Profile settings, no profile active
// POP_REPLACE: the working state into this profile and its fingerprint to the current signal; without a name
// the name and region stay, with one the region is `region` (PROFILE_REGION_NONE: none, as for a typed name)
const char* profileReplace(uint8_t index, const char* name, uint8_t region);
void profileRegsTouched();                       // a register action ran (ACTION_REGS): the active profile is Unsaved
void profileRegsClear();                         // registers back to a saved or preset state
void profileDeleteAll();                         // factory reset / restore
void profileReloadList();                        // after restore or import

// Custom preset path (applyPresets)
bool profileLoadRegs(uint8_t gbsMode, uint8_t* out432);   // registers of the active profile for this mode
// Base preset stored in profile id's file (PROFILE_BASE_NONE when unknown); readable before profileStoreInit
uint8_t profileStoredBase(const char* id);
uint16_t profileChoiceSeq();                       // bumps on every Load or Defaults done (by hand or by auto-load)
uint8_t profileBasePreset();                              // built-in preset to fall back to (0 = 960p)

// One queue for the web, OSD and OLED: operations run in profileOpsLoop(); result as s:status.profile_op
enum ProfileOpKind : uint8_t {
    POP_LOAD, POP_SAVE, POP_SAVEAS, POP_SAVEAS_GENERATED, POP_RENAME, POP_DELETE, POP_DUPLICATE, POP_CLEAR_FP,
    POP_IMPORT,   // file at PROFILE_IMPORT_TMP
    POP_FORGET_MODE,   // Reset geometry: drop the running mode's registers, reload the preset
    POP_DEFAULTS,      // Load > Defaults
    POP_REPLACE,       // Manage > Replace with current, same-signal "Replace it"
};
#define PROFILE_IMPORT_TMP "/tmp-import.txt"
const char* profileImport(const char* path);   // validates and adds an exported profile file
// false when the queue is full; region is used by POP_SAVEAS
bool profileQueueOp(uint8_t kind, int8_t index, const char* name, uint8_t region = PROFILE_REGION_NONE);
uint8_t profileOpsPending();                     // queued operations not yet run
bool profileOpResultFor(uint16_t seq, char* out, size_t n);   // result of operation number seq (last 4 kept)
void profileOpsLoop();
uint16_t profileOpResultSeq();                   // bumps after every operation (web, OSD, OLED, auto-load)
void profileGeneratedNameNow(char out[PROFILE_NAME_BUF]);      // "<input> <resolution>" for the current signal

void profileTick();                              // loop(): modified tracking and status broadcasts
void profileAutoloadTick();                      // loop(): auto-load from the input fingerprint (profile-autoload-device.cpp)
uint64_t profileMatchBits();                     // bit per list index matching the signal now (refreshed each second by the tick)
// The signal's field rate in mHz when it was measured for exactly this signal, else 0 (no scaler access)
uint32_t profileSignalMilliHzKnown(uint8_t input, uint8_t mode, uint16_t lines, bool interlaced);
bool profileAutoloadIdleFor(uint16_t lines);    // the decision for this signal (VPERIOD_IF lines) was taken (status pop-up timing)
void profilePath(uint8_t index, char out[24]);   // "/profiles/p07.cfg"

// Implemented in gbs-control.ino (they need the GBS register types)
void profileCaptureRegs(uint8_t* out432);        // scaler register dump
void profileReadExtras(uint8_t& adcGain);
void profileApplyAdcGain(uint8_t adcGain);
void profileResetAdcGain();                      // no fixed ADC gain (Defaults)
uint16_t profileSourceLines();                   // source lines per frame (VPERIOD_IF)
uint32_t profileSourceMilliHz();                 // source field rate in mHz, measured now (tens of ms); 0 = not measurable
bool profileSignalPresent();
