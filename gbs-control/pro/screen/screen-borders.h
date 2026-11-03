// Screen > Border position: move the TV5725 display blanking window without changing its size
#pragma once
#include <stdint.h>

// blankStart/blankStop = VDS_DIS_HB_ST/SP (or VB_ST/SP), total = HTOTAL (VTOTAL): the visible window runs from
// blankStop to blankStart. Moves both by delta; false (nothing written) when the window is wrapped or empty, or the
// edge moving towards its limit would leave [margin, total] (presets often end the window at blankStart == total).
bool screenBorderMove(uint16_t blankStart, uint16_t blankStop, uint16_t total, int16_t delta, uint16_t margin,
                      uint16_t& outStart, uint16_t& outStop);
