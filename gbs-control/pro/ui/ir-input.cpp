#include "ir-input.h"
#include "menu-types.h"

int irKeyFor(uint32_t code) {
    switch (code) {
        case IR_CODE_UP: return K_UP;
        case IR_CODE_DOWN: return K_DOWN;
        case IR_CODE_LEFT: return K_LEFT;
        case IR_CODE_RIGHT: return K_RIGHT;
        case IR_CODE_OK: return K_OK;
        case IR_CODE_MENU: return K_MENU;
        case IR_CODE_EXIT: return K_BACK;
        case IR_CODE_INFO: return K_INFO;
        case IR_CODE_SAVE: return K_SAVE;
        case IR_CODE_MUTE: return K_MUTE;
        case IR_CODE_VOLUP: return K_VOLUP;
        case IR_CODE_VOLDN: return K_VOLDOWN;
        default: return -1;
    }
}

int IrRepeater::feed(uint32_t code, bool isRepeat, uint32_t now, bool& repeat) {
    repeat = false;
    if (!isRepeat) {
        key_ = irKeyFor(code);            // unknown codes clear the held key
        start_ = last_ = frame_ = now;
        return key_;
    }
    if (key_ >= 0 && now - frame_ > kGapMs) key_ = -1;   // a repeat after a gap belongs to another press/remote
    frame_ = now;
    if (key_ < 0) return -1;
    // Only arrows and volume auto-repeat: a long MENU/SAVE/OK/EXIT press must act once
    if (key_ != K_UP && key_ != K_DOWN && key_ != K_LEFT && key_ != K_RIGHT && key_ != K_VOLUP && key_ != K_VOLDOWN) return -1;
    uint32_t held = now - start_;
    uint32_t interval = held >= 4000 ? 31 : held >= 2500 ? 62 : 125;
    if (now - last_ < interval) return -1;
    last_ = now;
    repeat = true;
    return key_;
}
