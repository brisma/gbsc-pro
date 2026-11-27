#include "../util/pgm.h"
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
    uint8_t caps = pro ? (uint8_t)CAPS_PRO : (uint8_t)CAP_ORIGINAL;
    if (!pro && !probedPro) caps |= CAP_LED;   // never drive GPIO2 on real Pro hardware (IR receiver)
    if (p.oled.ack) caps |= CAP_OLED | CAP_ENCODER;
    return caps;
}

uint8_t platformCapFromName(const char* name, size_t len) {
    static const uint8_t kCaps[] = {CAP_ADV, CAP_OSD, CAP_AUDIO, CAP_IR, CAP_OLED, CAP_ENCODER, CAP_LED, CAP_ORIGINAL};
    static constexpr char kNames[] PROGMEM = "adv\0osd\0audio\0ir\0oled\0encoder\0led\0original";
    static_assert(PGM_LIST_COUNT(kNames) == sizeof(kCaps), "kNames: the count follows the list");
    for (uint8_t i = 0; i < sizeof(kCaps); i++) {
        PGM_P n = pgmListAt(kNames, i, sizeof(kCaps));
        if (strlen_P(n) == len && !strncmp_P(name, n, len)) return kCaps[i];
    }
    return 0;
}
