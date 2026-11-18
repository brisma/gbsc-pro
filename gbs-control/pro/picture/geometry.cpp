#include "geometry.h"
#include "../util/pgm.h"   // PROGMEM, memcpy_P

static const uint16_t kScaleMax = 1023;   // VDS_HSCALE and VDS_VSCALE: 10 bits, the largest value is 1:1
static const uint16_t kScaleMin = 256;    // VDS_HSCALE: the floor of Screen > Size (scaleHorizontal)

static long roundL(float v) { return (long)(v < 0 ? v - 0.5f : v + 0.5f); }

// A position kept inside 0..hi; counts the ones it had to move
static uint16_t hold(long v, long lo, long hi, uint8_t& clamps) {
    if (v < lo) { clamps++; return (uint16_t)lo; }
    if (v > hi) { clamps++; return (uint16_t)hi; }
    return (uint16_t)v;
}

static const uint16_t kTotalMax = 4095;    // VDS_HSYNC_RST: 12 bits

// A position on a line that wraps
static uint16_t onLine(long v, long total) {
    long m = v % total;
    return (uint16_t)(m < 0 ? m + total : m);
}

// The memory blank as two positions that do not wrap: pixels are read from sp forward to st. Screen > Position
// moves both and wraps each at the line total, so st may come before sp in the registers.
static void readOf(const GeoRegs& r, long& sp, long& st) {
    sp = r.hbSp;
    st = r.hbSt;
    if (st <= sp) st += r.hTotal;
}

bool geoClockStep(GeoRegs& r) {
    float k;
    uint8_t next;
    if (r.clock == 0x65) { next = 0x85; k = 4.0f / 3.0f; }
    else if (r.clock == 0x85) { next = 0xA5; k = 1.5f; }
    else return false;
    long total = roundL(r.hTotal * k);
    if (total > kTotalMax) return false;
    long sp, st;
    readOf(r, sp, st);
    r.clock = next;
    r.hTotal = (uint16_t)total;
    r.hsSt = (uint16_t)roundL(r.hsSt * k);
    r.hsSp = (uint16_t)roundL(r.hsSp * k);
    r.disHbSp = (uint16_t)roundL(r.disHbSp * k);
    r.disHbSt = (uint16_t)roundL(r.disHbSt * k);
    r.hbSp = onLine(roundL(k * (sp + kGeoLat) - kGeoLat), total);
    r.hbSt = onLine(roundL(k * (st + kGeoLat) - kGeoLat), total);
    r.hScale = (uint16_t)roundL(r.hScale / k);
    r.hScaleByps = false;
    return true;
}

bool geoWidth(GeoRegs& r, float f) {
    if (f > 0.9995f && f < 1.0005f) return true;
    if (f <= 0) return false;
    GeoRegs n = r;   // clamps adds to the step's count (geometryApply starts it at 0)
    while (n.hScale / f > kScaleMax + 0.49f)
        if (!geoClockStep(n)) return false;
    if (n.disHbSp >= n.disHbSt || n.disHbSt >= n.hTotal) return false;
    const long total = n.hTotal;
    float c = (n.disHbSp + n.disHbSt) / 2.0f;
    // the read, placed on the turn of the line that holds the window
    long sp, st;
    readOf(n, sp, st);
    while ((sp + st) / 2.0f + kGeoLat - c > total / 2.0f) { sp -= total; st -= total; }
    while ((sp + st) / 2.0f + kGeoLat - c < -total / 2.0f) { sp += total; st += total; }
    bool endsInWindow = st <= n.disHbSt;

    long hs = roundL(n.hScale / f);
    if (hs < kScaleMin) return false;   // wider than Screen > Size ever goes: not applied
    n.hScale = (uint16_t)(hs > kScaleMax ? kScaleMax : hs);
    n.hScaleByps = n.hScale >= kScaleMax;
    n.disHbSp = hold(roundL(c + f * (n.disHbSp - c)), 0, total - 1, n.clamps);
    n.disHbSt = hold(roundL(c + f * (n.disHbSt - c)), 0, total - 1, n.clamps);
    if (n.disHbSp >= n.disHbSt) return false;
    // the memory blank keeps its distances in source pixels: its edges are scaled where their pixels show
    long newSp = roundL(c + f * (sp + kGeoLat - c) - kGeoLat);
    long newSt = roundL(c + f * (st + kGeoLat - c) - kGeoLat);
    if (endsInWindow && newSt > n.disHbSt) { newSt = n.disHbSt; n.clamps++; }   // the line total tuning resets a later one
    if (newSt - newSp > total - 1) { newSp = newSt - (total - 1); n.clamps++; }  // a read is never longer than the line
    if (newSt <= newSp) return false;
    n.hbSp = onLine(newSp, total);
    n.hbSt = onLine(newSt, total);
    r = n;
    return true;
}

// ---- Pictures per table ----

namespace {

struct Pic { GeoVert vert; uint16_t widthPm; };
#define GEO_TABLE {{0, 0, 0}, 1000}   // the table's own picture

// One row per table. The 60 Hz tables use pal (the table itself) and intPal (Integer scale) only.
struct Row {
    uint8_t id;
    uint16_t vScale;              // the table's VDS_VSCALE
    uint16_t forcePm, fullPm;     // Aspect: Force 4:3 on top of the picture's width, Full instead of it
    Pic pal, fullHeight, as60, intPal, intAs60;
};

const Row kRows[] PROGMEM = {   // in flash: read through rowOf()
    // id   vScale force full   PAL TV                   Full height             As 60Hz                   Integer (PAL TV)        Integer (As 60Hz)
    {0x01,  512,   750, 1000,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                GEO_TABLE,               GEO_TABLE},
    {0x02,  485,   750, 1000,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                {{512, 0, 0}, 947},     GEO_TABLE},
    {0x03,  683,  1000, 1333,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                GEO_TABLE,               GEO_TABLE},
    {0x04, 1023,   750, 1000,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                GEO_TABLE,               GEO_TABLE},
    {0x05,  455,  1000, 1400,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                {{512, 0, 0}, 889},     GEO_TABLE},
    {0x07,  410,   900, 1200,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                GEO_TABLE,               GEO_TABLE},
    {0x11,  585,   750, 1000,   {{614, 0, 0}, 1000},     GEO_TABLE,               {{512, 0, 0}, 988},       {{683, 0, 0}, 900},     {{512, 0, 0}, 988}},
    {0x12,  512,   750, 1000,   {{614, 10, 7}, 965},     GEO_TABLE,               {{0, 0, 0}, 953},         {{683, 0, 0}, 869},     {{0, 0, 0}, 953}},
    {0x13,  819,  1000, 1333,   {{0, 0, 0}, 1127},       GEO_TABLE,               {{683, 0, 0}, 1078},      {{1023, 0, 0}, 902},    {{683, 0, 0}, 1078}},
    {0x14, 1023,   750, 1000,   GEO_TABLE,               GEO_TABLE,               GEO_TABLE,                GEO_TABLE,               GEO_TABLE},
    {0x15,  512,  1000, 1260,   GEO_TABLE,               {{455, 14, 33}, 1000},  {{455, 14, 33}, 910},     GEO_TABLE,               {{0, 0, 0}, 809}},
    {0x17,  437,   900, 1200,   {{492, 0, 0}, 1000},     GEO_TABLE,               {{410, 0, 0}, 987},       {{512, 0, 0}, 960},     {{410, 0, 0}, 987}},
};

bool rowOf(uint8_t id, Row& out) {
    for (uint8_t i = 0; i < sizeof(kRows) / sizeof(kRows[0]); i++) {
        memcpy_P(&out, &kRows[i], sizeof(out));
        if (out.id == id) return true;
    }
    return false;
}

bool is50(uint8_t id) { return (id & 0x10) != 0; }

Pic picOf(const Row& row, GeoState s) {
    if (!is50(row.id)) return s.integer ? row.intPal : row.pal;
    if (s.style50 == GEO_AS_60HZ) return s.integer ? row.intAs60 : row.as60;
    if (s.integer) return row.intPal;
    return s.style50 == GEO_FULL_HEIGHT ? row.fullHeight : row.pal;
}

// The width of a 4:3 frame's picture on the 5:4 table (15/16) and of a 5:4 frame's on the 4:3 table (16/15), per
// mille; 0 for a table that never stands in
uint16_t standInPm(uint8_t id) { return id == 0x12 ? 938 : (id == 0x01 ? 1067 : 0); }

bool sameVert(const GeoVert& a, const GeoVert& b) { return a.vScale == b.vScale && a.top == b.top && a.ifVb == b.ifVb; }

uint16_t add(uint16_t v, int d, int hi) {
    int n = (int)v + d;
    return (uint16_t)(n < 0 ? 0 : (n > hi ? hi : n));
}

// IF_VB_ST / IF_VB_SP move as one pair (upstream keeps SP = ST + 2; equal values stop the picture): a move past
// either end of the register is shortened for both, and counted as a held edge
void moveCapture(GeoRegs& r, int d) {
    int want = d;
    if ((int)r.ifVbSt + d < 0) d = -(int)r.ifVbSt;
    if ((int)r.ifVbSp + d > 0x7FF) d = 0x7FF - (int)r.ifVbSp;
    if (d != want) r.clamps++;
    if (d == 0) return;
    r.ifVbSt = (uint16_t)(r.ifVbSt + d);
    r.ifVbSp = (uint16_t)(r.ifVbSp + d);
}

// A window of h lines shows h * vScale / 2048 source lines. A picture that changes its scale stays centred when the
// lines that leave the window (negative: that come back into it) are shared above and below: IF_VB counts half
// source lines, so the capture start moves by all of them
int centreSkip(long hOld, uint16_t oldVs, long hNew, uint16_t newVs) {
    return (int)roundL(((float)hOld * oldVs - (float)hNew * newVs) / 2048.0f);
}

// The table's picture -> the vertical part `to`
void vertApply(GeoRegs& r, const Row& row, const GeoVert& to) {
    uint16_t oldVs = r.vScale ? r.vScale : row.vScale;
    uint16_t newVs = oldVs;
    if (to.vScale && to.vScale != row.vScale) {
        long v = roundL((float)oldVs * to.vScale / row.vScale);   // a hand-made scale keeps its proportion
        newVs = (uint16_t)(v > kScaleMax ? kScaleMax : (v < 1 ? 1 : v));
    }
    r.vScale = newVs;
    r.bar = 0;
    if (r.disVbSt <= r.disVbSp) return;
    const long src = is50(row.id) ? 576 : 480;                   // the doubled frame the scale works on
    long h = r.disVbSt - r.disVbSp;
    long oldLines = src * 1024 / oldVs, newLines = src * 1024 / newVs;
    if (oldLines > h) oldLines = h;
    if (newLines < oldLines) {                                    // shorter: a bar above and below
        long d = (oldLines - newLines) / 2;
        if (d < 1) d = 1;                                         // the bar also says which way the picture went
        if (d > 255) d = 255;
        r.disVbSp = (uint16_t)(r.disVbSp + d);
        r.disVbSt = (uint16_t)(r.disVbSt - d);
        r.vbSp = (uint16_t)(r.vbSp + d);
        r.bar = (uint8_t)d;
        int skip = centreSkip(h, oldVs, h - 2 * d, newVs);       // a table that crops the frame gets its lines back
        moveCapture(r, skip);
        if (to.top || to.ifVb) {                                  // the row's own move: window, bars and capture
            r.disVbSp = add(r.disVbSp, to.top, 0x7FF);
            r.disVbSt = add(r.disVbSt, to.top, 0x7FF);
            r.vbSp = add(r.vbSp, to.top, 0x7FF);
            moveCapture(r, to.ifVb);
        }
    } else if (to.top || to.ifVb) {                               // taller, with the row's own values
        r.disVbSp = add(r.disVbSp, to.top, r.disVbSt - 1);
        moveCapture(r, to.ifVb);
    } else {                                                      // taller: cropped above and below alike
        int skip = centreSkip(h, oldVs, h, newVs);
        moveCapture(r, skip);
    }
}

// The vertical part `from` (with the bar it added) -> the table's picture
void vertUndo(GeoRegs& r, const Row& row, const GeoVert& from) {
    bool centred = r.bar || (!from.top && !from.ifVb);
    long hNow = (long)r.disVbSt - r.disVbSp;
    uint16_t now = r.vScale;
    if (r.bar) {
        if (from.top || from.ifVb) {                              // the row's own move first
            r.disVbSp = add(r.disVbSp, -from.top, 0x7FF);
            r.disVbSt = add(r.disVbSt, -from.top, 0x7FF);
            r.vbSp = add(r.vbSp, -from.top, 0x7FF);
            moveCapture(r, -from.ifVb);
        }
        r.disVbSp = add(r.disVbSp, -(int)r.bar, 0x7FF);
        r.disVbSt = add(r.disVbSt, r.bar, 0x7FF);
        r.vbSp = add(r.vbSp, -(int)r.bar, 0x7FF);
    } else if (from.top || from.ifVb) {
        r.disVbSp = add(r.disVbSp, -from.top, 0x7FF);
        moveCapture(r, -from.ifVb);
    }
    if (from.vScale && from.vScale != row.vScale && r.vScale) {
        long v = roundL((float)r.vScale * row.vScale / from.vScale);
        r.vScale = (uint16_t)(v > kScaleMax ? kScaleMax : (v < 1 ? 1 : v));
    }
    if (centred && hNow > 0 && r.disVbSt > r.disVbSp) {
        int skip = centreSkip(r.disVbSt - r.disVbSp, r.vScale, hNow, now);
        moveCapture(r, -skip);
    }
    r.bar = 0;
}

bool sameRegs(const GeoRegs& a, const GeoRegs& b) {
    return a.hTotal == b.hTotal && a.hsSt == b.hsSt && a.hsSp == b.hsSp && a.hbSt == b.hbSt && a.hbSp == b.hbSp &&
           a.disHbSt == b.disHbSt && a.disHbSp == b.disHbSp && a.hScale == b.hScale && a.hScaleByps == b.hScaleByps &&
           a.vScale == b.vScale && a.vbSp == b.vbSp && a.disVbSt == b.disVbSt && a.disVbSp == b.disVbSp &&
           a.ifVbSt == b.ifVbSt && a.ifVbSp == b.ifVbSp && a.clock == b.clock && a.mark == b.mark && a.bar == b.bar;
}

}  // namespace

bool geoTarget(uint8_t tableId, GeoState s, GeoTarget& out) {
    Row row;
    if (!rowOf(tableId, row)) return false;
    Pic p = picOf(row, s);
    out.vert = p.vert;
    if (s.aspect == GEO_FULL) out.widthPm = row.fullPm;
    else if (s.aspect == GEO_FORCE43) out.widthPm = (uint16_t)(((uint32_t)p.widthPm * row.forcePm + 500) / 1000);
    else out.widthPm = p.widthPm;
    if (s.standIn && s.aspect != GEO_FULL && standInPm(tableId))
        out.widthPm = (uint16_t)(((uint32_t)out.widthPm * standInPm(tableId) + 500) / 1000);
    return true;
}

// The 960p table standing in for 1024p (60 Hz source), Auto: widened x1.067 its window reaches back past the start
// of the capture (a column of fragments at the left) and its read past the end of the capture (fragments at the
// right). The read ends 10 clocks sooner and the window ends with it (the line total tuning moves a read that ends
// after the window to 97 % of it, and the picture then tears); the left edge closes by the same 93 clocks as the right
// one, so the window stays centred on the picture (the PRO's converter places the picture by the window's edges when
// it locks). dir 1 applies, -1 takes it back
static void standInNudge(GeoRegs& r, uint8_t tableId, const GeoState& s, int dir) {
    if (tableId != 0x01 || !s.standIn || s.aspect != GEO_AUTO || s.integer) return;
    const long total = r.hTotal;
    if (!total) return;
    r.disHbSp = (uint16_t)(r.disHbSp + dir * 93);
    r.disHbSt = (uint16_t)(r.disHbSt - dir * 93);
    r.hbSt = onLine((long)r.hbSt - dir * 10, total);
}

GeoState geoNormalise(uint8_t tableId, GeoState s) {
    if (s.aspect > GEO_FULL) s.aspect = GEO_AUTO;
    if (!is50(tableId) || s.style50 > GEO_AS_60HZ) s.style50 = GEO_PAL_TV;
    if (s.integer && s.style50 == GEO_FULL_HEIGHT) s.style50 = GEO_PAL_TV;
    if (!standInPm(tableId)) s.standIn = false;
    return s;
}

uint8_t geoMark(uint8_t tableId, GeoState s) {
    uint8_t m = (uint8_t)((s.aspect & 3) | (s.integer ? 4 : 0));
    if (is50(tableId)) m |= (uint8_t)((s.style50 + 1) << 3);
    if (s.standIn) m |= 0x20;
    return m;
}

GeoResult geometryApply(GeoRegs& r, uint8_t tableId, GeoState want) {
    GeoResult res = {false, false, false, 1000, r.clock, r.clock, r.vScale};
    Row row;
    if (!rowOf(tableId, row)) return res;
    res.acted = true;
    want = geoNormalise(tableId, want);
    GeoTarget to = {{0, 0, 0}, 1000}, from = to;   // mark 0: the table as loaded
    geoTarget(tableId, want, to);
    uint8_t style = (uint8_t)((r.mark >> 3) & 3);
    bool marked = r.mark != 0 && (r.mark & 3) <= GEO_FULL && (!is50(tableId) || style != 0);
    if (marked) {
        GeoState held = {(uint8_t)(r.mark & 3), (r.mark & 4) != 0, (uint8_t)(style ? style - 1 : 0), (r.mark & 0x20) != 0};
        geoTarget(tableId, geoNormalise(tableId, held), from);
    } else if (tableId == 0x15 && r.disVbSp == 42 && r.vScale >= 400 && r.vScale < 500) {
        // Registers saved before the mark existed, with the Full height option of that time on: its picture is told
        // by the window's first line and the scale (the table has 28 and 512). Another style starts from it.
        GeoState held = {GEO_AUTO, false, GEO_FULL_HEIGHT, false};
        geoTarget(tableId, held, from);
    }
    GeoRegs n = r;
    n.clamps = 0;
    if (!marked) n.bar = 0;   // a bar exists only with the mark that added it (registers by hand, a damaged profile)
    if (!sameVert(from.vert, to.vert)) {
        vertUndo(n, row, from.vert);
        vertApply(n, row, to.vert);
    }
    res.widthPm = (uint16_t)(((uint32_t)to.widthPm * 1000 + from.widthPm / 2) / from.widthPm);
    GeoState heldState = marked ? GeoState{(uint8_t)(r.mark & 3), (r.mark & 4) != 0, 0, (r.mark & 0x20) != 0}
                                : GeoState{GEO_AUTO, false, GEO_PAL_TV, false};
    standInNudge(n, tableId, heldState, -1);                       // the held picture's own move first
    if (to.widthPm != from.widthPm && !geoWidth(n, (float)to.widthPm / from.widthPm)) {
        res.failed = true;
        return res;
    }
    standInNudge(n, tableId, want, 1);
    n.mark = geoMark(tableId, want);
    res.changed = !sameRegs(n, r);
    res.clockTo = n.clock;
    res.vScale = n.vScale;
    uint8_t clamps = n.clamps;
    r = n;
    r.clamps = clamps;
    return res;
}
