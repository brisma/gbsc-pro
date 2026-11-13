// AdvState for the ADV Controller built from the user options (pure; the ESP is the source of truth)
#pragma once
#include "adv-link.h"
#include "../../options.h"

// termination: the 75 ohm load on SCART pin 20 (RGBs only: advTerminationWanted, or the input search's own); listen: the input search's decoder on pin 20 (RGBs only, never
// RGB SoG). Out-of-range options are clamped so the ADV Controller always accepts the frame (input outside 1..6 ->
// RGBs, format > 12 -> Auto)
void advStateFromOptions(const userOptions& u, bool termination, bool listen, AdvState& s);
// 75 ohm sync (input.sync_75ohm) is on the board only while an RGB SCART picture is shown: the input search tells
// the cable with the termination under its own control, so every new source starts without it
bool advTerminationWanted(const userOptions& u, bool picture, bool identifying);
