// IR remote input (pure): NEC codes of the 12 keys and held-key repeat with acceleration
#pragma once
#include <stdint.h>

enum : uint32_t {
    IR_CODE_UP = 0xEA52C03F, IR_CODE_DOWN = 0xEA5220DF, IR_CODE_LEFT = 0xEA52807F, IR_CODE_RIGHT = 0xEA5240BF,
    IR_CODE_OK = 0xEA52A05F, IR_CODE_MENU = 0xEA52609F, IR_CODE_EXIT = 0xEA52E01F, IR_CODE_INFO = 0xEA5210EF,
    IR_CODE_SAVE = 0xEA5200FF, IR_CODE_MUTE = 0xEA52D02F, IR_CODE_VOLUP = 0xEA52906F, IR_CODE_VOLDN = 0xEA5250AF };

int irKeyFor(uint32_t code);   // MenuKey, -1 for other codes

class IrRepeater {
public:
    // One decoded frame (code ignored for repeats). Returns the MenuKey to send or -1; `repeat` is true for held
    // keys. Only arrows and volume repeat: every 125 ms, every 62 ms after 2.5 s held, every 31 ms after 4 s.
    int feed(uint32_t code, bool isRepeat, uint32_t nowMs, bool& repeat);
    void clear() { key_ = -1; }
    static const uint32_t kGapMs = 150;   // no frame for this long: the held key is over (NEC repeats every 108 ms)
private:
    int key_ = -1;
    uint32_t start_ = 0, last_ = 0, frame_ = 0;
};
