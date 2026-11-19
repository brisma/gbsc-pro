// Picture geometry: the shape and size of the scaled picture as register arithmetic, no register access.
// The output side of the scaler places the picture with a shown window (VDS_DIS_HB/VB), a memory blank that reads
// each line from memory (VDS_HB), a horizontal and a vertical scale and the display clock. One table per output
// resolution and rate holds a fixed picture; the steps here turn it into another one and back.
#pragma once
#include <stdint.h>

// Output clocks between the memory blank and its pixels reaching the output. The delay is the scaler's own and does
// not scale with the picture: a memory blank edge p stands for the pixel at p + kGeoLat.
static const int16_t kGeoLat = 56;

struct GeoRegs {
    uint16_t hTotal;              // VDS_HSYNC_RST
    uint16_t hsSt, hsSp;          // VDS_HS_ST, VDS_HS_SP
    uint16_t hbSt, hbSp;          // VDS_HB_ST, VDS_HB_SP: the memory blank, pixels are read from SP to ST
    uint16_t disHbSt, disHbSp;    // VDS_DIS_HB_ST, VDS_DIS_HB_SP: the shown window, SP to ST
    uint16_t hScale;              // VDS_HSCALE: 1023 is 1:1, 512 is x2
    bool hScaleByps;              // VDS_HSCALE_BYPS
    uint16_t vTotal;              // VDS_VSYNC_RST, read only
    uint16_t vScale;              // VDS_VSCALE on the doubled frame: 1023 is 1:1, 512 is x2
    uint16_t vbSp;                // VDS_VB_SP
    uint16_t disVbSt, disVbSp;    // VDS_DIS_VB_ST, VDS_DIS_VB_SP: the shown window, SP to ST
    uint16_t ifVbSt, ifVbSp;      // IF_VB_ST, IF_VB_SP: which source lines enter the memory
    uint8_t clock;                // PLL648_CONTROL_01: 0x65 81 MHz, 0x85 108 MHz, 0xA5 162 MHz
    uint8_t mark, bar;            // GBS_GEOMETRY, GBS_GEOMETRY_BAR
    uint8_t clamps;               // edges the last step held inside the line
};

// The display clock one step up (81 -> 108 -> 162 MHz) with the same picture and line rate: every horizontal
// position follows the clock, the scale makes up for it. False (nothing changed) when the code has no step up.
bool geoClockStep(GeoRegs& r);

// The line made total clocks long with the same picture and line rate, on a clock that follows (the clock
// generator): every horizontal position and the scale follow, the clock code stays. False (nothing changed) when the
// line is that long already or total is outside the register.
bool geoRetime(GeoRegs& r, uint16_t total);

// The line the PRO's HDMI converter samples for a table, where the table's own differs: it takes the 1200p tables for
// CVT-RB 1920x1200 and samples 2080 clocks per line, so a line of another length is resampled (uneven columns on fine
// patterns). hz: the clock generator's frequency for that line at the table's line rate; own: the table's line, the
// one the scaler's own clocks (81, 108, 129.6, 162 MHz) give that rate with, kept without a clock generator. False for
// the other tables.
struct GeoLine { uint16_t total; uint32_t hz; uint16_t own; };
bool geoConverterLine(uint8_t tableId, GeoLine& out);

// The picture f times as wide about the centre of its shown window; the window and the memory blank follow. The
// horizontal scale ends at 1:1, so a narrower picture may need the clock stepped up first. False (nothing changed)
// when the width cannot be reached or the window is not a plain SP < ST one.
bool geoWidth(GeoRegs& r, float f);

// ---- Pictures: what the settings ask for, per table ----

enum GeoAspect : uint8_t { GEO_AUTO, GEO_FORCE43, GEO_FULL };              // output.aspect
enum GeoStyle50 : uint8_t { GEO_PAL_TV, GEO_FULL_HEIGHT, GEO_AS_60HZ };    // output.style_50hz
// standIn: the table stands in for the other resolution of its pair. Matching the output to the source loads the
// 1024p table (a 5:4 frame) for a 50 Hz source when 960p (4:3) was chosen, and the 960p table for a 60 Hz source
// when 1024p was chosen; a screen shows either in the frame of the chosen one, so the width follows that frame.
struct GeoState { uint8_t aspect; bool integer; uint8_t style50; bool standIn; };

// The vertical part of a picture against its table: the scale (0: the table's own), and the row's own move: for a
// picture taller than the window the lines its top moves down and the half source lines the input skips so that it
// stays centred; for a shorter one (bars above and below) the lines the window and its bars move down and the half
// source lines the capture moves, measured where a table's window is not centred on the lines a screen shows
struct GeoVert { uint16_t vScale; int8_t top; int8_t ifVb; };
struct GeoTarget { GeoVert vert; uint16_t widthPm; };   // the width against the table's, per mille

// The picture of a state on the table tableId (GBS_PRESET_ID). False for a table the steps do not handle.
bool geoTarget(uint8_t tableId, GeoState s, GeoTarget& out);

// The state the registers end in for these settings: the 50Hz style only counts on a 50 Hz table, Full height with
// Integer scale is the PAL TV picture (it cannot fill the height and be a whole multiple), and only the two tables
// that matching swaps in can stand in.
GeoState geoNormalise(uint8_t tableId, GeoState s);

// GBS_GEOMETRY for a normalised state: bits 0-1 the aspect, bit 2 Integer scale, bits 3-4 the 50Hz style plus one
// on a 50 Hz table, bit 5 a table standing in for the chosen one. 0 is a table as loaded.
uint8_t geoMark(uint8_t tableId, GeoState s);

struct GeoResult {
    bool acted;                   // false: not a table the steps handle, nothing read or written
    bool changed;                 // a register or the mark differs
    bool failed;                  // the width could not be reached: everything left as it was
    uint16_t widthPm;             // the width asked for against the picture the registers held
    uint8_t clockFrom, clockTo;
    uint16_t vScale;
};

// From the picture the registers hold (r.mark, r.bar) to the one the settings ask for.
GeoResult geometryApply(GeoRegs& r, uint8_t tableId, GeoState want);
