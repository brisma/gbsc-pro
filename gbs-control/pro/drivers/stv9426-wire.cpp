// STV9426 over Wire (device only)
#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include "../../options.h"
#include "../platform/platform-caps.h"
#include "../../tv5725.h"
#include "stv9426-device.h"

extern runTimeOptions* rto;
typedef TV5725<GBS_ADDR> GBS;
static const uint8_t kStvAddr = 0x5D;   // STV9426 I2C address
uint32_t osdSourceLineHz();   // gbs-control.ino: source lines x field rate, 0 when unknown
uint16_t profileSourceLines();   // gbs-control.ino: VPERIOD_IF, lines per source field

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

// The lines and the line rate of the source last measured in a bypass (osdDriverLoop): the window of an HD format
static uint16_t gBypassLines = 0;
static uint32_t gBypassFh = 0;

// The scaler's output line: the STV9426 locks on the falling edge of HS_OUT, which is VDS_HS_SP. Bypass presets have
// no VDS window: an HD video format gets its standard one (stvBypassWindow), anything else the fixed stvDdFor edge
void osdReadWindow() {
    if (!gStarted) return;
    StvWindow w = {0, 0, 0, 0, false};
    if (!rto->outModeHdBypass && rto->presetID != 0x21 && rto->presetID != 0x22) {
        w.htotal = GBS::VDS_HSYNC_RST::read();
        w.hsSp = GBS::VDS_HS_SP::read();
        w.act0 = GBS::VDS_DIS_HB_SP::read();
        w.act1 = GBS::VDS_DIS_HB_ST::read();
        w.valid = w.htotal > 0 && w.hsSp < w.htotal && w.act0 < w.htotal && w.act1 < w.htotal;
    } else if (rto->presetID != 0x22) {
        stvBypassWindow(gBypassLines, gBypassFh, w);
    }
    gDriver.setWindow(w);
}

// A window to read later: a preset's registers are still being written when the change is known
static uint32_t gWindowAt = 0;
static bool gWindowDue = false;
void osdWindowSoon() { gWindowDue = true; gWindowAt = millis() + 500; }

void osdDriverLoop() {
    if (!gStarted) return;
    static uint8_t lastPreset = 0xFF;
    if (gWindowDue && (int32_t)(millis() - gWindowAt) >= 0) { gWindowDue = false; osdReadWindow(); }
    static StvRateRetry rateRetry;
    bool again = rateRetry.due(millis());
    if (rto->presetID != lastPreset || again) {
        if (rto->presetID != lastPreset) {
            lastPreset = rto->presetID;
            osdWindowSoon();               // the new preset's window, once its registers are written
            rateRetry.stop();
        }
        StvOutStd s = stvOutStdOf(lastPreset);
        // bypass: the output follows the source (rate and lines; about 96 % of the lines are visible). A rate that
        // cannot be measured yet leaves the menu at its widest: asked again a few times (not the VGA pass-through,
        // which has no pulse to time)
        uint16_t lines = s == OUT_BYPASS ? profileSourceLines() : 0;
        uint32_t fh = s == OUT_BYPASS ? osdSourceLineHz() : 0;
        if (s == OUT_BYPASS && !fh && lastPreset != 0x22 && !again) rateRetry.start(millis());
        if (fh) rateRetry.stop();
        if (!again || fh) {
            gBypassLines = s == OUT_BYPASS ? lines : 0;
            gBypassFh = fh;
            gDriver.setOutput(s, fh, lines >= 200 && lines <= 1250 ? (uint16_t)(lines * 96 / 100) : 0);
            if (again) osdReadWindow();   // the rate came late: place the panel with it now
        }
    }
    gDriver.service(millis());
}
#endif
