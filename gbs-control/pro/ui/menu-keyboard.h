// On-screen keyboard: four key rows, a text field, per-purpose length and
// character rules. Pure: the engine feeds keys and reads the field; the renderers draw rows and the caret.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum : uint8_t { KB_PROFILE_NEW, KB_RENAME, KB_OTA_PASSWORD, KB_EXTRA_HOSTS };

class MenuKeyboard {
public:
    static const uint8_t kRows = 4, kMaxLen = 63;
    enum : uint8_t { SP_CASE, SP_LEFT, SP_RIGHT, SP_SPACE, SP_DEL, SP_DONE };   // special keys (row 3)

    void open(uint8_t purpose, const char* initial);   // field = initial (cut to the purpose's maximum)
    void key(uint8_t k);                                // arrows move, rotation walks, OK/click type, long = Done
    bool done() const { return done_; }
    void clearDone() { done_ = false; }
    bool valid() const;                                 // field acceptable for the purpose
    const char* text() const { return text_; }
    uint8_t row() const { return row_; }
    uint8_t col() const { return col_; }
    bool upper() const { return upper_; }
    uint8_t purpose() const { return purpose_; }
    uint8_t cursor() const { return cur_; }             // insertion point 0..len (< and > move it)
    uint8_t rowLength(uint8_t r) const;                 // the digit row has the comma key only where it is allowed
    void rowText(uint8_t r, char* out, size_t n) const; // keys of a row ("aA < > Space Del Done" for row 3)
    char keyChar(uint8_t r, uint8_t c) const;           // character of a key, 0 for the special keys
    // Up to `cells - 1` characters of the field around the cursor into out; returns the cursor cell (the cell
    // after the last character when the cursor is at the end)
    uint8_t window(uint8_t cells, char* out, size_t n) const;
    uint8_t maxLen() const;

private:
    bool allowed(char c) const;
    void press();
    void insert(char c);
    char text_[kMaxLen + 1] = {0};
    uint8_t purpose_ = KB_PROFILE_NEW, row_ = 0, col_ = 0, cur_ = 0;
    bool upper_ = true, done_ = false;
};
