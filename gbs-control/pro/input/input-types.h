// Input types (input.source / activeInputType) and their names: pure, shared by the firmware, the menu and the tests
#pragma once
#include <stdint.h>

enum InputType : uint8_t {
    InputTypeRGBs = 1,      // RGB SCART (csync, or sync from CVBS through the LM1881 on the Pro)
    InputTypeRGsB = 2,      // RGB with sync on green
    InputTypeVGA = 3,       // RGBHV
    InputTypeYUV = 4,       // Component
    InputTypeSV = 5,        // S-Video (Pro: ADV7280 decoder)
    InputTypeAV = 6         // Composite (Pro: ADV7280 decoder)
};

// TV5725 sync routing groups used by the detection
enum InputSource : uint8_t { InputSourceRGBs = 0, InputSourceVGA = 1, InputSourceYUV = 2 };

inline bool inputTypeValid(uint8_t t) { return t >= InputTypeRGBs && t <= InputTypeAV; }
inline bool inputIsDecoder(uint8_t t) { return t == InputTypeSV || t == InputTypeAV; }
// TV5725 ADC input of a path: 1 = RGB header (RGB SCART, RGB SoG, VGA), 0 = YPbPr (Component, and S-Video/Composite
// through the ADV7391 on the Pro)
inline uint8_t inputPathAdc(uint8_t t) { return t >= InputTypeRGBs && t <= InputTypeVGA ? 1 : 0; }
inline uint8_t getInputSourceFromType(uint8_t t) {
    return t == InputTypeRGBs || t == InputTypeRGsB ? InputSourceRGBs : t == InputTypeVGA ? InputSourceVGA : InputSourceYUV;
}
// A detection pass on the RGB ADC input found a stable HS and waited for a VS: is composite sync looked for? Without
// a VS (or on a path that never carries one) where csync may arrive: the standard board in Auto (anyRgb), RGB SCART
// and RGB SoG, and the Pro's VGA connector when VGA was chosen by hand (vgaByHand: its HS pin also takes RGBS, HD15
// cables with csync). Not in Auto: a switched-off device left on the VGA connector returns the picture of a SCART
// console (the R/G/B wires are shared) as pulses on its HS wire, which read as composite sync
inline bool inputDetectChecksRgbs(bool vsync, bool rgbHvAllowed, bool anyRgb, bool vgaByHand, uint8_t t) {
    uint8_t src = getInputSourceFromType(t);
    return (!vsync || !rgbHvAllowed) && (anyRgb || src == InputSourceRGBs || (vgaByHand && src == InputSourceVGA));
}

const char* inputLongName(uint8_t t);    // "RGB SCART" .. "Composite"; "-" for anything else
const char* inputShortName(uint8_t t);   // "RGB", "SoG", "VGA", "YPbPr", "S-Vid", "CVBS" (OLED home box); "-"
