#include "stv9426-timing.h"
#include "../util/pgm.h"

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

bool stvBypassWindow(uint16_t lines, uint32_t fhHz, StvWindow& w) {
    w = {0, 0, 0, 0, false};
    if (!fhHz || !lines) return false;
    uint32_t rate = (fhHz + lines / 2) / lines;         // fields a second (1080i: 60 or 50; 1080p: 60, 50, 30, 25, 24)
    uint16_t active, porch, total;
    if (lines >= 740 && lines <= 760) {                 // 720p: 750 lines
        active = 1280; porch = 220;
        total = rate >= 55 ? 1650 : 1980;
    } else if ((lines >= 555 && lines <= 570) || (lines >= 1115 && lines <= 1135)) {   // 1080i field, 1080p frame
        active = 1920; porch = 148;
        total = rate >= 55 || (rate >= 28 && rate <= 32) ? 2200 : rate >= 45 || rate == 25 ? 2640 : 2750;
    } else return false;
    w.htotal = total;
    w.hsSp = 0;
    w.act0 = porch;
    w.act1 = (uint16_t)(porch + active);
    w.valid = true;
    return true;
}

uint32_t stvLineHzFromIfPeriod(uint16_t hperiodIf) {
    if (!hperiodIf) return 0;
    uint32_t fh = (27000000UL / 4 + hperiodIf / 2) / hperiodIf;
    return fh >= 15000 && fh <= 100000 ? fh : 0;
}

// VESA DMT (and CTA-861 720p): lines a frame, refresh, pixels a line, sync, sync + back porch, active pixels, vertical
// sync + back porch, active lines. Two modes within 2 * kLinesNear lines have refresh rates more than 2 * kHzNear
// apart, or sync pulses at least 3 % of the line apart (the measured pulse picks one)
static const StvVgaMode kVgaModes[] PROGMEM = {
    {449, 70, 900, 108, 108 + 54, 720, 2 + 35, 400},      // 720x400 (and 640x400: the same proportions)
    {500, 75, 840, 64, 64 + 120, 640, 3 + 16, 480},       // 640x480
    {509, 85, 832, 56, 56 + 80, 640, 3 + 25, 480},
    {517, 60, 1088, 112, 112 + 112, 848, 8 + 23, 480},    // 848x480
    {520, 72, 832, 40, 40 + 128, 640, 3 + 28, 480},       // 640x480
    {525, 60, 800, 96, 96 + 48, 640, 2 + 33, 480},
    {525, 60, 858, 64, 168, 640, 40, 480},                // Dreamcast (BootROM; 27 MHz clocks): sync 7.5 %
    {625, 56, 1024, 72, 72 + 128, 800, 2 + 22, 600},      // 800x600
    {625, 75, 1056, 80, 80 + 160, 800, 3 + 21, 600},
    {628, 60, 1056, 128, 128 + 88, 800, 4 + 23, 600},
    {631, 85, 1048, 64, 64 + 152, 800, 3 + 27, 600},
    {666, 72, 1040, 120, 120 + 64, 800, 6 + 23, 600},
    {750, 60, 1650, 40, 40 + 220, 1280, 5 + 20, 720},     // 1280x720 (CTA-861)
    {800, 75, 1312, 96, 96 + 176, 1024, 3 + 28, 768},     // 1024x768
    {806, 60, 1344, 136, 136 + 160, 1024, 6 + 29, 768},
    {806, 70, 1328, 136, 136 + 144, 1024, 6 + 29, 768},
    {808, 85, 1376, 96, 96 + 208, 1024, 3 + 36, 768},
    {831, 60, 1680, 128, 128 + 200, 1280, 6 + 22, 800},   // 1280x800
    {900, 75, 1600, 128, 128 + 256, 1152, 3 + 32, 864},   // 1152x864
    {934, 60, 1904, 152, 152 + 232, 1440, 6 + 25, 900},   // 1440x900
    {1000, 60, 1800, 112, 112 + 312, 1280, 3 + 36, 960},  // 1280x960
    {1066, 60, 1688, 112, 112 + 248, 1280, 3 + 38, 1024}, // 1280x1024
};
static const uint8_t kVgaModeCount = sizeof(kVgaModes) / sizeof(kVgaModes[0]);
static const uint16_t kLinesNear = 3;   // the sync processor's count against the standard's
static const uint16_t kPassDelayPermille = 26;   // the pass-through's video delay, thousandths of a line
static const uint8_t kHzNear = 1;       // the line rate is one IF count from exact: under 0.5 % up to 60 kHz

uint8_t stvVgaPassModeCount() { return kVgaModeCount; }

StvVgaMode stvVgaPassModeAt(uint8_t i) {
    StvVgaMode m;
    memcpy_P(&m, &kVgaModes[i < kVgaModeCount ? i : 0], sizeof m);
    return m;
}

uint16_t stvVgaSyncPermille(const StvVgaMode& m) { return (uint16_t)((m.hsync * 1000UL + m.htotal / 2) / m.htotal); }

bool stvVgaPassMode(uint16_t lines, uint32_t fhHz, uint16_t syncPermille, StvVgaMode& m) {
    if (!fhHz || !lines) return false;
    uint32_t rate = (fhHz + lines / 2) / lines;
    bool found = false;
    uint16_t best = 0xFFFF;
    for (uint8_t i = 0; i < kVgaModeCount; i++) {
        StvVgaMode c = stvVgaPassModeAt(i);
        uint16_t dl = lines > c.lines ? lines - c.lines : c.lines - lines;
        uint32_t dr = rate > c.hz ? rate - c.hz : c.hz - rate;
        if (dl > kLinesNear || dr > kHzNear) continue;
        uint16_t sp = stvVgaSyncPermille(c);
        uint16_t d = !syncPermille ? 0 : syncPermille > sp ? syncPermille - sp : sp - syncPermille;
        if (!found || d < best) { m = c; best = d; found = true; }
    }
    return found;
}

bool stvVgaPassWindow(uint16_t lines, uint32_t fhHz, uint16_t syncPermille, StvWindow& w) {
    w = {0, 0, 0, 0, false};
    StvVgaMode m;
    if (!stvVgaPassMode(lines, fhHz, syncPermille, m)) return false;
    // The picture leaves the pass-through kPassDelayPermille of a line after its sync (the ADC to DAC path: a fixed
    // count of the ADC's clocks, 1857 a line in every mode). Measured with a Dreamcast in 12 modes, 31 to 68 kHz: the
    // menu sat 2.2 to 3.0 % of a line left of the picture's centre at any rate (a delay in time would double at 60 kHz)
    uint16_t delay = (uint16_t)((m.htotal * (uint32_t)kPassDelayPermille + 500) / 1000);
    w.htotal = m.htotal;
    w.hsSp = 0;
    w.act0 = (uint16_t)(m.hstart + delay);
    w.act1 = (uint16_t)(m.hstart + delay + m.hactive);
    w.valid = true;
    return true;
}

StvTiming stvTimingFor(StvOutStd s, uint32_t xtalHz) { return stvTimingForRate(s, 0, xtalHz); }

StvTiming stvTimingForRate(StvOutStd s, uint32_t fhHz, uint32_t xtalHz, uint16_t activeLines) {
    if (s >= OUT_STD_COUNT) s = OUT_480P;
    uint32_t fh = fhHz ? fhHz : kLineHz[s];
    uint8_t fm = stvFm(xtalHz);
    StvTiming t;
    t.ld = s == OUT_BYPASS && !fhHz ? STV_LD_IDLE : stvLdFor(fh, xtalHz, fm);   // a bypass of unknown rate: LD 32
    t.pp = stvPp(fm, xtalHz, t.ld, fh);
    t.dd = stvDdFor(t.ld);
    t.chMenu = kGeo[s].ch; t.topMenu = kGeo[s].top; t.zoomMenu = kGeo[s].zoom;
    if (s == OUT_BYPASS && activeLines) {              // the rows of a scaled output with as many visible lines
        uint16_t ch = (uint16_t)(18u * activeLines / 480u);
        t.chMenu = (uint8_t)(ch < 12 ? 12 : ch > 45 ? 45 : ch);
        t.topMenu = t.chMenu >= 27 ? 58 : t.chMenu >= 18 ? 40 : 20;
    }
    return t;
}

static bool linesNear(uint16_t a, uint16_t b) { return (a > b ? a - b : b - a) <= 3; }
static bool rateNear(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) * 50 <= (a > b ? a : b); }

bool StvSourceWatch::changed(uint16_t lines, uint32_t fh, uint16_t placedLines, uint32_t placedFh) {
    if (!fh || !lines) return false;
    if (linesNear(lines, placedLines) && rateNear(fh, placedFh)) {
        lastLines_ = 0;
        lastFh_ = 0;
        return false;
    }
    bool again = lastFh_ && linesNear(lines, lastLines_) && rateNear(fh, lastFh_);
    lastLines_ = again ? 0 : lines;
    lastFh_ = again ? 0 : fh;
    return again;
}

bool stvVdsLines(uint16_t vsSt, uint16_t vsSp, uint16_t disVbSt, uint16_t disVbSp, uint16_t vTotal, uint16_t& first,
                 uint16_t& active) {
    if (vTotal < 200 || vsSt >= vTotal || vsSp >= vTotal || disVbSt > vTotal || disVbSp >= vTotal) return false;
    uint16_t span = (uint16_t)((vsSp + vTotal - vsSt) % vTotal);        // vsSt -> vsSp
    uint16_t end = span <= vTotal / 2 ? vsSp : vsSt;                   // the sync is the shorter span: its end
    uint16_t f = (uint16_t)((disVbSp + vTotal - end) % vTotal);
    uint16_t a = (uint16_t)((disVbSt + vTotal - disVbSp) % vTotal);
    if (a < vTotal / 3 || f + a > vTotal) return false;
    first = f;
    active = a;
    return true;
}
