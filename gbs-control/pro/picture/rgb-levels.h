// RGB source levels (RGBs / RGsB): the console shows a full black, then a full white screen; the job reads the
// scaler's input-stage Y on the test bus and moves the three ADC offsets (black -> Y 16) or gains (white -> Y 235)
// together. Black is the profile setting input.rgb_black (offset steps, 128 = none); white is the ADC gain, saved as
// the profile's adc_gain like the Gain stepper (it turns Auto gain off). The maths is rgb-levels-math.h.
//
// While a job runs (a second or two) nothing else may move what it measures: auto gain, the input search, profile
// loads and the auto-load check wait for rgbLevelsActive().
#pragma once
#include <stdint.h>

bool rgbLevelsActive();
void rgbLevelsLoop();                // loop(): one step of the running job
void rgbLevelsStart(bool white);     // input.rgb_set_black / input.rgb_set_white
void rgbLevelsReset();               // input.rgb_reset: black back to the boot calibration
// After a preset load (doPostPresetLoadSteps) and on APPLY_COLOR: on the RGB inputs, the offsets with the saved black
// level; while a job runs, the job's working registers instead
void rgbLevelsApplyAdc();
