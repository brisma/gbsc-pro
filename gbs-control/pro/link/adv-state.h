// AdvState for the ADV Controller built from the user options (pure; the ESP is the source of truth)
#pragma once
#include "adv-link.h"
#include "../../options.h"

// termination: runtime 75R sync termination (RGBs only); listen: the input search's decoder on pin 20 (RGBs only, never
// RGB SoG). Out-of-range options are clamped so the ADV Controller always accepts the frame (input outside 1..6 ->
// RGBs, format > 12 -> Auto)
void advStateFromOptions(const userOptions& u, bool termination, bool listen, AdvState& s);
