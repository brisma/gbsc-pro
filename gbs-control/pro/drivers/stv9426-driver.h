// STV9426 driver core: layout/descriptors, shadow text, power management, per-output timing.
// Pure: talks to the chip only through OsdBus (Wire on the device, FakeStv in the native tests).
#pragma once
#include <stdint.h>
#include "stv9426-map.h"
#include "stv9426-timing.h"
#include "stv9426-bus.h"
#include "stv9426-shadow.h"

class Stv9426 {
public:
    explicit Stv9426(OsdBus& bus);
    void begin(uint32_t xtalHz);          // PLL regs, FM, UDC upload, compat layout, shadow reset+flush, power off
    // fhHz: measured line rate (bypass), 0 = nominal; activeLines: measured visible lines (bypass), 0 = the table
    void setOutput(StvOutStd s, uint32_t fhHz = 0, uint16_t activeLines = 0);
    uint16_t activeLines() const { return active_ ? active_ : stvActiveLines(out_); }
    bool setLayout(const stv::Layout& l, uint8_t ch);   // true when rewritten; display off until the next service()
    void setTimingOverride(uint8_t ld, uint8_t dd, uint8_t ch);   // Developer tuning; cleared by setOutput
    // Horizontal placement: DD from the scaler's output window and the alignment (stvDdForWindow); an invalid
    // window keeps the stvDdFor(ld) edge. Both survive setOutput; the device glue re-reads the window
    void setWindow(const StvWindow& w);
    void setHAlign(uint8_t align);        // STV_ALIGN_LEFT / CENTER / RIGHT
    uint8_t dd() const;                   // the DD in use
    static const uint32_t kSettleMs = 40;   // PLL capture after a line-duration change, before the display bit
    void powerOn(uint32_t nowMs);         // timing + full text rewrite now, display bit from service() after kSettleMs
    void powerOff();                      // display bit off first, then the idle line duration
    bool powered() const { return powered_; }
    void setAlwaysOn(bool on) { alwaysOn_ = on; }   // A/B: powerOff keeps the display bit and LD
    bool alwaysOn() const { return alwaysOn_; }
    void requestOn() { wantOn_ = true; }  // compat drawing: applied by service()
    // Another screen (Developer test screen) owns the chip: the menu renderer pauses while set
    void setExclusive(bool on) { exclusive_ = on; gen_++; }
    bool exclusive() const { return exclusive_; }
    void touch() { gen_++; }              // a writer other than the renderer changed the chip content
    uint16_t generation() const { return gen_; }   // bumps on layout rewrites, power-on and touch()
    void service(uint32_t nowMs);         // end of loop: pending power-on, flush, delayed display bit
    void flush();
    StvShadow& shadow() { return shadow_; }
    const StvTiming& timing() const { return t_; }
    StvOutStd output() const { return out_; }
    uint32_t xtal() const { return xtal_; }
    uint32_t lineHz() const { return fh_ ? fh_ : stvLineHz(out_); }
    // Glyphs on demand: glyph() returns the cell code of UDC glyph g (StvUdc), uploading it into a free slot when
    // needed; glyphs requested since the last glyphFrame() are never evicted (all slots taken: a space, 0x9A).
    // The renderer calls glyphFrame() before drawing a screen. Slots are forgotten on begin() and powerOn().
    void glyphFrame();
    uint8_t glyph(uint8_t g);
    uint16_t glyphMisses() const { return glyphMisses_; }   // glyphs drawn as a space (no free slot)
    void romDump(void (*line)(const char* text, void* ctx), void* ctx,   // 0x2000..0x32FF, 32 bytes per line
                 uint16_t from = 0x2000, uint16_t lines = 152);              // or `lines` lines from `from`
private:
    void reg(uint8_t n, uint8_t v);
    void writeActiveTiming();
    OsdBus& bus_;
    StvShadow shadow_;
    stv::Layout layout_;
    uint8_t ch_ = 21;
    StvOutStd out_ = OUT_480P;
    StvTiming t_ = {};

    uint32_t xtal_ = STV_XTAL_HZ;
    uint32_t fh_ = 0;                     // measured line rate (bypass), 0 = nominal
    uint16_t active_ = 0;                 // measured visible lines (bypass), 0 = stvActiveLines
    uint8_t fm_ = 10;
    bool powered_ = false, displayOn_ = false, alwaysOn_ = false, wantOn_ = false, displayPending_ = false;
    uint32_t displayAt_ = 0, lastService_ = 0;
    bool override_ = false;
    bool exclusive_ = false;
    uint16_t gen_ = 0;
    uint8_t ovLd_ = 0, ovDd_ = 0, ovCh_ = 0;
    StvWindow win_ = {0, 0, 0, 0, false};
    uint8_t align_ = STV_ALIGN_CENTER;
    void applyDd();                       // reg 1 when powered and changed
    uint8_t ddWritten_ = 0xFF;
    struct GlyphSlot { uint8_t g; bool pinned; uint16_t used; };   // g 0xFF: empty
    GlyphSlot slots_[stv::kUdcSlots];
    uint16_t glyphClock_ = 0, glyphMisses_ = 0;
    void forgetGlyphs();
};

// Developer test screen: full layout with title, arrows, 7 rows and help, every colour, every UDC
void stvDrawTestScreen(Stv9426& d);
const char* stvOutStdName(StvOutStd s);
