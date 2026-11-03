// TV5725 icon OSD on the standard board (upstream gbs-control's OSD): which icon a user adjustment shows, when the
// OSD can be seen, and the indicator timer. Pure; the device side is tv-osd-device.cpp.
#pragma once
#include <stdint.h>
#include "../settings/settings-request.h"

// Same values as OSDIcon in OSDManager.h (Hue = 2 and Volume = 3 are not used)
enum : uint8_t { TVO_BRIGHTNESS = 0, TVO_CONTRAST = 1, TVO_MOVE_Y = 4, TVO_MOVE_X = 5, TVO_SCALE_Y = 6, TVO_SCALE_X = 7,
                 TVO_NONE = 0xFF };
// TV OSD mode navigation (menu engine -> backend)
enum : uint8_t { TVOSD_OPEN, TVOSD_LEFT, TVOSD_RIGHT, TVOSD_ENTER, TVOSD_CLOSE };

uint8_t tvOsdIconForSetting(uint8_t sid);                          // picture.brightness / contrast, else TVO_NONE
uint8_t tvOsdIconForWrite(const SettingsPair* pairs, uint8_t n);   // a one-pair user write only (batches: none)
uint8_t tvOsdIconForAction(uint8_t aid);                           // screen.move_* / screen.scale_*, else TVO_NONE
int8_t tvOsdActionDir(uint8_t aid);                                // +1 right / down / wider / taller, -1 the other way
// Visible only without the STV9426 menu chip, with a source, and when the scaler draws the output (not Downscale,
// presetId low nibble 6, nor bypass, presetId 0x21 and up)
bool tvOsdAllowed(bool tvMenuChip, bool signal, uint8_t presetId);
// tv5725OsdOff() runs after every preset load and bumps a generation: true once when it changed since `seen`
bool tvOsdNeedsRearm(uint16_t& seen, uint16_t generation);

class TvOsdIndicator {
public:
    static const uint32_t kShowMs = 3000;
    void show(uint8_t icon, uint32_t now) { icon_ = icon; at_ = now; shown_ = true; }
    bool due(uint32_t now) const { return shown_ && now - at_ >= kShowMs; }
    void hide() { shown_ = false; }
    bool shown() const { return shown_; }
    uint8_t icon() const { return icon_; }
private:
    uint8_t icon_ = TVO_NONE;
    uint32_t at_ = 0;
    bool shown_ = false;
};
