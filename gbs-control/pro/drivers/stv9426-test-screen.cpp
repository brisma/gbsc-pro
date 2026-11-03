// Developer test screen for the STV9426 (pure; drawn by the dev.osd_test action)
#include <stdio.h>
#include <string.h>
#include "stv9426-driver.h"
#include "stv9426-udc.h"
using namespace stv;

static const char* const kStdNames[OUT_STD_COUNT] = {"480p", "576p", "720p60", "720p50", "960p60", "960p50",
                                                     "1024p60", "1024p50", "1080p60", "1080p50", "15k60",
                                                     "15k50", "bypass", "1200p60", "1200p50"};

const char* stvOutStdName(StvOutStd s) { return s < OUT_STD_COUNT ? kStdNames[s] : "?"; }

static void text(StvShadow& sh, uint8_t row, uint8_t col, const char* s, uint8_t attr) {
    for (; *s && col < kRowWidth; s++, col++) {
        uint8_t a = attr;
        if (*s == ' ') a = (uint8_t)((attr & 0xF0) | ((attr >> 4) & 0x07));
        sh.set(row, col, stvCharCode(*s), a);
    }
}

void stvDrawTestScreen(Stv9426& d) {
    const StvTiming& t = d.timing();
    Layout l;
    l.count = 0;
    l.strips[l.count++] = {STRIP_SPACE, t.topMenu, false};
    l.strips[l.count++] = {STRIP_ROW, 0, t.zoomMenu};
    l.strips[l.count++] = {STRIP_SPACE, 4, false};
    for (uint8_t r = 1; r <= 7; r++) l.strips[l.count++] = {STRIP_ROW, r, t.zoomMenu};
    l.strips[l.count++] = {STRIP_SPACE, 4, false};
    l.strips[l.count++] = {STRIP_ROW, 8, t.zoomMenu};
    d.setLayout(l, t.chMenu);

    StvShadow& sh = d.shadow();
    const uint8_t base = 0x17 /* white on blue */;
    char buf[32];
    for (uint8_t r = 0; r < kRows; r++) {
        sh.setWidth(r, kRowWidth);
        sh.fill(r, 0, kRowWidth, 0x20, 0x11 /* blue on blue */);
    }
    snprintf(buf, sizeof(buf), "STV9426 TEST 9x30 %s", stvOutStdName(d.output()));
    text(sh, 0, 0, buf, base);
    for (uint8_t r = 1; r <= 7; r++) {
        uint8_t bg = (uint8_t)((r + 4) & 7);
        uint8_t attr = (uint8_t)((bg << 4) | r);
        sh.fill(r, 0, kRowWidth, 0x20, (uint8_t)((bg << 4) | bg));
        snprintf(buf, sizeof(buf), "ROW %u FG %u BG %u", r, r, bg);
        text(sh, r, 0, buf, attr);
    }
    d.glyphFrame();                                      // the first glyphs that fit the dynamic slots
    for (uint8_t n = 0; n < kUdcSlots; n++)
        sh.set(1, (uint8_t)(kRowWidth - kUdcSlots + n), d.glyph(n), (uint8_t)((5 << 4) | 1));
    text(sh, 2, 15, "0123456789:+-./", (uint8_t)((6 << 4) | 2));
    static const uint8_t kBars[5] = {0x11, 0x12, 0x2C, 0x2D, 0x3F};
    for (uint8_t i = 0; i < 5; i++) sh.set(3, (uint8_t)(20 + i), kBars[i], (uint8_t)((7 << 4) | 3));
    static const uint8_t kRomArrows[4] = {0x05, 0x15, 0x06, 0x16};   // ◀ ▶ ▲ ▼ (boxed in the ROM)
    for (uint8_t i = 0; i < 4; i++) sh.set(3, (uint8_t)(26 + i), kRomArrows[i], (uint8_t)((7 << 4) | 2));
    text(sh, 8, 0, "Close: any key", base);
    d.flush();
}
