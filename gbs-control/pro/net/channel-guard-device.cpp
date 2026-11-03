#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}
#include "channel-guard.h"
#include "channel-guard-device.h"
#include "../log/log.h"

static ChannelGuard gGuard;
static WiFiEventHandler gOnUp, gOnDown;
static volatile uint8_t gUpChannel = 0, gDownReason = 0;
static volatile bool gUp = false, gDown = false;   // set in the SDK's context, taken by the loop

void channelGuardBegin() {
    gOnUp = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& e) { gUpChannel = e.channel; gUp = true; });
    gOnDown = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& e) { gDownReason = e.reason; gDown = true; });
}

void channelGuardLoop() {
    static uint32_t minuteAt = 0, minuteBase = 0;
    uint32_t now = millis();
    if (gDown) { gDown = false; gGuard.dropped(gDownReason, now); }
    if (gUp) { gUp = false; gGuard.connected(gUpChannel); }
    // From the association on, not from the address: the SDK leaves the channel before DHCP ends
    if (!gGuard.home() || WiFi.getMode() != WIFI_STA) return;
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;   // a scan walks the channels on purpose
    uint8_t home = gGuard.home();
    ChannelGuardStep s = gGuard.tick(wifi_get_channel(), now);
    if (s.setChannel && !wifi_set_channel(s.setChannel))
        logPrintf(LC_WIFI, LL_DEBUG, PSTR("channel guard: the radio refused channel %u"), s.setChannel);
    if (s.event == CG_PROBE)
        logPrintf(LC_WIFI, LL_DEBUG, PSTR("channel guard: the router announced a move from %u to %u, checking"), home, gGuard.announced());
    else if (s.event == CG_FALSE)
        logPrintf(LC_WIFI, LL_INFO, PSTR("the router keeps announcing channel %u and stays on %u: radio held on %u"),
                  gGuard.announced(), home, home);
    else if (s.event == CG_FOLLOWED)
        logPrintf(LC_WIFI, LL_INFO, PSTR("the router moved from channel %u to %u"), home, s.setChannel);
    if (gGuard.distrust() && now - minuteAt >= 60000) {
        if (gGuard.reverts() != minuteBase)
            logPrintf(LC_WIFI, LL_DEBUG, PSTR("channel guard: radio put back on %u, %lu times in the last minute"), home,
                      (unsigned long)(gGuard.reverts() - minuteBase));
        minuteAt = now;
        minuteBase = gGuard.reverts();
    }
}

unsigned char channelGuardChannel() { return gGuard.home() ? gGuard.home() : (unsigned char)WiFi.channel(); }
