// WiFi optimization: the TV5725 SDRAM clock
// that disturbs the ESP8266 radio least, judged by ICMP bursts to the gateway. Pure and host-tested; the device glue
// (wifi-tune-device.cpp) sends the bursts, writes PLL_MS and feeds the state. Clocks are PLL_MS codes (0..7).
#pragma once
#include <stdint.h>

static const uint8_t kWifiBurstCount = 20;      // echoes per burst
static const uint16_t kWifiBurstGapMs = 50;
static const uint16_t kWifiSettleMs = 500;      // after a clock change, before the burst
static const uint16_t kWifiLostMs = 250;        // a lost (or later) echo counts as this
static const uint16_t kWifiGoodP90Ms = 25;      // the preset clock is fine: stop there
static const uint16_t kWifiSlowP90Ms = 40;      // even the kept clock is slow: suggest another router channel
static const uint8_t kWifiClock144 = 4, kWifiClock162 = 3;   // the candidates, safe on every preset
static const uint8_t kWifiMaxAborts = 3;        // bursts dropped in a row before the round ends on the preset clock

struct WifiBurst { uint8_t sent, got; uint16_t p50x10, p90x10, maxx10; };   // ms x 10

// rttUs[i] == 0: lost; later than kWifiLostMs counts as kWifiLostMs
WifiBurst wifiBurstStats(const uint32_t* rttUs, uint8_t count);

// MEASURE: a burst on the current clock; SET_MEASURE: write the clock, settle, burst; APPLY: write it, no burst
enum : uint8_t { WT_NONE, WT_MEASURE, WT_SET_MEASURE, WT_APPLY };
struct WifiTuneAction { uint8_t kind; uint8_t clock; };

// One session (power-on to power-off): results per clock for the current channel, a fixed winner (144/162) once one
// beats a slow preset clock by half; next() is what the glue does when it is idle
class WifiTune {
public:
    WifiTune();
    void setEnabled(bool on);                            // system.wifi_optimize; re-enabling starts a new round
    void setLink(bool station, uint8_t channel);         // the channel counts only while connected
    void setPicture(bool stable, uint8_t presetClock);   // a new preset clock: the glue applied clockFor() at load
    void setBusy(bool busy);                             // update, OTA, WiFi scan
    void setBypass(bool bypass);                         // HD / RGBHV bypass: PLL_MS is theirs, nothing to do
    void presetLoaded(uint8_t presetClock);              // a load wrote clockFor() and reset the SDRAM: the hardware is
                                                         // there; a round that ended on dropped bursts may try again
    WifiTuneAction next();
    void burstDone(const WifiBurst& b);
    void burstAborted();                                 // dropped: the hardware stays on the burst's clock
    uint8_t clockFor(uint8_t presetClock) const;
    // No signal (upstream's reset writes FB): the session's winner, else 144 MHz; FB (2) with the option off
    uint8_t idleClock(bool enabled) const { return !enabled ? 2 : winner_ < 8 ? winner_ : kWifiClock144; }
    uint8_t appliedClock() const { return applied_; }    // what the tuner wrote or expects on the hardware
    bool slow() const { return slow_; }
    // every clock tried is slow with this picture on this channel: another router channel is the cure
    bool slowChannel() const { return en_ && slow_ && roundComplete(); }
    bool gaveUp() const { return gaveUp_; }
    bool stalled() const { return stalled_; }            // the round ended after kWifiMaxAborts drops in a row
    bool roundComplete() const;                          // nothing left to measure on this channel (not just waiting)
    uint8_t channel() const { return ch_; }
    uint16_t p90x10(uint8_t clock) const { return clock < 8 ? p90_[clock] : 0xFFFF; }
private:
    void newRound();
    WifiTuneAction request(uint8_t clock);               // a burst on clock: MEASURE when already there
    WifiTuneAction apply(uint8_t clock);                 // APPLY when the hardware is elsewhere, else NONE
    bool en_ = false, station_ = false, stable_ = false, busy_ = false, haveCh_ = false;
    uint8_t ch_ = 0, pc_ = 0, applied_ = 0, winner_ = 0xFF;
    uint16_t p90_[8];
    bool slow_ = false, gaveUp_ = false, firstBurst_ = true, retried_ = false, restore_ = false, stalled_ = false;
    bool bypass_ = false;
    uint8_t aborts_ = 0;                                 // bursts dropped in a row
    bool pending_ = false;                               // a burst requested, its result not in yet
    uint8_t pendingClock_ = 0;
};

// Profile capture: a clock the tuner applied is stored as the preset's own (manual clocks stay as they are)
uint8_t wifiTuneCaptureS040(uint8_t liveS040, uint8_t presetClock, uint8_t tunerClock);
