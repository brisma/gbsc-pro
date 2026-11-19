// STV9426 output timing: PLL limits (datasheet §VIII.1) and per-output values (pure, host-testable)
#pragma once
#include <stdint.h>

#define STV_XTAL_HZ 8000000UL
enum StvOutStd : uint8_t { OUT_480P, OUT_576P, OUT_720P60, OUT_720P50, OUT_960P60, OUT_960P50, OUT_1024P60,
                           OUT_1024P50, OUT_1080P60, OUT_1080P50, OUT_15K60, OUT_15K50, OUT_BYPASS, OUT_1200P60, OUT_1200P50,
                           OUT_STD_COUNT };
struct StvTiming { uint8_t ld, pp, dd, chMenu, topMenu; bool zoomMenu; };
StvOutStd stvOutStdOf(uint8_t presetID);       // GBS preset id → standard (unknown → OUT_480P)
uint32_t stvLineHz(StvOutStd s);               // nominal F_H
uint16_t stvActiveLines(StvOutStd s);          // visible lines of the output (OSD overlay placement)
uint8_t stvFm(uint32_t xtalHz);                // ceil(100 MHz / xtal) - 3, clamped 0..15 (8 MHz → 10)
uint8_t stvLdFor(uint32_t fh, uint32_t xtalHz, uint8_t fm);   // largest LD in 32..63 within the PLL limits
uint8_t stvPp(uint8_t fm, uint32_t xtalHz, uint8_t ld, uint32_t fh);
uint8_t stvDdFor(uint8_t ld);                  // left edge of LD 63 / DD 38 as a fraction of the line, moved
                                               // left when 30 cells would not end within 95% of the line
bool stvWithinPll(uint8_t ld, uint32_t fh, uint32_t xtalHz, uint8_t fm);
// TV5725 output window (VDS registers, scaler pixels): line length, H-SYNC falling edge (VDS_HS_SP: the sync is
// high from HS_ST to HS_SP and the STV9426 locks on the falling edge), first active and first blanked pixel
struct StvWindow { uint16_t htotal, hsSp, act0, act1; bool valid; };
enum : uint8_t { STV_ALIGN_LEFT, STV_ALIGN_CENTER, STV_ALIGN_RIGHT };
// DD that puts the 30-cell panel at the left (5 % margin), centre or right (5 % margin) of the active window;
// refOffsetPx: data-path delay between VDS_HS_SP and the edge the chip sees. No usable window: stvDdFor(ld)
int stvDdForWindow(uint8_t ld, const StvWindow& w, uint8_t align, int16_t refOffsetPx);
// An HD video source passed through (720p, 1080i, 1080p) has no scaler window: the standard timing of its format
// stands for it, from the source's lines (a frame's, or a field's for 1080i) and line rate. The picture starts a back
// porch after the end of the sync pulse, the edge the chip locks on. false (w.valid false): no rate, or another
// format (the fixed stvDdFor placement stays)
bool stvBypassWindow(uint16_t lines, uint32_t fhHz, StvWindow& w);
// The VGA pass-through (VGA upscaling off) has no scaler window either: a VGA mode found by its lines and refresh
// rate gets its standard timing (VESA DMT; 720p CTA-861). The picture starts sync + back porch after the start of
// the sync, the edge the chip locks on with a negative sync. hstart, hactive in the mode's pixels (htotal a line);
// vstart, vactive in lines (vstart from the start of the vertical sync)
struct StvVgaMode { uint16_t lines; uint8_t hz; uint16_t htotal, hsync, hstart, hactive, vstart, vactive; };
// The mode of a source of these lines (a frame's, sync to sync) and line rate: false for a mode not in the table
// (the fixed stvDdFor placement and the lines counted from the sync stay). syncPermille: the measured sync pulse in
// thousandths of the line, 0 = not known; it picks between modes of the same lines and rate (VESA 640x480 sync 12 %,
// the Dreamcast's 7.5 %), the first one when not known
bool stvVgaPassMode(uint16_t lines, uint32_t fhHz, uint16_t syncPermille, StvVgaMode& m);
// Its window (w.valid false: not in the table)
bool stvVgaPassWindow(uint16_t lines, uint32_t fhHz, uint16_t syncPermille, StvWindow& w);
uint16_t stvVgaSyncPermille(const StvVgaMode& m);   // the mode's sync pulse in thousandths of the line
uint8_t stvVgaPassModeCount();
StvVgaMode stvVgaPassModeAt(uint8_t i);
// Line rate from the TV5725's HPERIOD_IF (the line period in counts of the 27 MHz oscillator / 4): the VGA pass-through
// has no field rate to time, this register still follows the source. 0 outside 15..100 kHz (511 = overflowed). One
// count is 0.5 % at 31 kHz, 1.2 % at 80 kHz: inside the margin of the PLL limit (stvWithinPll)
uint32_t stvLineHzFromIfPeriod(uint16_t hperiodIf);
StvTiming stvTimingFor(StvOutStd s, uint32_t xtalHz);
// fhHz 0 = the nominal rate. Bypass (the output is the source): activeLines, the visible lines of a field, give the
// rows their height (18 lines at 480, as the scaled outputs); 0 = not known, 480p rows
StvTiming stvTimingForRate(StvOutStd s, uint32_t fhHz, uint32_t xtalHz, uint16_t activeLines = 0);
// A bypass whose line rate could not be measured when the output changed is asked again every kEveryMs, kTries times
struct StvRateRetry {
    static const uint32_t kEveryMs = 2000;
    static const uint8_t kTries = 3;
    void start(uint32_t now) { left_ = kTries; at_ = now; }
    void stop() { left_ = 0; }
    bool due(uint32_t now) {
        if (!left_ || now - at_ < kEveryMs) return false;
        left_--;
        at_ = now;
        return true;
    }
private:
    uint8_t left_ = 0;
    uint32_t at_ = 0;
};
// One pass of the watch that keeps the menu on the scaler's output. presetChanged: another output since the last
// pass; retryDue: the bypass's line rate is asked again now (StvRateRetry); bypass / haveRate: the output is a bypass
// and its line rate could be read. place: set the timing for this output now; windowNow: read its window at once (a
// rate that came late; a new output's window is read once its registers are written); startRetry: ask for the rate
// again later.
struct StvOutputPass { bool place, windowNow, startRetry; };
inline StvOutputPass stvOutputPass(bool presetChanged, bool retryDue, bool bypass, bool haveRate) {
    StvOutputPass p;
    p.place = presetChanged || (retryDue && haveRate);     // a new output always; a retry only with its rate
    p.windowNow = !presetChanged && p.place;
    p.startRetry = presetChanged && bypass && !haveRate;
    return p;
}
// A scaled output's picture, vertically, from the scaler's registers (VDS_VS_ST/SP, VDS_DIS_VB_ST/SP, VDS_VSYNC_RST):
// first = lines from the end of the vertical sync (the shorter of the two spans between its edges) to the first
// visible line, active = visible lines. false: not a usable frame (the menu keeps the output's nominal lines)
bool stvVdsLines(uint16_t vsSt, uint16_t vsSp, uint16_t disVbSt, uint16_t disVbSp, uint16_t vTotal, uint16_t& first,
                 uint16_t& active);
// A bypass source may change format under the same preset (the VGA pass-through stays preset 0x22 from 640x480 to
// 1024x768): read again every kEveryMs, a reading far from the placed one (lines by more than 3, line rate by more
// than 2 %) that agrees with the reading before it asks for a new placement. A reading without a rate is skipped
struct StvSourceWatch {
    static const uint32_t kEveryMs = 1000;
    bool due(uint32_t now) {
        if (now - at_ < kEveryMs) return false;
        at_ = now;
        return true;
    }
    bool changed(uint16_t lines, uint32_t fh, uint16_t placedLines, uint32_t placedFh);
private:
    uint16_t lastLines_ = 0;
    uint32_t lastFh_ = 0;
    uint32_t at_ = 0;
};
// PLL time constants (datasheet §VIII.2, Table 2): A/B valid for this line duration
bool stvTimeConstantsOk(uint8_t a, uint8_t b, uint8_t ld);
const uint8_t STV_LOCK_TC = 0x02;      // 0x3FF4: AS 0, BS 2
const uint8_t STV_CAPTURE_TC = 0x24;   // 0x3FF5: AF 2, BF 4 (datasheet's recommended capture constants)
const uint8_t STV_CH_COMPAT = 21, STV_LD_IDLE = 32;
