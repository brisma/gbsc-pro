// Console log, device side: every line gets a category, a
// level and the uptime, goes into the 2 KB RAM ring (GET /api/log) and out on the WebSocket ("l:") and, where GPIO1
// is free, the USB serial. Debug lines only with system.devmode on. Lines are written from the main loop.
#pragma once
#include <Arduino.h>
#include "log-ring.h"
#include "log-lines.h"

void logPrintf(uint8_t cat, uint8_t level, PGM_P fmt, ...);   // fmt: PSTR("..."); text past LOG_TEXT_MAX is cut
void logText(uint8_t cat, uint8_t level, const char* text);   // RAM text, never a format
bool logDebugOn();                                            // system.devmode, read on every call
void logMark(uint8_t kind);                                   // upstream sync marks (LM_*), summarised by logLoop()
void logTagNext(uint8_t cat, uint8_t level, bool ring = true);   // category/level of the next upstream line
void logUpstreamWrite(const char* data, size_t n);            // SerialMirror (gbs-control.ino)
void logUpstreamFlush();
void logBegin();                                              // first thing in setup(): a new boot id for /api/log
void logLoop();                                               // pending upstream text, marks summary, Wi-Fi watcher
const LogRing& logRing();
// The WebSocket starts off (on after webSocket.begin()), the USB echo on (it still follows the board probe in the
// sink); safe mode turns both off: GPIO1 is the ADV Controller link wire on a Pro and it runs before the probes
void logSinkEnable(bool ws, bool usb);
void logWifiDropped(uint8_t reason);                          // Wi-Fi SDK callback: stores the reason only
void logPresetContext();                                      // after upstream's "preset applied": input and profile

// gbs-control.ino (it owns webSocket): one formatted line ("[   1.234] [gbs]      text\n")
void logSinkLine(const char* line, size_t len, bool ws, bool usb);
