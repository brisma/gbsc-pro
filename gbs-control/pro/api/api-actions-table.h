// Named one-shot operations. ACTION(id, name, caps, flags, kind, arg)
// ACT_SERIAL / ACT_USER: arg is a legacy command char queued from loop(); ACT_FUNC: arg is an AF_* id.
#pragma once

#define ACTIONS_TABLE(ACTION) \
    ACTION(sys_restart,        "system.restart",           CAP_NONE, ACTION_CONFIRM, ACT_USER,   'a') \
    ACTION(sys_factory_reset,  "system.factory_reset",     CAP_NONE, ACTION_CONFIRM, ACT_USER,   '1') \
    ACTION(sys_check_update,   "system.check_update",      CAP_NONE, 0, ACT_FUNC, AF_CHECK_UPDATE) \
    ACTION(sys_install_update, "system.install_update",    CAP_NONE, ACTION_CONFIRM | ACTION_PARAM_OPT_TEXT, ACT_FUNC, AF_INSTALL_UPDATE) \
    ACTION(sys_check_channel,  "system.check_channel",     CAP_NONE, ACTION_PARAM_TEXT, ACT_FUNC, AF_CHECK_CHANNEL) \
    ACTION(sys_update_adv_controller,    "system.update_adv_controller",       CAP_ADV,  ACTION_CONFIRM, ACT_FUNC, AF_ADV_CONTROLLER_UPDATE) \
    ACTION(in_gain_up,         "input.gain_up",            CAP_NONE, ACTION_REGS, ACT_USER,   'n') \
    ACTION(in_gain_down,       "input.gain_down",          CAP_NONE, ACTION_REGS, ACT_USER,   'o') \
    ACTION(in_sog_up,          "input.sog_up",             CAP_NONE, 0, ACT_FUNC,   AF_SOG_UP) \
    ACTION(in_sog_down,        "input.sog_down",           CAP_NONE, 0, ACT_FUNC,   AF_SOG_DOWN) \
    ACTION(scr_move_left,      "screen.move_left",         CAP_NONE, ACTION_REGS, ACT_SERIAL, '7') \
    ACTION(scr_move_right,     "screen.move_right",        CAP_NONE, ACTION_REGS, ACT_SERIAL, '6') \
    ACTION(scr_move_up,        "screen.move_up",           CAP_NONE, ACTION_REGS, ACT_SERIAL, '*') \
    ACTION(scr_move_down,      "screen.move_down",         CAP_NONE, ACTION_REGS, ACT_SERIAL, '/') \
    ACTION(scr_scale_left,     "screen.scale_left",        CAP_NONE, ACTION_REGS, ACT_SERIAL, 'h') \
    ACTION(scr_scale_right,    "screen.scale_right",       CAP_NONE, ACTION_REGS, ACT_SERIAL, 'z') \
    ACTION(scr_scale_up,       "screen.scale_up",          CAP_NONE, ACTION_REGS, ACT_SERIAL, '4') \
    ACTION(scr_scale_down,     "screen.scale_down",        CAP_NONE, ACTION_REGS, ACT_SERIAL, '5') \
    ACTION(scr_border_left,    "screen.border_left",       CAP_NONE, ACTION_REGS, ACT_USER,   'B') \
    ACTION(scr_border_right,   "screen.border_right",      CAP_NONE, ACTION_REGS, ACT_USER,   'A') \
    ACTION(scr_border_up,      "screen.border_up",         CAP_NONE, ACTION_REGS, ACT_USER,   'C') \
    ACTION(scr_border_down,    "screen.border_down",       CAP_NONE, ACTION_REGS, ACT_USER,   'D') \
    ACTION(scr_bmove_left,     "screen.border_move_left",  CAP_NONE, ACTION_REGS, ACT_FUNC,   AF_BORDER_MOVE_LEFT) \
    ACTION(scr_bmove_right,    "screen.border_move_right", CAP_NONE, ACTION_REGS, ACT_FUNC,   AF_BORDER_MOVE_RIGHT) \
    ACTION(scr_bmove_up,       "screen.border_move_up",    CAP_NONE, ACTION_REGS, ACT_FUNC,   AF_BORDER_MOVE_UP) \
    ACTION(scr_bmove_down,     "screen.border_move_down",  CAP_NONE, ACTION_REGS, ACT_FUNC,   AF_BORDER_MOVE_DOWN) \
    ACTION(pic_bright_up,      "picture.brightness_up",    CAP_NONE, 0, ACT_USER,   'Z') \
    ACTION(pic_bright_down,    "picture.brightness_down",  CAP_NONE, 0, ACT_USER,   'T') \
    ACTION(pic_contrast_up,    "picture.contrast_up",      CAP_NONE, 0, ACT_USER,   'N') \
    ACTION(pic_contrast_down,  "picture.contrast_down",    CAP_NONE, 0, ACT_USER,   'M') \
    ACTION(pic_color_up,       "picture.color_up",         CAP_NONE, 0, ACT_USER,   'V') \
    ACTION(pic_color_down,     "picture.color_down",       CAP_NONE, 0, ACT_USER,   'R') \
    ACTION(pic_color_info,     "picture.color_info",       CAP_NONE, 0, ACT_USER,   'O') \
    ACTION(pic_reset_colors,   "picture.reset_colors",     CAP_NONE, 0, ACT_USER,   'U') \
    ACTION(pic_reset,          "picture.reset",            CAP_NONE, ACTION_CONFIRM, ACT_FUNC, AF_PICTURE_RESET) \
    ACTION(scr_reset,          "screen.reset",             CAP_NONE, ACTION_CONFIRM | ACTION_REGS, ACT_FUNC, AF_SCREEN_RESET) \
    ACTION(in_termination,     "input.termination",        CAP_ADV,  ACTION_PARAM_BYTES, ACT_FUNC, AF_TERMINATION) \
    ACTION(adv_ace_defaults,   "adv.ace_defaults",         CAP_ADV,  0, ACT_FUNC,   AF_ACE_DEFAULTS) \
    ACTION(adv_filt_defaults,  "adv.filters_defaults",     CAP_ADV,  0, ACT_FUNC,   AF_FILTERS_DEFAULTS) \
    ACTION(adv_decoder_reset,  "adv.decoder_reset",        CAP_ADV,  ACTION_CONFIRM, ACT_FUNC, AF_DECODER_RESET) \
    ACTION(dev_adv_read,       "dev.adv_read",             CAP_ADV,  ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_ADV_READ) \
    ACTION(dev_adv_i2c,        "dev.adv_i2c",              CAP_ADV,  ACTION_DEV | ACTION_PARAM_HEX, ACT_FUNC, AF_ADV_I2C) \
    ACTION(dev_mem_left,       "dev.mem_left",             CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, '-') \
    ACTION(dev_mem_right,      "dev.mem_right",            CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, '+') \
    ACTION(dev_osd_test,       "dev.osd_test",             CAP_OSD,  ACTION_DEV, ACT_FUNC,   AF_OSD_TEST) \
    ACTION(dev_osd_rom_dump,   "dev.osd_rom_dump",         CAP_OSD,  ACTION_DEV | ACTION_PARAM_OPT_BYTES, ACT_FUNC, AF_OSD_ROM_DUMP) \
    ACTION(dev_osd_always_on,  "dev.osd_always_on",        CAP_OSD,  ACTION_DEV, ACT_FUNC,   AF_OSD_ALWAYS_ON) \
    ACTION(dev_osd_timing,     "dev.osd_timing",           CAP_OSD,  ACTION_DEV | ACTION_PARAM_HEX, ACT_FUNC, AF_OSD_TIMING) \
    ACTION(menu_key,           "menu.key",                 CAP_NONE, ACTION_PARAM_BYTES, ACT_FUNC, AF_MENU_KEY) \
    ACTION(dev_menu_key,       "dev.menu_key",             CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_MENU_KEY) \
    ACTION(dev_screen_snap,    "dev.screen_snapshot",      CAP_NONE, ACTION_DEV, ACT_FUNC,   AF_SCREEN_SNAPSHOT) \
    ACTION(dev_hs_left,        "dev.hs_left",              CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, '1') \
    ACTION(dev_hs_right,       "dev.hs_right",             CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, '0') \
    ACTION(dev_print_info,     "dev.print_info",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'i') \
    ACTION(dev_tv5725_dump,    "dev.tv5725_dump",          CAP_NONE, ACTION_DEV, ACT_SERIAL, 'd') \
    ACTION(dev_tv5725_read,    "dev.tv5725_read",          CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_TV5725_READ) \
    ACTION(dev_si5351,         "dev.si5351",               CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_SI5351) \
    ACTION(dev_loop_pause,     "dev.loop_pause",           CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_LOOP_PAUSE) \
    ACTION(dev_clockgen,       "dev.clockgen",             CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_CLOCKGEN) \
    ACTION(dev_content_probe,  "dev.content_probe",        CAP_NONE, ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_CONTENT_PROBE) \
    ACTION(dev_listen,         "dev.listen",               CAP_ADV,  ACTION_DEV | ACTION_PARAM_BYTES, ACT_FUNC, AF_LISTEN) \
    ACTION(dev_tv5725_reg,     "dev.tv5725_reg",           CAP_NONE, ACTION_DEV | ACTION_PARAM_HEX | ACTION_REGS, ACT_FUNC, AF_TV5725_REG) \
    ACTION(dev_timings,        "dev.video_timings",        CAP_NONE, ACTION_DEV, ACT_SERIAL, ',') \
    ACTION(dev_adc_filter,     "dev.adc_filter",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'F') \
    ACTION(dev_debug_view,     "dev.debug_view",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'D') \
    ACTION(dev_htotal_up,      "dev.htotal_up",            CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, 'a') \
    ACTION(dev_htotal_down,    "dev.htotal_down",          CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, 'A') \
    ACTION(dev_htotal_resync,  "dev.htotal_resync",        CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, '.') \
    ACTION(dev_pll_up,         "dev.pll_div_up",           CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, 'n') \
    ACTION(dev_pll_down,       "dev.pll_div_down",         CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, 'M') \
    ACTION(dev_invert_sync,    "dev.invert_sync",          CAP_NONE, ACTION_DEV, ACT_SERIAL, '8') \
    ACTION(dev_syncwatcher,    "dev.syncwatcher",          CAP_NONE, ACTION_DEV, ACT_SERIAL, 'm') \
    ACTION(dev_sync_reset,     "dev.sync_reset",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'l') \
    ACTION(dev_osr_cycle,      "dev.osr_cycle",            CAP_NONE, ACTION_DEV, ACT_SERIAL, 'o') \
    ACTION(dev_snap_rate,      "dev.snap_frame_rate",      CAP_NONE, ACTION_DEV | ACTION_REGS, ACT_SERIAL, 'S') \
    ACTION(dev_reset_chip,     "dev.reset_chip",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'q') \
    ACTION(dev_enable_ota,     "dev.enable_ota",           CAP_NONE, ACTION_DEV, ACT_SERIAL, 'c') \
    ACTION(dev_ir_codes,       "dev.ir_codes",             CAP_IR,   ACTION_DEV, ACT_USER,   'I') \
    ACTION(dev_list_files,     "dev.list_files",           CAP_NONE, ACTION_DEV, ACT_USER,   'e') \
    ACTION(dev_freeze,         "dev.freeze_capture",       CAP_NONE, ACTION_DEV, ACT_USER,   'F') \
    ACTION(dev_sdram_cycle,    "dev.sdram_cycle",          CAP_NONE, ACTION_DEV, ACT_USER,   'l') \
    ACTION(dev_if_offset,      "dev.if_auto_offset",       CAP_NONE, ACTION_DEV, ACT_USER,   'E') \
    ACTION(dev_crash,          "dev.crash",                CAP_NONE, ACTION_DEV | ACTION_CONFIRM, ACT_FUNC, AF_CRASH)
