#include <stdio.h>
#include <string.h>
#include "stv9426-driver.h"
#include "stv9426-udc.h"
using namespace stv;

static const uint8_t kDisplayOn = 0x80 | kPageP, kDisplayOff = kPageP;   // 0x3FF3: OSD bit + page P[8:6]

static bool sameLayout(const Layout& a, const Layout& b) {
    if (a.count != b.count) return false;
    for (uint8_t i = 0; i < a.count; i++)
        if (a.strips[i].kind != b.strips[i].kind || a.strips[i].arg != b.strips[i].arg ||
            a.strips[i].zoom != b.strips[i].zoom)
            return false;
    return true;
}

Stv9426::Stv9426(OsdBus& bus) : bus_(bus) { layout_.count = 0; forgetGlyphs(); }

void Stv9426::reg(uint8_t n, uint8_t v) { bus_.write((uint16_t)(0x3FF0 + n), &v, 1); }

void Stv9426::begin(uint32_t xtalHz) {
    xtal_ = xtalHz;
    fm_ = stvFm(xtalHz);
    out_ = OUT_480P;
    fh_ = 0;
    t_ = stvTimingFor(out_, xtal_);
    powered_ = wantOn_ = override_ = displayPending_ = false;
    reg(3, kDisplayOff);
    displayOn_ = false;
    reg(7, fm_);
    reg(4, STV_LOCK_TC);                  // PLL time constants valid for LD 32..63 (datasheet Table 2)
    reg(5, STV_CAPTURE_TC);
    reg(0, STV_LD_IDLE);
    reg(6, stvPp(fm_, xtal_, STV_LD_IDLE, lineHz()));
    reg(1, t_.dd);
    reg(2, STV_CH_COMPAT);
    forgetGlyphs();                       // glyphs are uploaded when a screen first uses them
    layout_.count = 0;                    // force the descriptor write
    setLayout(compatLayout(), STV_CH_COMPAT);
    shadow_.reset();
    for (uint8_t r = 0; r < 3; r++) shadow_.setWidth(r, 28);
    shadow_.flush(bus_);
}

void Stv9426::writeActiveTiming() {
    uint8_t ld = override_ ? ovLd_ : t_.ld;
    reg(0, ld);
    reg(6, stvPp(fm_, xtal_, ld, lineHz()));
    ddWritten_ = dd();
    reg(1, ddWritten_);
    reg(2, override_ ? ovCh_ : ch_);
}

uint8_t Stv9426::dd() const {
    if (override_) return ovDd_;
    int v = stvDdForWindow(t_.ld, win_, align_, 0);
    return (uint8_t)(v > 255 ? 255 : v);
}

// A DD change moves the whole panel: blank it until the next flush, like a layout change (no sideways jump of
// the old screen before the new one is drawn)
void Stv9426::applyDd() {
    if (!powered_) return;
    uint8_t v = dd();
    if (v == ddWritten_) return;
    if (displayOn_) {
        reg(3, kDisplayOff);
        displayOn_ = false;
        displayPending_ = true;
        displayAt_ = lastService_;
    }
    reg(1, v);
    ddWritten_ = v;
}

void Stv9426::setWindow(const StvWindow& w) {
    bool same = w.valid == win_.valid && w.htotal == win_.htotal && w.hsSp == win_.hsSp && w.act0 == win_.act0 &&
                w.act1 == win_.act1;
    win_ = w;
    if (!same) applyDd();
}

void Stv9426::setHAlign(uint8_t align) {
    if (align == align_) return;
    align_ = align;
    applyDd();
}

void Stv9426::setOutput(StvOutStd s, uint32_t fhHz, uint16_t activeLines) {
    out_ = s;
    fh_ = fhHz;
    active_ = activeLines;
    t_ = stvTimingForRate(s, fhHz, xtal_);
    override_ = false;
    if (powered_) writeActiveTiming();
}

void Stv9426::setTimingOverride(uint8_t ld, uint8_t dd, uint8_t ch) {
    override_ = true;
    ovLd_ = ld > 63 ? 63 : ld;            // LD bits 7-6 are the sync polarity bits
    ovDd_ = dd;
    ovCh_ = ch > 63 ? 63 : ch;            // CH[5:0]
    if (powered_) writeActiveTiming();
}

bool Stv9426::setLayout(const Layout& l, uint8_t ch) {
    if (layout_.count && sameLayout(l, layout_) && ch == ch_) return false;
    uint8_t desc[kDescSlots * 2];
    if (!buildDescriptors(l, desc)) return false;
    if (displayOn_) {                     // never show a half-written table, nor the new layout over old text:
        reg(3, kDisplayOff);              // the display comes back from service(), after the next flush
        displayOn_ = false;
        displayPending_ = true;
        displayAt_ = lastService_;
    }
    gen_++;
    if (!bus_.write(kPageAddr, desc, sizeof(desc))) {
        layout_.count = 0;                // not stored: the next setLayout writes it again
    } else {
        layout_ = l;
        ch_ = ch;
        reg(2, override_ ? ovCh_ : ch_);
    }
    return true;
}

void Stv9426::powerOn(uint32_t nowMs) {
    wantOn_ = false;
    gen_++;
    forgetGlyphs();                       // the chip RAM may have been lost while closed
    writeActiveTiming();
    Layout l = layout_;                   // resend descriptors and text: the chip RAM may have been lost while closed
    layout_.count = 0;
    setLayout(l, ch_);
    shadow_.invalidate();
    powered_ = true;
    displayPending_ = true;
    displayAt_ = nowMs + kSettleMs;
}

void Stv9426::powerOff() {
    wantOn_ = false;
    powered_ = false;
    displayPending_ = false;
    if (alwaysOn_) return;                // A/B: always on, the display bit and LD stay
    reg(3, kDisplayOff);
    displayOn_ = false;
    reg(0, STV_LD_IDLE);
    reg(6, stvPp(fm_, xtal_, STV_LD_IDLE, lineHz()));
}

void Stv9426::flush() {
    if (shadow_.dirty()) shadow_.flush(bus_);
}

void Stv9426::service(uint32_t nowMs) {
    lastService_ = nowMs;
    if (wantOn_ && !powered_) powerOn(nowMs);
    wantOn_ = false;
    flush();
    // the display bit only over a stored table and a clean text buffer (an I2C error retries on the next call)
    if (displayPending_ && (int32_t)(nowMs - displayAt_) >= 0 && layout_.count && !shadow_.dirty()) {
        reg(3, kDisplayOn);
        displayOn_ = true;
        displayPending_ = false;
    }
}

void Stv9426::romDump(void (*line)(const char* text, void* ctx), void* ctx, uint16_t from, uint16_t lines) {
    char buf[112];
    uint8_t data[32];
    for (uint16_t a = from, n = 0; a < 0x3300 && n < lines; a += 32, n++) {
        bus_.read(a, data, sizeof(data));
        int o = snprintf(buf, sizeof(buf), "%04X:", a);
        for (uint8_t i = 0; i < 32; i++) o += snprintf(buf + o, sizeof(buf) - o, " %02X", data[i]);
        line(buf, ctx);
    }
}

void Stv9426::forgetGlyphs() {
    for (uint8_t i = 0; i < kUdcSlots; i++) slots_[i] = {0xFF, false, 0};
}

void Stv9426::glyphFrame() {
    for (uint8_t i = 0; i < kUdcSlots; i++) slots_[i].pinned = false;
}

uint8_t Stv9426::glyph(uint8_t g) {
    if (g >= UDC_COUNT) return 0x9A;
    glyphClock_++;
    int8_t pick = -1;
    for (uint8_t i = 0; i < kUdcSlots; i++) {
        if (slots_[i].g == g) { pick = (int8_t)i; break; }
    }
    if (pick < 0) {                       // an empty slot, else the least recently used one not on this screen
        for (uint8_t i = 0; i < kUdcSlots; i++) {
            if (slots_[i].pinned) continue;
            if (pick < 0 || slots_[i].g == 0xFF ||
                (slots_[pick].g != 0xFF && (uint16_t)(glyphClock_ - slots_[i].used) > (uint16_t)(glyphClock_ - slots_[pick].used)))
                pick = (int8_t)i;
            if (slots_[pick].g == 0xFF) break;
        }
        if (pick < 0) { glyphMisses_++; return 0x9A; }   // every slot shows something on this screen
        uint8_t b[kUdcBytes];
        stvUdcBytes(g, b);
        if (!bus_.write((uint16_t)(kUdcBytes * (kUdcFirst + pick)), b, sizeof(b))) {
            slots_[pick].g = 0xFF;
            return 0x9A;
        }
        slots_[pick].g = g;
    }
    slots_[pick].pinned = true;
    slots_[pick].used = glyphClock_;
    return (uint8_t)(0x80 | (kUdcFirst + pick));
}
