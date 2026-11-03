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
StvTiming stvTimingFor(StvOutStd s, uint32_t xtalHz);
StvTiming stvTimingForRate(StvOutStd s, uint32_t fhHz, uint32_t xtalHz);   // fhHz 0 = the nominal rate
// PLL time constants (datasheet §VIII.2, Table 2): A/B valid for this line duration
bool stvTimeConstantsOk(uint8_t a, uint8_t b, uint8_t ld);
const uint8_t STV_LOCK_TC = 0x02;      // 0x3FF4: AS 0, BS 2
const uint8_t STV_CAPTURE_TC = 0x24;   // 0x3FF5: AF 2, BF 4 (datasheet's recommended capture constants)
const uint8_t STV_CH_COMPAT = 21, STV_LD_IDLE = 32;
