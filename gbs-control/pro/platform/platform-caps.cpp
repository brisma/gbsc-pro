#include <string.h>
#include "platform-caps.h"

static uint8_t gCaps = CAPS_DEFAULT;

uint8_t platformCaps() { return gCaps; }
void platformSetCaps(uint8_t caps) { gCaps = caps; }

bool platformProbesSayPro(const PlatformProbes& p) { return p.stv.ack || p.pt.ack; }

bool platformDetectDone(const PlatformProbes& p, uint8_t round) { return platformProbesSayPro(p) || round >= 5; }

uint8_t platformCapsFrom(const PlatformProbes& p, uint8_t board) {
    bool probedPro = platformProbesSayPro(p);
    bool pro = probedPro;
    if (board == BOARD_PRO) pro = true;
    else if (board == BOARD_ORIGINAL) pro = false;
    uint8_t caps = pro ? CAPS_PRO : CAP_ORIGINAL;
    if (!pro && !probedPro) caps |= CAP_LED;   // never drive GPIO2 on real Pro hardware (IR receiver)
    if (p.oled.ack) caps |= CAP_OLED | CAP_ENCODER;
    return caps;
}

uint8_t platformCapFromName(const char* name, size_t len) {
    static const struct { const char* n; uint8_t cap; } kNames[] = {
        {"adv", CAP_ADV}, {"osd", CAP_OSD}, {"audio", CAP_AUDIO}, {"ir", CAP_IR},
        {"oled", CAP_OLED}, {"encoder", CAP_ENCODER}, {"led", CAP_LED}, {"original", CAP_ORIGINAL},
    };
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
        if (strlen(kNames[i].n) == len && !strncmp(kNames[i].n, name, len)) return kNames[i].cap;
    return 0;
}
