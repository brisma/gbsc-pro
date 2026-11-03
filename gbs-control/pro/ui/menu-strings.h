// Every device string of the menu (English, STV9426 ROM characters only), stored in flash
#pragma once
#include <stdint.h>
#include <stddef.h>

#define MENU_STRINGS(S) \
    S(S_NONE, "") \
    S(S_MENU, "GBSC PRO") \
    S(S_INPUT, "Input") \
    S(S_OUTPUT, "Output") \
    S(S_PICTURE, "Picture") \
    S(S_SCREEN, "Screen") \
    S(S_PROFILES, "Profiles") \
    S(S_AUDIO, "Sound") \
    S(S_SYSTEM, "System") \
    S(S_DEVELOPER, "Developer") \
    S(S_SOURCE, "Source") \
    S(S_VIDEO_FORMAT, "Video standard") \
    S(S_SYNC, "Sync") \
    S(S_SYNC_STRIPPER, "Sync cleanup") \
    S(S_ADAPTIVE_SYNC, "Adaptive Sync") \
    S(S_LOST_WAIT, "Wait before search") \
    S(S_TERMINATION, "75 ohm sync") \
    S(S_ADC, "Sampling") \
    S(S_AUTO_SEARCH, "Auto search") \
    S(S_IN_VGA, "VGA") \
    S(S_IN_RGBS, "RGB SCART") \
    S(S_IN_RGSB, "RGB SoG") \
    S(S_IN_YUV, "Component") \
    S(S_IN_SV, "S-Video") \
    S(S_IN_AV, "Composite") \
    S(S_AUTO_GAIN, "Auto gain") \
    S(S_GAIN, "Gain") \
    S(S_CALIBRATION, "Boot calibration") \
    S(S_DECODER, "Decoder") \
    S(S_BRIGHTNESS, "Brightness") \
    S(S_CONTRAST, "Contrast") \
    S(S_SATURATION, "Saturation") \
    S(S_HUE, "Hue") \
    S(S_FILTERS, "Filters") \
    S(S_Y_FILTER, "Luma filter") \
    S(S_Y_FILTER_MODE, "Luma filter mode") \
    S(S_C_FILTER, "Chroma filter") \
    S(S_BANDWIDTH, "Comb split") \
    S(S_LUMA_MODE, "Luma comb") \
    S(S_CHROMA_MODE, "Chroma comb") \
    S(S_CHROMA_TAPS, "Comb taps") \
    S(S_DNR, "Noise reduction") \
    S(S_CTI, "Color transients") \
    S(S_ACE, "Adaptive contrast") \
    S(S_LUMA_GAIN, "Contrast boost") \
    S(S_CHROMA_GAIN, "Color boost") \
    S(S_CHROMA_MAX, "Color boost max") \
    S(S_GAMMA_GAIN, "Shadow boost") \
    S(S_RESPONSE_SPEED, "Speed") \
    S(S_RESET_DECODER, "Reset decoder") \
    S(S_RESOLUTION, "Resolution") \
    S(S_FORCE60, "Always 60Hz") \
    S(S_FRAME_LOCK, "Smooth scrolling") \
    S(S_LOCK_METHOD, "Lock method") \
    S(S_EXT_CLOCK, "External clock") \
    S(S_COMPONENT, "Analog out") \
    S(S_HDMI_RANGE, "HDMI range") \
    S(S_COLOR, "Color") \
    S(S_RED, "Red") \
    S(S_GREEN, "Green") \
    S(S_BLUE, "Blue") \
    S(S_RESET_COLORS, "Reset colors") \
    S(S_DETAIL, "Detail") \
    S(S_SHARPNESS, "Sharpness") \
    S(S_STEP_RESPONSE, "Color sharpness") \
    S(S_SCANLINES, "Scanlines") \
    S(S_SCAN_STRENGTH, "Scanline strength") \
    S(S_LINE_FILTER, "Line filter") \
    S(S_RGBHV_SCALING, "VGA upscaling") \
    S(S_DEINTERLACE, "Deinterlace") \
    S(S_POSITION, "Position") \
    S(S_SIZE, "Size") \
    S(S_BORDER_POS, "Mask position") \
    S(S_BORDERS, "Edge mask") \
    S(S_FULL_HEIGHT, "PAL full height") \
    S(S_RESET_GEOMETRY, "Reset screen") \
    S(S_LOAD, "Load") \
    S(S_MANAGE, "Manage") \
    S(S_SWITCH_INPUT, "Load sets input") \
    S(S_AUTOLOAD, "Auto-load") \
    S(S_VOLUME, "Volume") \
    S(S_MUTE, "Mute") \
    S(S_SIGNAL_INFO, "Signal info") \
    S(S_OSD_THEME, "Theme") \
    S(S_OSD, "OSD settings") \
    S(S_OSD_POPUP_POS, "Pop-ups") \
    S(S_OSD_TIMEOUT, "Auto close") \
    S(S_WIFI, "WiFi") \
    S(S_MODE, "Mode") \
    S(S_NETWORK, "Network") \
    S(S_ADDRESS, "Address") \
    S(S_PASSWORD, "Password") \
    S(S_HOSTNAME, "Hostname") \
    S(S_BACK, "Back") \
    S(S_ALL_SETTINGS, "All settings") \
    S(S_TV_OSD, "TV OSD") \
    S(S_TV_CHANGES, "Changes on TV") \
    S(S_RESET_AP, "Reset to hotspot") \
    S(S_UPDATE, "Update") \
    S(S_CHECK_NOW, "Check now") \
    S(S_INSTALL, "Install") \
    S(S_UPDATE_ADV_CONTROLLER, "ADV Controller") \
    S(S_ADV_UPDATE, "ADV update") \
    S(S_DAILY_CHECK, "Auto check") \
    S(S_CHANNEL, "Channel") \
    S(S_OFFER, "Offer") \
    S(S_OTHER_CHANNELS, "Other channels") \
    S(S_CH_STABLE, "Stable") \
    S(S_CH_BETA, "Beta") \
    S(S_LAST_RESULT, "Last result") \
    S(S_RESTART, "Restart") \
    S(S_FACTORY_RESET, "Factory reset") \
    S(S_DEV_MODE, "Developer menu") \
    S(S_WATCHER_MODE, "Watcher profile") \
    S(S_SYNC_WATCHER, "Sync watcher") \
    S(S_INVERT_SYNC, "Sync polarity") \
    S(S_HSYNC_POS, "HSync position") \
    S(S_RESET_SYNC, "Reset sync") \
    S(S_SOG_LEVEL, "SOG level") \
    S(S_IF_OFFSET, "IF auto offset") \
    S(S_CLOCK_ADC, "Clock / ADC") \
    S(S_HTOTAL, "HTotal") \
    S(S_RESYNC_HTOTAL, "Resync HTotal") \
    S(S_PLL_DIV, "Samples per line") \
    S(S_SDRAM, "SDRAM clock") \
    S(S_OVERSAMPLING, "Oversampling") \
    S(S_ADC_FILTER, "ADC filter") \
    S(S_SNAP, "Snap 60/50Hz") \
    S(S_MEM_SHIFT, "H capture shift") \
    S(S_DEBUG, "Debug") \
    S(S_DEBUG_VIEW, "Debug view") \
    S(S_FREEZE, "Freeze capture") \
    S(S_RESET_CHIP, "Reset chip") \
    S(S_OSD_TEST, "OSD test screen") \
    S(S_OSD_ALWAYS_ON, "OSD always on") \
    S(S_OSD_ROM_DUMP, "OSD ROM dump") \
    S(S_PRINT_INFO, "Print info") \
    S(S_VIDEO_TIMINGS, "Print timings") \
    S(S_LIST_FILES, "List files") \
    S(S_IR_LOG, "IR codes log") \
    S(S_WIFI_WATCH, "WiFi link watch") \
    S(S_ENABLE_OTA, "Arduino upload") \
    S(S_BOARD, "Board type") \
    S(S_ENCODER_DIR, "Knob direction") \
    S(S_ENCODER_STEPS, "Knob steps") \
    S(S_OTA_PASSWORD, "OTA password") \
    S(S_WIFI_OPTIMIZE, "Optimize WiFi") \
    S(S_EXTRA_HOSTS, "Extra hosts") \
    S(S_MENU_MODE, "Menu mode") \
    S(S_MENU_POSITION, "Menu position") \
    S(S_2X, "2X") \
    S(S_2X_SMOOTH, "2X smooth") \
    S(S_LEVELS, "Levels") \
    S(S_COMB, "Comb") \
    S(S_ENHANCE, "Enhance") \
    S(S_ENABLED, "Enabled") \
    S(S_RESET_ADAPTIVE, "Reset adaptive") \
    S(S_RESET_FILTERS, "Reset comb/filters") \
    S(S_RESET_PICTURE, "Reset picture") \
    S(S_FW_GBS, "GBS-Control") \
    S(S_SERIAL_LOG, "Serial log") \
    S(S_R_NEEDS_BOB, "Needs Deinterlace: Bob") \
    S(S_R_2X_FIRST, "Turn 2X on first") \
    S(S_R_2X_ON, "Not needed with 2X on") \
    S(S_R_FORCE60, "Off with Always 60Hz") \
    S(S_R_FTL_OFF, "Turn Smooth scrolling on") \
    S(S_NEW_PROFILE, "New profile") \
    S(S_SAVE_CHANGES, "Save changes") \
    S(S_DISCARD_CHANGES, "Discard changes") \
    S(S_DEFAULTS, "Defaults") \
    S(S_REPLACE, "Replace with current") \
    S(S_STARTUP, "Startup") \
    S(S_V_ACTIVE, "Active") \
    S(S_V_UNSAVED, "Unsaved") \
    S(S_V_MATCH, "Match") \
    S(S_T_LOSE, "Discard unsaved changes?") \
    S(S_T_NO_CHANGES, "No unsaved changes") \
    S(S_T_SAVE_TO, "Save to") \
    S(S_T_REPLACE_Q, "Overwrite this profile?") \
    S(S_T_NEW_SIGNAL, "Overwrite for this signal?") \
    S(S_SIGNAL_NAME, "Signal name") \
    S(S_CONSOLES, "Consoles") \
    S(S_T_SAME_SIGNAL, "Same signal as") \
    S(S_KEEP_BOTH, "Keep both") \
    S(S_REPLACE_IT, "Replace it") \
    S(S_CANCEL, "Cancel") \
    S(S_STATUS_POPUP, "Status pop-up") \
    S(S_T_DISCARDED, "Discarded:") \
    S(S_T_OTHER, "Left/Right") \
    S(S_T_OF, "of") \
    S(S_T_PROFILES, "profiles") \
    S(S_QUICK, "Quick menu") \
    S(S_PROFILE, "Profile") \
    S(S_T_CHOOSE_INPUT, "Choose the input") \
    S(S_OFF, "Off") \
    S(S_CUSTOM, "Custom") \
    S(S_NORMAL, "Normal") \
    S(S_SHARP, "Sharp") \
    S(S_SMOOTH, "Smooth") \
    S(S_LIGHT, "Light") \
    S(S_MEDIUM, "Medium") \
    S(S_STRONG, "Strong") \
    S(S_FILTER, "Filter") \
    S(S_RES_DOWNSCALE, "Downscale") \
    S(S_RES_480, "480p/576p") \
    S(S_RES_720, "720p") \
    S(S_RES_960_FIXED, "960p fixed") \
    S(S_RES_960_1024, "960p/1024p") \
    S(S_RES_1024_FIXED, "1024p fixed") \
    S(S_RES_1080, "1080p") \
    S(S_RES_1200, "1200p") \
    S(S_RES_BYPASS, "HD bypass") \
    S(S_ON, "On") \
    S(S_DELETE, "Delete") \
    S(S_RENAME, "Rename") \
    S(S_EDIT_NAME, "Edit name...") \
    S(S_FORGET, "Forget signal") \
    S(S_NO_PROFILES, "No profiles") \
    S(S_R_WY_MANUAL, "Set Luma filter mode: Manual") \
    S(S_R_RGBS, "RGB SCART input only") \
    S(S_R_ACE, "Turn Enabled on first") \
    S(S_R_SCANLINES, "Not with this signal") \
    S(S_R_NO_FP, "No signal stored") \
    S(S_T_BUSY, "Busy - try again") \
    S(S_T_CHECKING, "Checking...") \
    S(S_T_RESTART, "Applies after restart") \
    S(S_T_ADV_STAY, "Advanced changes stay active") \
    S(S_T_WAIT, "Please wait") \
    S(S_T_DONE, "Done") \
    S(S_T_FAILED, "Failed:") \
    S(S_T_KEEP, "OK keep  EXIT undo") \
    S(S_T_ADJUST, "Arrows adjust  OK done") \
    S(S_T_ADJUST_UNDO, "Arrows  OK keep  EXIT undo") \
    S(S_Q_UPDATE_ADV_CONTROLLER, "Update the ADV Controller?") \
    S(S_Q_RESTART, "Restart the GBSC PRO?") \
    S(S_Q_FACTORY_RESET, "Erase settings and profiles?") \
    S(S_Q_RESET_AP, "Forget WiFi and use hotspot?") \
    S(S_Q_RESET_PICTURE, "Reset the Picture page?") \
    S(S_Q_RESET_COLORS, "Reset Red/Green/Blue?") \
    S(S_Q_RESET_GEOMETRY, "Undo position and size?") \
    S(S_Q_RESET_DECODER, "Reset the Decoder settings?") \
    S(S_Q_RESET_ADAPTIVE, "Reset the adaptive contrast?") \
    S(S_Q_RESET_FILTERS, "Reset comb and filters?") \
    S(S_Q_RESET_CHIP, "Reset the scaler chip?") \
    S(S_Q_INSTALL, "Install the update now?") \
    S(S_Q_INSTALL_MANUAL, "Install this version?") \
    S(S_T_UPDATING, "Updating GBS-Control") \
    S(S_T_UPDATING_ADV_CONTROLLER, "Updating ADV Controller") \
    S(S_T_VERIFYING, "Verifying") \
    S(S_T_UPDATED, "Update successful") \
    S(S_T_UPDATE_FAILED, "Update failed") \
    S(S_T_MUTE_ON, "Mute on") \
    S(S_T_MUTE_OFF, "Mute off") \
    S(S_T_NO_SIGNAL, "No signal") \
    S(S_T_INVALID, "Invalid") \
    S(S_T_LOADED, "Loaded") \
    S(S_T_ADV_CONTROLLER_OLD, "Flash ADV Controller via USB") \
    S(S_T_TRY_COMPOSITE, "No picture: try Composite") \
    S(S_T_ADV_CONTROLLER_DONE, "ADV Controller updated") \
    S(S_T_ADV_CONTROLLER_FAIL, "ADV Controller update failed") \
    S(S_T_2X_UNSUPPORTED, "2X off: not for this source") \
    S(S_T_KB_HELP, "OK type  EXIT cancel") \
    S(S_K_NAME, "Name") \
    S(S_YES, "Yes") \
    S(S_NO, "No") \
    S(S_OK, "OK") \
    S(S_IP, "IP") \
    S(S_T_INFO_CLOSE, "INFO or EXIT to close") \
    S(S_T_HORIZONTAL, "horizontal") \
    S(S_T_VERTICAL, "vertical") \
    S(H_ACE_ENABLE, "Turns adaptive contrast on: more detail in dark areas without blowing out bright ones.") \
    S(H_MENU_MODE, "Easy shows the everyday items. Advanced shows every item and the fine tuning.") \
    S(H_2X, "Line doubler for S-Video/Composite: 480i/576i become 480p/576p. Interlaced games may flicker.") \
    S(H_2X_SMOOTH, "Extra smoothing in the 2X picture. Compare it on and off and keep what looks better.") \
    S(H_RESET_PICTURE, "Puts every setting on the Picture page back to its default.") \
    S(H_SERIAL_LOG, "Prints status/timings/files/IR codes to the serial log for debugging.") \
    S(H_FW_GBS, "Version running now. GBS-Control: main firmware. ADV Controller: the decoder board.") \
    S(H_CHANNEL, "Release channel of the installed firmware: Stable/Beta.") \
    S(H_OFFER, "Channel of the offered version. Switch: it moves you to that channel. Older: a downgrade.") \
    S(H_OTHER_CHANNELS, "Check Stable/Beta by hand to change channel or to go back to an older version.") \
    S(H_CH_CHECK, "Looks for the latest version on this channel. Install then appears below.") \
    S(H_INSTALL_MANUAL, "Installs the version the last channel check found. It may be older than yours.") \
    S(H_PIC_BRIGHTNESS, "Brightness of the final picture on every input. 50 is neutral.") \
    S(H_PIC_CONTRAST, "Contrast of the final picture on every input. 50 is neutral.") \
    S(H_PIC_SATURATION, "Color strength of the final picture on every input. 50 is neutral. 0 is black and white.") \
    S(H_ANALOG_OUT, "Signal on the analog port. Off: RGB for PC monitors. On: YPbPr for the component input of a TV.") \
    S(H_FTL, "Locks the output frame rate to the console: no stutter or tearing. Off if the TV blanks.") \
    S(H_NEW_PROFILE, "Saves the current settings as a new profile. Pick a name from the list or type your own.") \
    S(H_SAVE_CHANGES, "Stores your changes in the profile in use so they come back when it loads.") \
    S(H_DISCARD_CHANGES, "Drops your changes: the profile in use loads again as it was saved.") \
    S(H_STARTUP, "At power-on: the last settings or Defaults at the last resolution. Auto-load may still apply.") \
    S(H_STATUS_POPUP, "Signal and profile box. Input only: on input or profile change. Always: also on mode changes.") \
    S(H_QUICK_PROFILE, "Left/Right on the remote load another profile of this signal. OK or a click opens the list.") \
    S(H_ALL_SETTINGS, "Leaves the Quick menu and opens the full menu with every page.") \
    S(H_TV_OSD, "Opens icons on the TV to change brightness/contrast/position/size.") \
    S(H_TV_CHANGES, "Shows an icon on the TV while you change brightness/contrast/position/size.") \
    S(S_T_TVOSD_HELP, "Turn=change Click=next") \
    S(H_INPUT, "Choose the input cable and tune how its signal is read.") \
    S(H_OUTPUT, "What the TV receives: resolution/60Hz/smooth scrolling/HDMI range.") \
    S(H_PICTURE, "How the picture looks: filter/scanlines/levels/deinterlace/colors.") \
    S(H_SCREEN, "Fit the picture to the TV: position/size/edge mask.") \
    S(H_PROFILES, "Save the settings of each console or game and load them by hand or by signal.") \
    S(H_AUDIO, "Volume and mute of the audio output.") \
    S(H_SYSTEM, "Signal info/sound/WiFi/updates and device options.") \
    S(H_DEVELOPER, "Low-level scaler tools for testing and debugging. Not needed for normal use.") \
    S(H_SOURCE, "Input cable. Auto finds it. A cable picked by hand returns to Auto after Wait before search.") \
    S(H_AUTO_SEARCH, "Choose the inputs Auto checks. Turning off unused ones finds the signal faster.") \
    S(H_LOST_WAIT, "How long a signal may vanish before Auto searches or a manual pick returns to Auto.") \
    S(H_SCAN, "On: Auto checks this input. Off: Auto skips it. Turn off the inputs you never use.") \
    S(H_VIDEO_FORMAT, "Color system of S-Video and Composite. Auto finds it. Grey blacks on NTSC: Auto 7.5 IRE.") \
    S(H_SYNC_STRIPPER, "Cleans the RGB SCART and SoG sync with a separator chip. Off only if the picture rolls or drops.") \
    S(H_ADAPTIVE_SYNC, "Steadier picture on bright screens and video mode changes. Off is the original sync handling.") \
    S(H_TERMINATION, "75 ohm load on the SCART sync for luma-sync cables. Resets at restart and input change.") \
    S(H_ADC, "How the scaler reads the input: gain/SOG level/black calibration.") \
    S(H_GAIN, "Input signal level. Too dark: raise it. Whites blown out or colors too strong: lower it.") \
    S(H_CALIBRATION, "Calibrates the input black level at every power-on. Keep it on.") \
    S(H_DECODER, "Fine tuning of the S-Video and Composite decoder: levels/comb/filters/enhance.") \
    S(H_BANDWIDTH, "Composite comb split filter. Wide: less dot crawl. Narrow: cleaner diagonal lines.") \
    S(H_FILTER_DEFAULTS, "Puts comb/filters/noise reduction/color transients back to their defaults.") \
    S(H_ACE, "Adaptive contrast of the decoder: more detail in dark scenes. S-Video and Composite.") \
    S(H_ACE_DEFAULTS, "Puts the adaptive contrast boosts and speed back to their defaults.") \
    S(H_RESET_DECODER, "Puts every Decoder setting and 2X back to their defaults.") \
    S(H_RESOLUTION, "Sent to the TV. 1080p suits most TVs. Even scanlines: 960p/1024p on any source. 1200p on 60 Hz.") \
    S(H_FORCE60, "Sends 50Hz PAL sources at 60Hz for TVs that dislike 50Hz. Smooth scrolling then stays off.") \
    S(H_EXT_CLOCK, "Uses the board's external clock chip when one is fitted. Applies after a restart.") \
    S(H_HDMI_RANGE, "Grey blacks: try Full. Crushed dark areas: try Limited. Auto: Limited at 720p and 1080p.") \
    S(H_SHARPNESS, "Edge sharpening of the scaler. 0 turns it off. The Filter row sets it too.") \
    S(H_DEINTERLACE, "How 480i/576i are shown. Adaptive: sharper still images. Bob: no combing and allows scanlines.") \
    S(H_RESET_GEOMETRY, "Undoes Position/Size/Edge mask for this signal mode. The profile in use forgets them too.") \
    S(H_MANAGE, "Rename or delete profiles. Replace one with the current settings or unlink its signal.") \
    S(H_SWITCH_INPUT, "Loading a profile by hand also switches to the input it was saved on.") \
    S(H_AUTOLOAD, "When a signal comes in the profile saved for it loads by itself.") \
    S(H_WIFI, "WiFi state and the address of the web page. Reset goes back to the hotspot.") \
    S(H_OSD, "Look and place of the TV menu and pop-ups and when menus close.") \
    S(H_AUTO_GAIN, "Sets the input level by itself. Changing Gain by hand turns it off.") \
    S(H_Y_FILTER, "Luma filter of the decoder. Higher SVHS numbers keep more detail. Lower ones look softer.") \
    S(H_Y_FILTER_MODE, "Auto: the decoder picks the S-Video luma filter. Manual: the Luma filter row sets it.") \
    S(H_C_FILTER, "Color bandwidth of Composite. Wider keeps color detail. Narrower cuts color noise.") \
    S(H_LUMA_MODE, "Luma comb filter of Composite. Adaptive is the default. Try the others against dot crawl.") \
    S(H_CHROMA_MODE, "Chroma comb filter of Composite. Adaptive is the default. Try the others against rainbows.") \
    S(H_CHROMA_TAPS, "How many lines the Composite color comb uses. More lines give cleaner color.") \
    S(H_DNR, "Digital noise reduction in the decoder. Removes grain but softens fine detail.") \
    S(H_CTI, "Color transient improvement: sharper color edges on S-Video and Composite.") \
    S(H_LUMA_GAIN, "How strongly adaptive contrast raises contrast. Higher is stronger.") \
    S(H_CHROMA_GAIN, "How much color adaptive contrast adds with the boost.") \
    S(H_CHROMA_MAX, "Upper limit of the color boost so colors never get too strong.") \
    S(H_GAMMA_GAIN, "Further contrast on top of Contrast boost. Higher is stronger.") \
    S(H_RESPONSE_SPEED, "How quickly adaptive contrast follows scene changes. Higher is faster.") \
    S(H_BRIGHTNESS, "Decoder brightness of S-Video and Composite. 50 is neutral. Applies before Picture brightness.") \
    S(H_CONTRAST, "Decoder contrast of S-Video and Composite. 50 is neutral. Applies before Picture contrast.") \
    S(H_SATURATION, "Decoder color of S-Video and Composite. 50 is neutral. Applies before Picture saturation.") \
    S(H_HUE, "Shifts the color tint of S-Video and Composite. 50 is neutral. Use it to correct skin tones.") \
    S(H_LOCK_METHOD, "How Smooth scrolling locks the frames. If the TV blanks out with one try the other.") \
    S(H_RED, "Adds or removes red in the whole picture. 128 is neutral.") \
    S(H_GREEN, "Adds or removes green in the whole picture. 128 is neutral.") \
    S(H_BLUE, "Adds or removes blue in the whole picture. 128 is neutral.") \
    S(H_RESET_COLORS, "Puts Red/Green/Blue back to neutral 128.") \
    S(H_STEP_RESPONSE, "Sharper color edges in the scaler. No effect at 1080p.") \
    S(H_SCANLINES, "Dark lines between the picture lines as on a CRT. Stronger settings darken the picture.") \
    S(H_FILTER, "How pixels look when scaled. Sharp: crisp edges. Smooth: soft edges that blend dithering.") \
    S(H_SCAN_STRENGTH, "How dark the scanlines are. Lower is darker. The Scanlines row sets it too.") \
    S(H_LINE_FILTER, "Smooths the picture vertically. Every Filter choice turns it on.") \
    S(H_RGBHV_SCALING, "Upscales VGA sources of 640x480 and lower. Off passes them through unscaled.") \
    S(H_POSITION, "Moves the picture on the TV. Open it and move it with the arrows or the knob.") \
    S(H_SIZE, "Stretches or shrinks the picture. Open it and use the arrows or the knob.") \
    S(H_BORDER_POS, "Moves the edge mask without changing its size when it cuts one side too much.") \
    S(H_BORDERS, "Blacks out junk at the picture edges. Open it and use the arrows or the knob.") \
    S(H_FULL_HEIGHT, "On 1080p output PAL 50Hz sources fill the whole screen height.") \
    S(H_LOAD, "Loads a saved profile or the Defaults. Match marks the profiles of this signal.") \
    S(H_VOLUME, "Volume of the audio output. The remote volume keys change it too.") \
    S(H_MUTE, "Silences the audio output. The volume stays as it was.") \
    S(H_MODE, "AP: the GBSC PRO runs its own hotspot. Client: it is on your WiFi network.") \
    S(H_NETWORK, "Name of the WiFi in use: your network or the GBSC PRO hotspot.") \
    S(H_ADDRESS, "Type this address in a browser on the same WiFi to open the web page.") \
    S(H_PASSWORD, "Password to join the GBSC PRO hotspot from a phone or computer.") \
    S(H_HOSTNAME, "Name of the device on the network. Open it in a browser instead of the address.") \
    S(H_CHECK_NOW, "Looks online now for a newer version on this channel or a more stable one.") \
    S(H_DAILY_CHECK, "Daily: looks for new firmware once a day and tells you when one is found.") \
    S(H_LAST_RESULT, "What the last update check found or why it failed.") \
    S(H_OSD_THEME, "Colors of the menu on the TV: GBSC PRO/Classic/Light/Retro.") \
    S(H_POSITION_OSD, "Where the menu appears on the TV screen: nine places to pick from.") \
    S(H_OSD_POPUP_POS, "Where the volume and status pop-ups appear on the TV screen.") \
    S(H_OSD_TIMEOUT, "Closes the TV and OLED menus after this long without a key press.") \
    S(H_SIGNAL_INFO, "Shows input/signal/rates/output mode and the profile in use right now.") \
    S(H_RESTART, "Restarts the GBSC PRO. Settings and profiles are kept.") \
    S(H_DEV_MODE, "Shows the Developer page with low-level tools in Advanced mode.") \
    S(H_INVERT_SYNC, "Flips the polarity of the output HSync and VSync.") \
    S(H_HSYNC_POS, "Moves the output HSync pulse left or right.") \
    S(H_RESET_SYNC, "Resets the sync processor to regain a lost lock.") \
    S(H_IF_OFFSET, "Toggles the scaler IF auto offset. Experimental: for tests only.") \
    S(H_HTOTAL, "Total length of each output line. Changes the output timing.") \
    S(H_RESYNC_HTOTAL, "Measures the source again and sets the best HTotal for it.") \
    S(H_PLL_DIV, "How many samples the ADC takes per input line. Should match the source total pixels per line.") \
    S(H_ADC_FILTER, "Turns the ADC low-pass filter on or off. On cuts high frequency noise.") \
    S(H_SNAP, "Sets the output frame rate to exactly 60 or 50Hz.") \
    S(H_MEM_SHIFT, "Shifts the captured picture left or right in the scaler memory.") \
    S(H_DEBUG_VIEW, "Boosts the picture to show noise and the blanking area. For sampling tests.") \
    S(H_FREEZE, "Stops the capture: the last frame stays on screen until you turn it off.") \
    S(H_RESET_CHIP, "Resets the scaler digital logic and its memory.") \
    S(H_OSD_TEST, "Shows the test screen of the OSD chip on the TV.") \
    S(H_PRINT_INFO, "Prints a status line to the serial log again and again while on.") \
    S(H_VIDEO_TIMINGS, "Prints the current video timings to the serial log once.") \
    S(H_LIST_FILES, "Prints the files in flash and their sizes to the serial log.") \
    S(H_IR_LOG, "Prints the code of each remote key to the serial log.") \
    S(H_WIFI_WATCH, "Pings the router every minute and logs when the WiFi link drops and for how long.") \
    S(H_ENABLE_OTA, "Allows firmware uploads from the Arduino IDE over WiFi until restart.") \
    S(H_SYNC_DEV, "Sync tools: sync watcher/output polarity/HSync position/resets.") \
    S(H_CLOCK_ADC, "Output line timing and input sampling tools.") \
    S(H_DEBUG, "Test views and resets for the scaler and the OSD chip.") \
    S(H_BOARD, "Auto finds the board. Pick Standard or Pro only if features look wrong. Restart to apply.") \
    S(H_ENCODER_DIR, "Reverses the knob if turning it moves the cursor the wrong way.") \
    S(H_ENCODER_STEPS, "How many menu steps one click of the knob moves. Change it if the knob skips or lags.") \
    S(H_OTA_PASSWORD, "Sets the password the Arduino IDE needs for WiFi uploads.") \
    S(H_WIFI_OPTIMIZE, "Picks at power-on the scaler memory clock that disturbs WiFi the least. Keep it on.") \
    S(H_EXTRA_HOSTS, "Extra names the web page accepts. Add the name your router or proxy uses for the device.") \
    S(H_RESET_AP, "Forgets your WiFi network and restarts into the GBSC PRO hotspot.") \
    S(H_UPDATE, "Check for and install new firmware from the internet. Shows the installed versions.") \
    S(H_INSTALL, "Downloads and installs the offered firmware. It includes the ADV Controller update too.") \
    S(H_UPDATE_ADV_CONTROLLER, "Flashes the ADV Controller with the version that came with this firmware.") \
    S(H_FACTORY_RESET, "Erases every setting and profile and restarts with the defaults. The WiFi network is kept.") \
    S(H_WATCHER_MODE, "This profile's sync watcher. Off: signal changes and loss are not followed. Inherit: on.") \
    S(H_SYNC_WATCHER, "Turns the sync watcher on or off now. It detects signal changes and loss.") \
    S(H_SOG_LEVEL, "Sync-on-green detection level for SoG and Component. Lower it if the picture drops out.") \
    S(H_SDRAM, "Cycles the scaler memory clock. Optimize WiFi picks one at power-on.") \
    S(H_OVERSAMPLING, "Cycles the input oversampling: 1x/2x/4x samples per pixel.") \
    S(H_OSD_ALWAYS_ON, "Keeps the OSD chip powered while the menu is closed. For A/B tests.") \
    S(H_OSD_ROM_DUMP, "Prints the character ROM of the OSD chip to the serial log.") \

#define MENU_STR_ENUM(id, text) id,
enum MenuStr : uint16_t { MENU_STRINGS(MENU_STR_ENUM) S_COUNT };
#undef MENU_STR_ENUM

void menuStrCopy(uint16_t id, char* out, size_t n);   // always terminated
uint8_t menuStrLen(uint16_t id);
