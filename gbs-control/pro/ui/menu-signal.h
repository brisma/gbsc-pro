// Signal helpers for the device menu backend (pure)
#pragma once
#include <stdint.h>
#include "menu-backend.h"

// VPERIOD_IF counts about two fields' lines in the SD modes 1/2 (240p reads ~525): halve them for display
uint16_t menuSourceLines(uint8_t videoStandardInput, uint16_t vperiodIf);
// The line count the menus show: a value that stayed within 2 lines for kStableMs and that video uses (kMinLines ..
// kMaxLines: 240p has 262, 1200p ~1235); 0 (shown "--") otherwise, e.g. an unstable sync after a console power-up or
// the "218 lines" of a console switching off
// The source rates: measured kRetryMs after a source is shown with them unknown (0), then every kRetryMs until known
class MenuRatesRetry {
public:
    static const uint32_t kRetryMs = 1500;
    static const uint8_t kMaxTries = 3;   // each failed measurement blocks the loop ~2 s: then stop until a new signal
    bool due(bool noSignal, uint16_t srcHzX10, uint32_t nowMs);
    void restart() { have_ = false; }   // the shown signal changed: measure again (settling first)
private:
    uint32_t at_ = 0;
    bool have_ = false;
    uint8_t tries_ = 0;
};

class MenuLineFilter {
public:
    static const uint32_t kStableMs = 1000;
    static const uint16_t kMinLines = 230, kMaxLines = 1250;
    uint16_t feed(uint16_t lines, uint32_t nowMs);
private:
    uint16_t cand_ = 0, shown_ = 0;
    uint32_t since_ = 0;
};
// Auto formats (0, 12) from STATUS_00 (0x80 clear -> NTSC, 0x60 set -> PAL, else NTSC), else the format list
bool menuAdvIsNtsc(uint8_t format, uint8_t status00);

uint8_t menuSyncType(bool sogMode, bool csync);   // 0 HV, 1 CSync, 2 SOG (SOG sources also set the CSync flag)

struct MenuSignalKey { uint8_t input, standard; bool bypass; uint8_t deint; uint16_t lines; uint8_t preset; };   // preset: output (rates)
bool menuSignalChanged(const MenuSignalKey& a, const MenuSignalKey& b);   // lines within 2 count as unchanged

// Status pop-up: what the glue compares between two stable signals
struct StatusKey { uint8_t input; uint8_t mode; uint16_t lines; int8_t profile; bool interlaced; };
bool menuStatusKeyChanged(const StatusKey& a, const StatusKey& b);   // lines within 4 are the same signal
// osd.status_popup: 0 never, 1 a new input or profile (not a 240p/480i flip in the same game), 2 any change
bool menuStatusPopupDue(uint8_t setting, const StatusKey& before, const StatusKey& now);
// Engine-function write lists from registry defaults (Decoder reset, Reset picture)
uint8_t menuDecoderResetWrites(MenuWrite* out);                 // 12 writes: levels, Enhance and its gains, 2X
uint8_t menuPictureResetWrites(MenuWrite* out);                 // 12 writes: every Picture page setting to its default
