#include "oled-splash.h"
#include <stdio.h>
#include "oled-splash-art.h"

void oledDrawSplash(OLEDDisplay& d, const char* version) {
    static const char kHost[] = "https://github.com";
    static const char kPath[] = "/brisma";
    char ver[24];
    snprintf(ver, sizeof(ver), "v%s", version);
    d.clear();
    d.setColor(WHITE);
    d.drawXbm(0, 0, gbsicon_width, gbsicon_height, gbsicon_bits);
    d.setFont(ArialMT_Plain_10);
    // "by Brisma" centred under "GBSC"; a long version (right-aligned) pushes it left so they keep 4 px apart, and a
    // channel build too long for both (e.g. "3.10.10-beta999") shows the version alone (the link names the author)
    static const char kBy[] = "by Brisma";
    int16_t wBy = (int16_t)d.getStringWidth(kBy), verLeft = (int16_t)(125 - d.getStringWidth(ver));
    int16_t xBy = (int16_t)(gbsicon_name_cx - wBy / 2);
    if (xBy + wBy + 4 > verLeft) xBy = (int16_t)(verLeft - 4 - wBy);
    d.setTextAlignment(TEXT_ALIGN_LEFT);
    if (xBy >= 2) d.drawString(xBy, 38, kBy);
    d.setTextAlignment(TEXT_ALIGN_RIGHT);
    d.drawString(125, 38, ver);
    d.setTextAlignment(TEXT_ALIGN_LEFT);
    // "/brisma" 1 px after the host: in ArialMT_Plain_10 the slash would touch the "m" of "com"
    int16_t wHost = (int16_t)d.getStringWidth(kHost);
    int16_t x = (int16_t)(64 - (wHost + 1 + (int16_t)d.getStringWidth(kPath)) / 2);
    d.drawString(x, 51, kHost);
    d.drawString((int16_t)(x + wHost + 1), 51, kPath);
    d.display();
}
