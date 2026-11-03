#include "menu-keyboard.h"
#include <string.h>
#include <stdio.h>
#include "menu-types.h"
#include "../profiles/profile-names.h"
#include "../api/api-guard.h"

static const char kDigits[] = "0123456789-./,";

uint8_t MenuKeyboard::rowLength(uint8_t r) const { return r < 2 ? 13 : r == 2 ? (allowed(',') ? 14 : 13) : 6; }

uint8_t MenuKeyboard::maxLen() const {
    switch (purpose_) {
        case KB_OTA_PASSWORD: return 32;
        case KB_EXTRA_HOSTS: return 63;
        default: return PROFILE_NAME_MAX;
    }
}

void MenuKeyboard::open(uint8_t purpose, const char* initial) {
    purpose_ = purpose;
    snprintf(text_, (size_t)maxLen() + 1, "%s", initial ? initial : "");
    cur_ = (uint8_t)strlen(text_);
    row_ = col_ = 0;
    upper_ = true;
    done_ = false;
}

char MenuKeyboard::keyChar(uint8_t r, uint8_t c) const {
    if (r < 2) return (char)((upper_ ? 'A' : 'a') + r * 13 + c);
    if (r == 2) return kDigits[c];
    return 0;
}

void MenuKeyboard::rowText(uint8_t r, char* out, size_t n) const {
    if (!n) return;
    if (r == 3) { snprintf(out, n, "aA < > Space Del Done"); return; }
    size_t i = 0;
    for (uint8_t c = 0; c < rowLength(r) && i + 1 < n; c++) out[i++] = keyChar(r, c);
    out[i] = '\0';
}

bool MenuKeyboard::allowed(char c) const {
    if (c == ',') return purpose_ == KB_EXTRA_HOSTS;            // profile names and passwords: no comma
    return true;
}

void MenuKeyboard::insert(char c) {
    size_t len = strlen(text_);
    if (len >= maxLen()) return;
    memmove(text_ + cur_ + 1, text_ + cur_, len - cur_ + 1);   // the terminator moves too
    text_[cur_++] = c;
}

void MenuKeyboard::press() {
    size_t len = strlen(text_);
    if (row_ == 3) {
        switch (col_) {
            case SP_CASE: upper_ = !upper_; break;
            case SP_LEFT: if (cur_) cur_--; break;
            case SP_RIGHT: if (cur_ < len) cur_++; break;
            case SP_SPACE: insert(' '); break;
            case SP_DEL:                                        // the character left of the cursor
                if (cur_) { memmove(text_ + cur_ - 1, text_ + cur_, len - cur_ + 1); cur_--; }
                break;
            case SP_DONE: done_ = true; break;
        }
        return;
    }
    char c = keyChar(row_, col_);
    if (allowed(c)) insert(c);
}

void MenuKeyboard::key(uint8_t k) {
    uint8_t len = rowLength(row_);
    switch (k) {
        case K_LEFT: col_ = (uint8_t)((col_ + len - 1) % len); break;
        case K_RIGHT: col_ = (uint8_t)((col_ + 1) % len); break;
        case K_UP: case K_DOWN: {
            uint8_t to = (uint8_t)((row_ + (k == K_UP ? kRows - 1 : 1)) % kRows);
            uint8_t tl = rowLength(to);
            if (to == 3) col_ = (uint8_t)(col_ * tl / len >= tl ? tl - 1 : col_ * tl / len);   // proportional
            else if (row_ == 3) col_ = col_ == SP_DONE ? (uint8_t)(tl - 1) : (uint8_t)(col_ * tl / len);
            else if (col_ >= tl) col_ = (uint8_t)(tl - 1);
            row_ = to;
            break;
        }
        case K_ROT_R:
            if (++col_ >= len) { col_ = 0; row_ = (uint8_t)((row_ + 1) % kRows); }
            break;
        case K_ROT_L:
            if (col_ == 0) { row_ = (uint8_t)((row_ + kRows - 1) % kRows); col_ = (uint8_t)(rowLength(row_) - 1); }
            else col_--;
            break;
        case K_OK: case K_CLICK: press(); break;
        case K_LONG: done_ = true; break;
        default: break;
    }
}

bool MenuKeyboard::valid() const {
    switch (purpose_) {
        case KB_PROFILE_NEW: case KB_RENAME: {
            char t[PROFILE_NAME_MAX + 1];
            return profileNameSanitize(text_, t, sizeof(t));
        }
        case KB_EXTRA_HOSTS: return guardExtraHostsValid(text_);   // the same check as every other writer
        default: return true;                                   // OTA password: keyboard characters only
    }
}

uint8_t MenuKeyboard::window(uint8_t cells, char* out, size_t n) const {
    if (!n || !cells) { if (n) out[0] = '\0'; return 0; }
    size_t keep = cells - 1;                                    // the cursor cell may be the one after the text
    size_t start = cur_ > keep ? cur_ - keep : 0;
    size_t w = strlen(text_ + start);
    if (w > keep) w = keep;
    if (w + 1 > n) w = n - 1;
    memcpy(out, text_ + start, w);
    out[w] = '\0';
    return (uint8_t)(cur_ - start);
}
