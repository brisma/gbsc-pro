// Draws the menu engine's view on the STV9426: list layout, info panel and overlays
#pragma once
#include <stdint.h>
#include "menu-view.h"
#include "../drivers/stv9426-driver.h"
#include "marquee.h"

class OsdRenderer {
public:
    explicit OsdRenderer(Stv9426& d);
    // Applies the layout every call (the driver de-duplicates), redraws the cells when the view, the theme or the
    // output changed or the layout had to be rewritten; requests power while a view is open, powers off on close
    void render(const MenuView& v, uint8_t theme, uint32_t nowMs);
    static bool rowFits(const char* label, const char* value, uint8_t kind);   // list row without clipping
    // osd.position (menu, info, keyboard, update, quick slider, toasts over the menu) and osd.popup_position
    // (mute/volume, status pop-up, toasts with the menu closed):
    // 0..8 = Top Left .. Bottom Right; Screen adjust takes the bottom row of the menu's column
    void setPlacement(uint8_t pos, uint8_t popup) { pos_ = pos < 9 ? pos : 4; popup_ = popup < 9 ? popup : 7; }

private:
    enum : uint8_t { LAY_NONE, LAY_LIST, LAY_INFO, LAY_PILL, LAY_LOW, LAY_UPDATE, LAY_CENTER, LAY_STATUS };
    bool applyLayout(uint8_t lay, uint8_t rows, uint8_t vrow);   // vrow 0 top, 1 centre, 2 bottom
    void blankRows(uint8_t role);
    uint8_t put(uint8_t row, uint8_t col, const char* s, uint8_t attr, uint8_t lastCol = 28);
    void putCode(uint8_t row, uint8_t col, uint8_t code, uint8_t attr);
    void drawChoice(uint8_t row, uint8_t choice, uint8_t base);
    void fillRow(uint8_t row, uint8_t attr, uint8_t from = 0, uint8_t to = 30);
    void putRight(uint8_t row, const char* s, uint8_t attr, uint8_t endCol = 28);
    void drawList(const MenuView& v);
    void drawScrollbar(const MenuView& v);
    void drawItem(uint8_t row, const MenuViewRow& r);
    void drawHelp(const MenuView& v);
    void drawInfo(const MenuView& v);
    void drawPill(const MenuView& v);
    void drawStatus(const MenuView& v);
    void drawLow(const MenuView& v);
    void drawUpdate(const MenuView& v);
    void drawKeyboard(const MenuView& v);
    void drawBar(uint8_t row, uint8_t col, uint8_t cells, int halves, uint8_t attr);
    uint8_t a(uint8_t role) const;
    // Scrolling text (marquee.h): offset of text in visible cells, restarting when the text changes
    uint16_t scroll(MarqueeState& s, const char* text, uint16_t visible);
    uint16_t scrollSignature(const MenuView& v);          // the offsets the next draw will use
    static uint8_t profileLabelCells(const MenuViewRow& r);   // cells left of the value on a ROWK_PROFILE row

    Stv9426& d_;
    uint16_t rev_ = 0xFFFF;
    uint8_t theme_ = 0xFF;
    uint8_t pos_ = 4, popup_ = 7;         // osd.position / osd.popup_position
    uint8_t place_ = 0xFF;                // placement of the last drawn screen
    uint8_t col_ = 1;                     // horizontal column of the current screen (0 left, 1 centre, 2 right)
    uint8_t out_ = 0xFF;
    bool open_ = false;
    uint16_t gen_ = 0;                    // driver generation after this renderer's last draw
    uint32_t now_ = 0;
    MarqueeState helpScroll_ = {0, 0}, rowScroll_ = {0, 0}, titleScroll_ = {0, 0};
    static uint8_t titleCells(const MenuView& v);      // cells for the title left of the position
    uint16_t scrollSig_ = 0;
};
