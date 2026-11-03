// Board detection and board-specific I/O (device only)
#pragma once
#include <stdint.h>
#include "platform-caps.h"

void platformDetect();                               // probes 0x5D, 0x44, 0x3C; call right after Wire starts
uint8_t platformApplyBoardSetting(uint8_t board);    // system.board override; returns the previous caps
const PlatformProbes& platformProbes();
// A Pro chip answered: the ADV Controller link and the Pro pins exist, whatever system.board says (the setting
// picks the features; electrical use of GPIO1/GPIO2/GPIO15 follows the hardware)
bool platformLinkHardware();
void platformLed(bool on);                           // GPIO2 status LED, standard board only
const char* platformBoardName();                     // "pro" or "standard"
uint8_t platformProbeRounds();             // probe rounds needed at boot (diagnostics)
uint8_t platformBusAtBoot();               // bit0 SDA, bit1 SCL high before probing
