// ADV7280 parameter defaults owned by the ESP (link v3: the ESP is the source of truth)
#pragma once

// ACE (ADV7280 User Sub Map 2)
#define ADV_ACE_LUMA_GAIN_DEFAULT       13
#define ADV_ACE_CHROMA_GAIN_DEFAULT     8
#define ADV_ACE_CHROMA_MAX_DEFAULT      8
#define ADV_ACE_GAMMA_GAIN_DEFAULT      8
#define ADV_ACE_RESPONSE_SPEED_DEFAULT  15

// Shaping filters (Main Register Map)
#define ADV_FILTER_Y_SHAPING_DEFAULT     1   // Auto Narrow
#define ADV_FILTER_C_SHAPING_DEFAULT     0   // Auto 1.5MHz
#define ADV_FILTER_WY_SHAPING_DEFAULT    19  // SVHS 18 (CCIR 601)
#define ADV_FILTER_WY_OVERRIDE_DEFAULT   1   // Manual
#define ADV_FILTER_COMB_NTSC_DEFAULT     0   // Narrow
#define ADV_FILTER_COMB_PAL_DEFAULT      1   // Medium

// Comb control (NTSC 0x38 = 0x80, PAL 0x39 = 0xC0)
#define ADV_COMB_LUMA_MODE_NTSC_DEFAULT     0
#define ADV_COMB_CHROMA_MODE_NTSC_DEFAULT   0
#define ADV_COMB_CHROMA_TAPS_NTSC_DEFAULT   2
#define ADV_COMB_LUMA_MODE_PAL_DEFAULT      0
#define ADV_COMB_CHROMA_MODE_PAL_DEFAULT    0
#define ADV_COMB_CHROMA_TAPS_PAL_DEFAULT    3
