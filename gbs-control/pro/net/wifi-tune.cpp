#include "wifi-tune.h"

WifiBurst wifiBurstStats(const uint32_t* rttUs, uint8_t count) {
    WifiBurst b = {};
    if (count > 64) count = 64;
    b.sent = count;
    uint32_t v[64];
    const uint32_t lostUs = (uint32_t)kWifiLostMs * 1000;
    for (uint8_t i = 0; i < count; i++) {
        if (rttUs[i]) b.got++;
        v[i] = rttUs[i] && rttUs[i] < lostUs ? rttUs[i] : lostUs;
    }
    for (uint8_t i = 1; i < count; i++) {   // insertion sort: 20 values
        uint32_t x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    if (!count) return b;
    auto at = [&](uint8_t q) { return (uint16_t)(v[(count - 1) * q / 100] / 100); };
    b.p50x10 = at(50);
    b.p90x10 = at(90);
    b.maxx10 = at(100);
    return b;
}

WifiTune::WifiTune() { newRound(); }

void WifiTune::newRound() {
    for (uint16_t& p : p90_) p = 0xFFFF;
    winner_ = 0xFF;
    slow_ = gaveUp_ = retried_ = pending_ = stalled_ = false;
    firstBurst_ = true;
    aborts_ = 0;
}

void WifiTune::setEnabled(bool on) {
    if (on && !en_) newRound();
    if (!on && en_) pending_ = false;
    en_ = on;
}

void WifiTune::setLink(bool station, uint8_t channel) {
    station_ = station;
    if (!station) return;
    if (haveCh_ && channel != ch_) newRound();
    ch_ = channel;
    haveCh_ = true;
}

void WifiTune::setPicture(bool stable, uint8_t presetClock) {
    if (presetClock != pc_) {
        pc_ = presetClock;
        applied_ = clockFor(presetClock);   // the load wrote it (doPostPresetLoadSteps)
        pending_ = false;
    }
    stable_ = stable;
}

void WifiTune::setBusy(bool busy) { busy_ = busy; }

void WifiTune::setBypass(bool bypass) { bypass_ = bypass; }

void WifiTune::presetLoaded(uint8_t presetClock) {
    pc_ = presetClock;
    applied_ = clockFor(presetClock);
    pending_ = restore_ = stable_ = false;
    slow_ = false;                    // judged again for this picture
    stalled_ = false;                 // drops before this picture (the input search stalls the loop) do not count
    aborts_ = 0;
}

bool WifiTune::roundComplete() const {
    if (gaveUp_ || stalled_) return true;
    if (pending_) return false;
    if (winner_ < 8) return true;
    if (p90_[pc_] == 0xFFFF) return false;
    if (p90_[pc_] <= kWifiGoodP90Ms * 10) return true;
    const uint8_t cands[2] = {kWifiClock144, kWifiClock162};
    for (uint8_t c : cands)
        if (c != pc_ && p90_[c] == 0xFFFF) return false;
    return true;
}

uint8_t WifiTune::clockFor(uint8_t presetClock) const { return en_ && winner_ < 8 ? winner_ : presetClock; }

WifiTuneAction WifiTune::request(uint8_t clock) {
    pending_ = true;
    pendingClock_ = clock;
    WifiTuneAction a = {clock == applied_ ? WT_MEASURE : WT_SET_MEASURE, clock};
    applied_ = clock;
    return a;
}

WifiTuneAction WifiTune::apply(uint8_t clock) {
    if (clock == applied_) return {WT_NONE, 0};
    applied_ = clock;
    return {WT_APPLY, clock};
}

WifiTuneAction WifiTune::next() {
    if (bypass_) return {WT_NONE, 0};
    if (!en_) return apply(pc_);                          // off: the preset's own clock, at once
    if (restore_) { restore_ = false; return apply(clockFor(pc_)); }   // after an abort, not gated
    if (!station_ || !stable_ || busy_ || gaveUp_ || stalled_) return {WT_NONE, 0};
    if (pending_) return {applied_ == pendingClock_ ? WT_MEASURE : WT_SET_MEASURE, pendingClock_};
    if (p90_[pc_] == 0xFFFF) {
        if (winner_ < 8) return apply(winner_);           // a winner holds: no switch during play to measure
        return request(pc_);
    }
    if (winner_ < 8) return apply(winner_);
    if (p90_[pc_] <= kWifiGoodP90Ms * 10) { slow_ = false; return apply(pc_); }
    const uint8_t cands[2] = {kWifiClock144, kWifiClock162};
    uint8_t best = 0xFF;
    for (uint8_t c : cands) {
        if (c == pc_) continue;
        if (p90_[c] == 0xFFFF) return request(c);
        if (best == 0xFF || p90_[c] < p90_[best]) best = c;
        if (p90_[c] <= kWifiGoodP90Ms * 10 && p90_[c] <= p90_[pc_] / 2) break;   // good already: no burst on the next
    }
    if (best < 8 && p90_[best] <= p90_[pc_] / 2) {
        winner_ = best;
        slow_ = p90_[best] > kWifiSlowP90Ms * 10;
        return apply(best);
    }
    slow_ = p90_[pc_] > kWifiSlowP90Ms * 10;
    return apply(pc_);
}

void WifiTune::burstDone(const WifiBurst& b) {
    if (!pending_) return;
    pending_ = false;
    aborts_ = 0;
    if (b.got == 0 && firstBurst_) {                      // the router may not answer ping at all
        if (!retried_) { retried_ = true; pending_ = true; return; }
        gaveUp_ = true;
        return;
    }
    firstBurst_ = false;
    p90_[pendingClock_] = b.p90x10;
}

void WifiTune::burstAborted() {
    if (!pending_) return;
    pending_ = false;
    restore_ = applied_ != clockFor(pc_);
    if (++aborts_ >= kWifiMaxAborts) stalled_ = true;
}

uint8_t wifiTuneCaptureS040(uint8_t liveS040, uint8_t presetClock, uint8_t tunerClock) {
    uint8_t live = (liveS040 >> 4) & 7;
    if (live != tunerClock || tunerClock == presetClock) return liveS040;
    return (uint8_t)((liveS040 & 0x8F) | ((presetClock & 7) << 4));
}
