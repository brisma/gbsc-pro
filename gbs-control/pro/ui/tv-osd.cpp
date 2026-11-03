#include "tv-osd.h"
#include "../settings/settings-registry.h"
#include "../api/api-actions.h"

uint8_t tvOsdIconForSetting(uint8_t sid) {
    if (sid == SID_pic_brightness) return TVO_BRIGHTNESS;
    if (sid == SID_pic_contrast) return TVO_CONTRAST;
    return TVO_NONE;
}

uint8_t tvOsdIconForWrite(const SettingsPair* pairs, uint8_t n) {
    return n == 1 ? tvOsdIconForSetting(pairs[0].index) : TVO_NONE;
}

uint8_t tvOsdIconForAction(uint8_t aid) {
    switch (aid) {
        case AID_scr_move_left: case AID_scr_move_right: return TVO_MOVE_X;
        case AID_scr_move_up: case AID_scr_move_down: return TVO_MOVE_Y;
        case AID_scr_scale_left: case AID_scr_scale_right: return TVO_SCALE_X;
        case AID_scr_scale_up: case AID_scr_scale_down: return TVO_SCALE_Y;
        default: return TVO_NONE;
    }
}

int8_t tvOsdActionDir(uint8_t aid) {
    switch (aid) {
        case AID_scr_move_right: case AID_scr_move_down: case AID_scr_scale_right: case AID_scr_scale_up: return 1;
        case AID_scr_move_left: case AID_scr_move_up: case AID_scr_scale_left: case AID_scr_scale_down: return -1;
        default: return 0;
    }
}

bool tvOsdAllowed(bool tvMenuChip, bool signal, uint8_t presetId) {
    return !tvMenuChip && signal && (presetId & 0x0F) != 6 && presetId < 0x21;
}

bool tvOsdNeedsRearm(uint16_t& seen, uint16_t generation) {
    if (seen == generation) return false;
    seen = generation;
    return true;
}
