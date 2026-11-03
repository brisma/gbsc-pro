#include "../input/input-types.h"
#include "adv-state.h"
#include <string.h>

// Decoder levels 0..100 -> ADV7280 BCSH register values: 50 = neutral 128, 100 = 254
static uint8_t advLevel(uint8_t v) {
    if (v > 100) v = 100;
    return v <= 50 ? (uint8_t)(v * 128 / 50) : (uint8_t)(128 + (v - 50) * 126 / 50);
}

void advStateFromOptions(const userOptions& u, bool termination, bool listen, AdvState& s) {
    memset(&s, 0, sizeof(s));
    s.version = ADV_STATE_VERSION;
    s.input = inputTypeValid(u.activeInputType) ? u.activeInputType : InputTypeRGBs;
    s.syncStripper = u.advSyncStripper ? 1 : 0;
    s.termination = termination ? 1 : 0;
    s.listen = listen && s.input == ADV_INPUT_RGBS ? 1 : 0;
    s.formatSv = u.svVideoFormat < ADV_FORMAT_COUNT ? u.svVideoFormat : 0;
    s.formatAv = u.avVideoFormat < ADV_FORMAT_COUNT ? u.avVideoFormat : 0;
    s.i2p = u.advI2P ? 1 : 0;
    s.smooth = u.advSmooth ? 1 : 0;
    s.ace = u.advACE ? 1 : 0;
    s.aceLumaGain = u.advACELumaGain;
    s.aceChromaGain = u.advACEChromaGain;
    s.aceChromaMax = u.advACEChromaMax;
    s.aceGammaGain = u.advACEGammaGain;
    s.aceResponseSpeed = u.advACEResponseSpeed;
    s.brightness = advLevel(u.advBrightness);
    s.contrast = advLevel(u.advContrast);
    s.saturation = advLevel(u.advSaturation);
    s.hue = advLevel(u.advHue);
    s.filterY = u.advFilterYShaping;
    s.filterC = u.advFilterCShaping;
    s.filterWy = u.advFilterWYShaping;
    s.filterWyOverride = u.advFilterWYOverride;
    s.combNtsc = u.advFilterCombNTSC;
    s.combPal = u.advFilterCombPAL;
    s.combLumaNtsc = u.advCombLumaModeNTSC;
    s.combChromaNtsc = u.advCombChromaModeNTSC;
    s.combTapsNtsc = u.advCombChromaTapsNTSC;
    s.combLumaPal = u.advCombLumaModePAL;
    s.combChromaPal = u.advCombChromaModePAL;
    s.combTapsPal = u.advCombChromaTapsPAL;
    s.dnr = u.advDnr ? 1 : 0;
    s.cti = u.advCti ? 1 : 0;
}
