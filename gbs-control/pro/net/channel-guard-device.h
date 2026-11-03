// Device glue of the channel guard: the SDK's association events, the radio channel and the log lines around
// ChannelGuard
#pragma once

void channelGuardBegin();   // once, before the station connects: registers the SDK event handlers
void channelGuardLoop();    // every pass (also inside long waits that keep the WiFi alive)
unsigned char channelGuardChannel();   // the router's channel while associated (the radio may be off it for an instant), else the radio's
