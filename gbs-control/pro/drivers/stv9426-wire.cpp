// STV9426 over Wire (device only)
#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include "../../options.h"
#include "../platform/platform-caps.h"
#include "../../tv5725.h"
#include "stv9426-device.h"
#include "../log/log.h"

extern runTimeOptions* rto;
typedef TV5725<GBS_ADDR> GBS;
static const uint8_t kStvAddr = 0x5D;   // STV9426 I2C address
uint32_t osdSourceLineHz();   // gbs-control.ino: source lines x field rate, 0 when unknown
uint16_t profileSourceLines();   // gbs-control.ino: VPERIOD_IF, lines per source field
uint16_t osdSourceSyncPermille();   // gbs-control.ino: H-SYNC pulse in thousandths of the line, 0 when unknown
bool updateInProgress();            // no scaler reads while an upload writes the flash

class WireOsdBus : public OsdBus {
public:
    bool write(uint16_t addr, const uint8_t* data, uint8_t len) override {
        if (!hwHas(CAP_OSD)) return true;
        do {   // the Wire buffer is 128 bytes: 2 address bytes + up to 126 data bytes per transaction
            uint8_t n = len > 126 ? 126 : len;
            Wire.beginTransmission(kStvAddr);
            Wire.write((uint8_t)(addr & 0xFF));
            Wire.write((uint8_t)((addr >> 8) & 0x3F));
            Wire.write(data, n);
            if (Wire.endTransmission() != 0) return false;
            addr += n; data += n; len -= n;
        } while (len);
        return true;
    }
    void read(uint16_t addr, uint8_t* out, uint8_t len) override {
        memset(out, 0, len);
        if (!hwHas(CAP_OSD)) return;
        Wire.beginTransmission(kStvAddr);
        Wire.write((uint8_t)(addr & 0xFF));
        Wire.write((uint8_t)((addr >> 8) & 0x3F));
        Wire.endTransmission();
        Wire.requestFrom((uint8_t)kStvAddr, len);
        for (uint8_t i = 0; i < len && Wire.available(); i++) out[i] = (uint8_t)Wire.read();
    }
};

static WireOsdBus gBus;
static Stv9426 gDriver(gBus);
static bool gStarted = false;

Stv9426& osdDriver() { return gDriver; }
OsdBus& osdBus() { return gBus; }

void osdDriverBegin() {
    if (!hwHas(CAP_OSD)) return;
    gDriver.begin(STV_XTAL_HZ);
    gDriver.setOutput(stvOutStdOf(rto->presetID));
    gStarted = true;
}

// The lines and the line rate of the source last measured in a bypass (osdDriverLoop): the window of its format
static uint16_t gBypassLines = 0;
static uint32_t gBypassFh = 0;
static uint16_t gBypassSync = 0;   // VGA pass-through: the sync pulse, thousandths of the line

// The scaler's output window: the STV9426 locks on the falling edge of HS_OUT, which is VDS_HS_SP, and counts lines
// from the end of the vertical sync (stvVdsLines). Bypass presets have
// no VDS window: an HD video format gets its standard one (stvBypassWindow), a VGA mode in the pass-through its VESA
// one (stvVgaPassWindow), anything else the fixed stvDdFor edge
void osdReadWindow() {
    if (!gStarted) return;
    StvWindow w = {0, 0, 0, 0, false};
    if (!rto->outModeHdBypass && rto->presetID != 0x21 && rto->presetID != 0x22) {
        w.htotal = GBS::VDS_HSYNC_RST::read();
        w.hsSp = GBS::VDS_HS_SP::read();
        w.act0 = GBS::VDS_DIS_HB_SP::read();
        w.act1 = GBS::VDS_DIS_HB_ST::read();
        w.valid = w.htotal > 0 && w.hsSp < w.htotal && w.act0 < w.htotal && w.act1 < w.htotal;
        uint16_t first = 0, active = 0;   // and the picture's lines: the menu centred on them, not on the table's
        StvOutStd o = stvOutStdOf(rto->presetID);   // (15 kHz outputs count fields: their nominal lines stay)
        if (o != OUT_15K60 && o != OUT_15K50 &&
            stvVdsLines(GBS::VDS_VS_ST::read(), GBS::VDS_VS_SP::read(), GBS::VDS_DIS_VB_ST::read(),
                        GBS::VDS_DIS_VB_SP::read(), GBS::VDS_VSYNC_RST::read(), first, active))
            gDriver.setLines(first, active);
    } else if (rto->presetID == 0x22) {
        stvVgaPassWindow(gBypassLines, gBypassFh, gBypassSync, w);
    } else {
        stvBypassWindow(gBypassLines, gBypassFh, w);
    }
    gDriver.setWindow(w);
}

// A window to read later: a preset's registers are still being written when the change is known
static uint32_t gWindowAt = 0;
static bool gWindowDue = false;
void osdWindowSoon() { gWindowDue = true; gWindowAt = millis() + 500; }

// Bypass: the output follows the source (rate and lines; about 96 % of the lines are visible, a VGA mode of the table
// its own). false: no line rate yet
static bool placeBypass(uint8_t preset, StvOutStd s, uint16_t lines, uint32_t fh) {
    gBypassLines = s == OUT_BYPASS ? lines : 0;
    gBypassFh = fh;
    gBypassSync = preset == 0x22 ? osdSourceSyncPermille() : 0;
    StvVgaMode vm;
    bool std = preset == 0x22 && stvVgaPassMode(lines, fh, gBypassSync, vm);
    if (std)
        gDriver.setOutput(s, fh, vm.vactive, vm.vstart);
    else
        gDriver.setOutput(s, fh, lines >= 200 && lines <= 1250 ? (uint16_t)(lines * 96 / 100) : 0);
    if (preset == 0x22 && fh) {
        if (std)
            logPrintf(LC_MENU, LL_INFO,
                      PSTR("osd: VGA pass-through %u lines %lu Hz sync %u/1000: %ux%u %u Hz timing, line %u"),
                      lines, (unsigned long)fh, gBypassSync, vm.hactive, vm.vactive, vm.hz, vm.htotal);
        else
            logPrintf(LC_MENU, LL_INFO,
                      PSTR("osd: VGA pass-through %u lines %lu Hz sync %u/1000: no standard mode, fixed place"),
                      lines, (unsigned long)fh, gBypassSync);
    }
    return fh != 0;
}

void osdDriverLoop() {
    if (!gStarted) return;
    static uint8_t lastPreset = 0xFF;
    uint32_t now = millis();
    if (gWindowDue && (int32_t)(now - gWindowAt) >= 0) { gWindowDue = false; osdReadWindow(); }
    static StvRateRetry rateRetry;
    static StvSourceWatch sourceWatch;
    bool again = rateRetry.due(now);
    if (rto->presetID != lastPreset || again) {
        if (rto->presetID != lastPreset) {
            lastPreset = rto->presetID;
            osdWindowSoon();               // the new preset's window, once its registers are written
            rateRetry.stop();
        }
        StvOutStd s = stvOutStdOf(lastPreset);
        // A rate that cannot be measured yet leaves the menu at its widest: asked again a few times
        uint16_t lines = s == OUT_BYPASS ? profileSourceLines() : 0;
        uint32_t fh = s == OUT_BYPASS ? osdSourceLineHz() : 0;
        if (s == OUT_BYPASS && !fh && !again) rateRetry.start(now);
        if (fh) rateRetry.stop();
        if (!again || fh) {
            placeBypass(lastPreset, s, lines, fh);
            if (again) osdReadWindow();   // the rate came late: place the panel with it now
        }
    } else if (lastPreset == 0x22 && !updateInProgress() && sourceWatch.due(now)) {
        // the VGA pass-through, another source mode (640x480 to 800x600): three register reads a second (the HD
        // bypass's rate is a blocking field measurement, not read here)
        uint16_t lines = profileSourceLines();
        uint32_t fh = osdSourceLineHz();
        if (sourceWatch.changed(lines, fh, gBypassLines, gBypassFh) && placeBypass(lastPreset, OUT_BYPASS, lines, fh))
            osdReadWindow();
    }
    gDriver.service(now);
}
#endif
