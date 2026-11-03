#pragma once
#include <stdint.h>
#include <stddef.h>

enum class SettingType : uint8_t { Bool, U8, U16, Enum };
enum class SettingScope : uint8_t { Device, Profile };

// Apply groups: runtime side effects triggered after a setting changes
enum : uint16_t {
    APPLY_NONE        = 0,
    APPLY_INPUT       = 1 << 0,   // re-select the active input
    APPLY_SYNC        = 1 << 1,   // LM1881 sync stripper switch
    APPLY_ADV_FORMAT  = 1 << 2,   // ADV7280 video standard
    APPLY_ADV_I2P     = 1 << 3,   // I2P deinterlacer + smooth
    APPLY_ADV_ACE     = 1 << 9,   // ACE on/off + ACE params
    APPLY_ADV_BCSH    = 1 << 10,  // brightness, contrast, saturation, hue
    APPLY_ADV_FILTERS = 1 << 4,   // shaping + comb filters
    APPLY_COLOR       = 1 << 5,   // TV5725 RGB offsets
    APPLY_PRESET      = 1 << 6,   // reload the scaler preset (deferred to loop)
    APPLY_OSD         = 1 << 7,   // OSD theme
    APPLY_AUDIO       = 1 << 8,   // PT2257 volume/mute
    APPLY_PICTURE     = 1 << 11,  // TV5725 peaking gains (picture.sharpness), scanline strength, no preset reload
};

// Hardware a setting, option or action needs; missing hardware hides it (standard board support)
enum : uint8_t {
    CAP_NONE     = 0,
    CAP_ADV      = 1 << 0,   // ADV Controller (decoder, analog switches, LM1881 switching)
    CAP_OSD      = 1 << 1,   // STV9426 TV OSD
    CAP_AUDIO    = 1 << 2,   // PT2257 volume
    CAP_IR       = 1 << 3,   // IR remote
    CAP_OLED     = 1 << 4,   // SSD1306 OLED at 0x3C
    CAP_ENCODER  = 1 << 5,   // rotary encoder (assumed with the OLED)
    CAP_LED      = 1 << 6,   // GPIO2 status LED (standard board; GPIO2 is IR on the Pro)
    CAP_ORIGINAL = 1 << 7,   // standard gbs-control board (standard-board-only features)
};
static const uint8_t CAPS_PRO = CAP_ADV | CAP_OSD | CAP_AUDIO | CAP_IR;
// Before detection runs (and in native tests): behave like a Pro with OLED
static const uint8_t CAPS_DEFAULT = CAPS_PRO | CAP_OLED | CAP_ENCODER;

enum : uint8_t {
    SETTING_FLAG_NONE        = 0,
    SETTING_FLAG_INVERT      = 1 << 0, // Bool stored inverted in uopt
    SETTING_FLAG_SECRET      = 1 << 1, // write-only over HTTP, never in backups
    SETTING_FLAG_DEVICE_ONLY = 1 << 2, // read-only over HTTP, never in backups
    SETTING_FLAG_BACKUP      = 1 << 3, // with DEVICE_ONLY: still read-only over HTTP, but it travels with a backup
};

// Kept out of a backup, and kept as the unit has it across a restore: secrets and the unit's own state. The active
// profile and its hash describe the working settings and the profiles a backup carries, so they go with them
inline bool settingKeptOutOfBackups(uint8_t flags) {
    return (flags & SETTING_FLAG_SECRET) || ((flags & SETTING_FLAG_DEVICE_ONLY) && !(flags & SETTING_FLAG_BACKUP));
}

struct SettingDef {
    const char* key;      // PROGMEM on device
    uint16_t offset;      // offsetof(userOptions, field)
    SettingType type;
    SettingScope scope;
    uint16_t def;
    uint16_t min;
    uint16_t max;
    uint16_t apply;
    const char* options;  // PROGMEM "Name|Name=value|..." or nullptr
    uint8_t flags;
    uint8_t caps;         // CAP_* bits this setting needs (all required)
};

struct StrSettingDef {
    const char* key;      // PROGMEM on device
    uint16_t offset;      // offsetof(userOptions, field), a char[] field
    uint8_t maxLen;       // sizeof(field) - 1
    SettingScope scope;
    uint8_t flags;
};
