// Safe mode: entered at boot after repeated crash resets; WiFi + firmware upload + backup only
#pragma once
#include "update-rtc.h"

// RTC record: safe mode crash counter and the restart note
RtcRecord& rtcRecord();
void rtcLoad();
void rtcStore();

bool safeModeCheckOnBoot();   // reads/updates the RTC record; true: call safeModeRun()
void safeModeRun();           // never returns
bool safeModeActive();        // true while safeModeRun() serves (/api/info safe_mode)
void safeModeHealthyTick();   // call from loop(): marks the firmware healthy after 30 s
// The last crash from RTC memory as JSON (`null` when none): cause, epc1-3, excvaddr, depc, stack code addresses
int crashInfoJson(char* out, size_t n);
void crashLogBoot();   // console log line for the crash recorded in RTC (setup, after the boot line)
void restartNoteSet(uint8_t note);   // RestartNote for the next boot's log (call right before a restart)
void restartNoteLogBoot();           // console log line for the note left before this restart (setup), then cleared
