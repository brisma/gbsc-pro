// OLED renderer: draws the engine view on the 128x64 SSD1306 through ThingPulse OLEDDisplay.
// Title bar + 5 list rows taken around the cursor from the view's 7, full-screen overlays, an idle status
// home screen while the menu is closed and, after 3 minutes without input, a screensaver with the resolution
// path that moves every 30 s.
#pragma once
#include <stdint.h>
#include "oled-display.h"
#include "menu-view.h"
#include "marquee.h"

// Signal info as up to 5 label/value rows from the engine's info lines: In, Out (resolution and rate), Lock
// (On/Free), Prof, then IP when a row is left
struct OledInfoRow { char label[8]; char value[29]; bool lock; };   // lock: the Output row ends with the icon
uint8_t oledInfoRows(const MenuView& v, OledInfoRow out[5]);
// The value as drawn beside its label: a locked Output row leaves room for the icon and falls back to
// "<lines>p <rate>" ("1920x1200 59.9Hz" -> "1200p 59.9Hz") when the whole resolution does not fit, then clips
void oledInfoShownValue(OLEDDisplay& d, const OledInfoRow& r, char* out, size_t n);

class OledRenderer {
public:
    explicit OledRenderer(OLEDDisplay& d) : d_(d) {}
    void draw(const MenuView& v, const MenuIdle& idle, bool saver, uint32_t nowMs);   // buffer only
    // Draws and sends a frame when the content changed, at most every kMinFrameMs; true when it sent one
    bool render(const MenuView& v, const MenuIdle& idle, uint32_t nowMs);
    void activity(uint32_t nowMs) { lastActivity_ = nowMs; }   // a key or encoder event
    // Label + value (+ slider) of one row within the list width, measured with the real font
    static bool rowFits(OLEDDisplay& d, const char* label, const char* value, uint8_t kind);

    static const uint32_t kSaverMs = 180000, kSaverStepMs = 30000, kMinFrameMs = 50;
    static const uint8_t kRows = 5;

private:
    void drawList(const MenuView& v);
    void drawRow(const MenuViewRow& r, int16_t y);
    void drawOverlay(const MenuView& v);
    void drawIdle(const MenuIdle& idle);
    void icon(int16_t x, int16_t y, const uint8_t* rows, uint8_t h);
    int16_t boxed(int16_t cx, int16_t y, const char* text, int16_t lo, int16_t hi);
    void drawKeyboard(const MenuView& v);
    void band(const char* title, const char* right = nullptr);   // inverted title band, rows 0..12
    // Unselected label/value row; with a scroll state a value too long for the room scrolls (loops)
    void plainRow(int16_t y, const char* label, const char* value, MarqueeState* scroll = nullptr);
    void bigValue(const char* text);                             // Plain_16, centred under the band
    void frameBar(int16_t fill108);                              // x 8..119, y 44..53, fill from x 10
    void feet(const char* a, const char* b);                     // one foot (b null) or two
    void drawSaver(const MenuIdle& idle);
    void clipped(int16_t x, int16_t y, const char* text, int16_t maxW);
    void centered(int16_t y, const char* text);
    void advanceSaver(uint32_t step);
    // Scrolling text (marquee.h), one character per step: restarts when the text changes
    uint32_t since(MarqueeState& s, const char* key) { return marqueeSince(s, key, now_); }
    uint8_t skipToFit(const char* text, int16_t maxW);   // characters to drop until the rest fits maxW
    bool scrollPossible(const MenuView& v, const MenuIdle& idle, bool saver);

    OLEDDisplay& d_;
    bool dirty_ = true;
    uint16_t rev_ = 0;
    uint8_t mode_ = 0xFF;
    bool saver_ = false;
    uint32_t step_ = 0;
    uint32_t lcg_ = 0x2545F491u;
    MenuIdle idle_ = {};
    uint32_t lastActivity_ = 0, lastFrame_ = 0;
    uint32_t now_ = 0, lastScroll_ = 0;
    MarqueeState bandScroll_ = {0, 0}, rowScroll_ = {0, 0}, homeScroll_ = {0, 0}, titleScroll_ = {0, 0},
                 infoScroll_ = {0, 0}, wifiScroll_ = {0, 0};
};
