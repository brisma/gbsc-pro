#include "stv9426-timing.h"

static const uint32_t kLineHz[OUT_STD_COUNT] = {31469, 31250, 45000, 37500, 60000, 50000, 63981, 53300,
                                                67500, 56250, 15734, 15625, 87500,    // bypass: unknown rate, LD 32 is safe to 87.5 kHz
                                                74040, 61750};                        // 1200p60 / 1200p50
struct MenuGeo { uint8_t ch, top; bool zoom; };
// Rows of 18 * active / 480 lines, unzoomed (zoom doubles the height only): the 480p share of the height at every
// output; 576p and the 15 kHz outputs as before
static const MenuGeo kGeo[OUT_STD_COUNT] = {
    {18, 40, false}, {18, 40, false}, {27, 58, false}, {27, 58, false}, {36, 58, false}, {36, 58, false},
    {38, 58, false}, {38, 58, false}, {40, 58, false}, {40, 58, false}, {12, 20, false}, {12, 20, false},
    {18, 40, false}, {45, 58, false}, {45, 58, false}};

static const uint16_t kActive[OUT_STD_COUNT] = {480, 576, 720, 720, 960, 960, 1024, 1024, 1080, 1080, 240, 288, 480, 1200, 1200};
uint16_t stvActiveLines(StvOutStd s) { return s < OUT_STD_COUNT ? kActive[s] : 480; }

uint32_t stvLineHz(StvOutStd s) { return s < OUT_STD_COUNT ? kLineHz[s] : kLineHz[OUT_480P]; }

StvOutStd stvOutStdOf(uint8_t id) {
    switch (id) {
        case 0x01: return OUT_960P60;  case 0x11: return OUT_960P50;
        case 0x02: return OUT_1024P60; case 0x12: return OUT_1024P50;
        case 0x03: return OUT_720P60;  case 0x13: return OUT_720P50;
        case 0x04: return OUT_480P;    case 0x14: return OUT_576P;
        case 0x05: return OUT_1080P60; case 0x15: return OUT_1080P50;
        case 0x06: return OUT_15K60;   case 0x16: return OUT_15K50;
        case 0x07: return OUT_1200P60; case 0x17: return OUT_1200P50;
        case 0x21: case 0x22: return OUT_BYPASS;
        default: return OUT_480P;
    }
}

uint8_t stvFm(uint32_t xtalHz) {
    uint32_t n = (uint32_t)((100000000ULL + xtalHz - 1) / xtalHz);   // ceil(200 MHz / (2 * xtal))
    int fm = (int)n - 3;
    return (uint8_t)(fm < 0 ? 0 : fm > 15 ? 15 : fm);
}

// Datasheet: pixel <= 50 MHz and VCO > 6x pixel (34.7 MHz with 8 MHz: the menu filled 3/4 of a 1080p line). On a
// TV: 45.3 MHz steady and sharp, 49.8 MHz shimmering edges: stop at 45.5 MHz (VCO >= 4.5x pixel)
bool stvWithinPll(uint8_t ld, uint32_t fh, uint32_t xtalHz, uint8_t fm) {
    uint64_t pixel = 12ULL * (ld + 1) * fh;
    uint64_t vco = 2ULL * (fm + 3) * xtalHz;
    return pixel <= 45500000ULL && 2ULL * vco >= 9ULL * pixel;
}

uint8_t stvLdFor(uint32_t fh, uint32_t xtalHz, uint8_t fm) {
    for (int ld = 63; ld > 32; ld--)
        if (stvWithinPll((uint8_t)ld, fh, xtalHz, fm)) return (uint8_t)ld;
    return 32;
}

uint8_t stvPp(uint8_t fm, uint32_t xtalHz, uint8_t ld, uint32_t fh) {
    uint64_t num = 8ULL * 2 * (fm + 3) * xtalHz;
    uint64_t den = 12ULL * (ld + 1) * fh;
    int64_t pp = (int64_t)((num + den / 2) / den) - 24;
    return (uint8_t)(pp < 0 ? 0 : pp > 255 ? 255 : pp);
}

uint8_t stvDdFor(uint8_t ld) {
    int dd = (int)((138 * (ld + 1) + 96) / 192) - 8;   // (DD+8)*3 / (12*(LD+1)) = 138/768, the LD 63 / DD 38 edge
    int fit = ((int)(12 * (ld + 1)) * 95 / 100 - 30 * 12) / 3 - 8;   // 30 cells end within 95% of the line
    if (dd > fit) dd = fit;
    return (uint8_t)(dd < 4 ? 4 : dd);
}

int stvDdForWindow(uint8_t ld, const StvWindow& w, uint8_t align, int16_t refOffsetPx) {
    if (!w.valid || !w.htotal) return stvDdFor(ld);
    int32_t ht = w.htotal;
    int32_t s = (((int32_t)w.act0 - w.hsSp - refOffsetPx) % ht + ht) % ht;      // active start after the edge
    int32_t a = (((int32_t)w.act1 - w.act0) % ht + ht) % ht;                     // active width
    int32_t px = 12 * (ld + 1);                                                    // STV pixels per line
    int32_t sp = (s * px + ht / 2) / ht, ap = (a * px + ht / 2) / ht;
    int32_t centre = sp + (ap - 30 * 12) / 2, x = centre;
    if (align == STV_ALIGN_LEFT) x = sp + ap * 5 / 100;
    else if (align == STV_ALIGN_RIGHT) x = sp + ap - ap * 5 / 100 - 30 * 12;
    if (align == STV_ALIGN_LEFT && x > centre) x = centre;      // a window narrower than the panel + margins:
    if (align == STV_ALIGN_RIGHT && x < centre) x = centre;     // left and right meet at the centre
    int dd = (int)((x + 1) / 3) - 8;                                              // round(x / 3) - 8
    int fit = ((int)px * 95 / 100 - 30 * 12) / 3 - 8;                             // 30 cells end within 95 %
    if (dd > fit) dd = fit;
    return dd < 4 ? 4 : dd;
}

// Datasheet Table 2: rows B 0..7, columns A 0..6, four flags each for LD 16/32/48/63 (Y = the PLL can lock)
static const char* const kTable2[8][7] = {
    {"YYYY", "YYYY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"YYYY", "YYYY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NYYY", "YYYY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NNNY", "YYYY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NNNN", "NYYY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NNNN", "NNNY", "YYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NNNN", "NNNN", "NYYY", "YYYN", "YNNN", "NNNN", "NNNN"},
    {"NNNN", "NNNN", "NNNY", "YYYN", "YNNN", "NNNN", "NNNN"}};

bool stvTimeConstantsOk(uint8_t a, uint8_t b, uint8_t ld) {
    if (a > 6 || b > 7 || b < a || b - a >= 3) return false;   // datasheet summary: B >= A, B - A < 3
    static const uint8_t kLd[4] = {16, 32, 48, 63};
    const char* f = kTable2[b][a];
    for (uint8_t i = 0; i < 4; i++) {
        if (ld == kLd[i]) return f[i] == 'Y';
        if (i < 3 && ld > kLd[i] && ld < kLd[i + 1]) return f[i] == 'Y' && f[i + 1] == 'Y';   // both neighbours
    }
    return false;
}

StvTiming stvTimingFor(StvOutStd s, uint32_t xtalHz) { return stvTimingForRate(s, 0, xtalHz); }

StvTiming stvTimingForRate(StvOutStd s, uint32_t fhHz, uint32_t xtalHz) {
    if (s >= OUT_STD_COUNT) s = OUT_480P;
    uint32_t fh = fhHz ? fhHz : kLineHz[s];
    uint8_t fm = stvFm(xtalHz);
    StvTiming t;
    t.ld = s == OUT_BYPASS && !fhHz ? STV_LD_IDLE : stvLdFor(fh, xtalHz, fm);   // a bypass of unknown rate: LD 32
    t.pp = stvPp(fm, xtalHz, t.ld, fh);
    t.dd = stvDdFor(t.ld);
    t.chMenu = kGeo[s].ch; t.topMenu = kGeo[s].top; t.zoomMenu = kGeo[s].zoom;
    return t;
}
