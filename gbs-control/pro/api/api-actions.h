// Named actions: lookup, availability and listing (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../settings/settings-types.h"
#include "../settings/settings-store.h"
#include "api-actions-table.h"

// Generated ids: AID_<table id> == the action's index
#define ACTION_ENUM(id, ...) AID_##id,
enum ActionId : uint8_t { ACTIONS_TABLE(ACTION_ENUM) AID_COUNT };
#undef ACTION_ENUM

enum : uint8_t { ACT_SERIAL = 0, ACT_USER = 1, ACT_FUNC = 2 };
enum : uint8_t { ACTION_DEV = 1 << 0, ACTION_CONFIRM = 1 << 1, ACTION_PARAM_HEX = 1 << 2, ACTION_PARAM_BYTES = 1 << 3, ACTION_PARAM_OPT_BYTES = 1 << 4,
                 ACTION_REGS = 1 << 5,     // changes captured scaler registers: the active profile becomes Unsaved
                 ACTION_PARAM_TEXT = 1 << 6, ACTION_PARAM_OPT_TEXT = 1 << 7 };   // data = a short [a-z0-9_] word
enum : uint8_t { AF_ACE_DEFAULTS = 1, AF_FILTERS_DEFAULTS, AF_ADV_I2C, AF_CRASH,
                 AF_CHECK_UPDATE, AF_INSTALL_UPDATE,
                 AF_OSD_TEST, AF_OSD_ROM_DUMP, AF_OSD_ALWAYS_ON, AF_OSD_TIMING, AF_ADV_CONTROLLER_UPDATE,
                 AF_MENU_KEY, AF_SCREEN_SNAPSHOT,
       AF_BORDER_MOVE_LEFT, AF_BORDER_MOVE_RIGHT, AF_BORDER_MOVE_UP, AF_BORDER_MOVE_DOWN,
                 AF_SOG_UP, AF_SOG_DOWN, AF_TV5725_REG, AF_CHECK_CHANNEL,
                 AF_PICTURE_RESET, AF_SCREEN_RESET, AF_DECODER_RESET, AF_TERMINATION, AF_TV5725_READ, AF_ADV_READ, AF_CLOCKGEN, AF_LOOP_PAUSE, AF_SI5351, AF_CONTENT_PROBE, AF_LISTEN };

struct ActionDef { const char* name; uint8_t caps; uint8_t flags; uint8_t kind; uint8_t arg; };

uint8_t actionsCount();
void actionsDefAt(uint8_t i, ActionDef& out);   // out.name points to a shared RAM copy, valid until the next call
int actionsFind(const char* name);
bool actionsAvailable(const ActionDef& d, bool devmode);
void actionsListJson(bool devmode, SettingsSink sink, void* ctx);
// Byte parameters of ACTION_PARAM_BYTES actions after parsing: count and value range per action
enum : int { ACT_PARAM_OK = 0, ACT_ERR_PARAM = 1 };
int actionsValidateParams(const ActionDef& d, const uint8_t* data, uint8_t len);
// "42,0E,00,..." → bytes; true for complete triplets of 1-2 digit hex bytes that fit in maxOut
// "06,1,4" → bytes; true for 1..maxOut hex bytes of 1-2 digits (dev.menu_key: a MenuKey sequence)
bool actionsParseHexBytes(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len);
bool actionsParseHexTriplets(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len);
// A text parameter ("beta", "manual"): 1..maxOut-1 characters of [a-z0-9_], NUL-terminated in out; len = its length
bool actionsParseText(const char* text, uint8_t* out, uint8_t maxOut, uint8_t& len);
