// Loop watch, device side: loop() marks the end of each stretch; a slow pass logs its slowest stretch (debug line)
#pragma once
#include <stdint.h>

enum LoopStretch : uint8_t {
    LW_API = 1,     // settings, log, apiLoop
    LW_PROFILE,     // profile ops, profile tick, auto-load
    LW_LINK,        // ADV Controller link
    LW_MENU,        // keys, menu engine, OSD/OLED renderers
    LW_OSD,         // STV9426 driver
    LW_OTA,         // OTA client, Wi-Fi power
    LW_AUDIO,       // buttons, LED, audio volume
    LW_WEB,         // handleWiFi (web server, WebSocket, mDNS)
    LW_COMMANDS,    // serial and web commands
    LW_FRAMELOCK,   // frame time lock
    LW_SYNCWATCH,   // sync watcher and its timers
    LW_PRESET,      // HTotal, preset done stages
    LW_INPUT,       // Source = Auto
    LW_WIFITUNE,    // WiFi optimization
    LW_DETECT,      // source detection without a signal, board power
    LW_COUNT
};

void loopWatchBegin();               // top of loop()
void loopWatchMark(uint8_t stretch);
