// ESP side of the ADV Controller link v3:
// builds the ADV state from uopt every loop pass and sends it as a STATE frame when it changes
#pragma once
#include <stdint.h>
#include "adv-link-client.h"

void advLinkBegin();                                   // setup(), after the settings are loaded: first STATE now
void advLinkLoop();                                    // loop()
void advLinkKick();                                    // input switch: send without the coalescing delay
void advLinkI2c(const uint8_t* bytes, uint8_t len);    // dev.adv_i2c: (addr, reg, val) triplets
const AdvControllerInfo& advLinkController();                         // ADV Controller status from PING/INFO (menu, /api/info)
const AdvSignal* advLinkSignal();                      // SIGNAL report less than 5 s old, else nullptr
uint32_t advLinkSignalAtMs();                          // millis() of the last SIGNAL report (newer than an input switch?)
void advLinkSignalPollFast(bool on);                   // auto input scan: poll SIGNAL every 500 ms
// Input search identification: the decoder listens on pin 20 while the TV5725 stays on RGB SCART
void advLinkSetListen(bool on);                        // STATE at once on a change (RGB SCART only, adv-state.cpp)
bool advLinkListenRequested();
uint32_t advLinkSwitchSentAt();                        // millis() of the STATE that changed listen or the termination, 0 never
bool advLinkCanListen();                               // the ADV Controller answers and reports ADV_INFO_CAP_LISTEN
bool advLinkChecking();                                // the boot check of the ADV Controller has not ended yet
// dev.adv_read: the web handler announces a read (its gen goes back in the POST response), the loop queues it
uint8_t advLinkRegReadAnnounce();                      // network context
void advLinkRegRead(uint8_t dev, uint8_t map, uint8_t reg, uint8_t count, uint8_t gen);
uint8_t advLinkRegReadAnnounced();
const AdvRegRead& advLinkRegReadResult();
bool advLinkNoReplyNotice();                           // true once when the boot check ends with no reply
bool advLinkControllerUpdateAvailable();                     // the ADV Controller runs another build than the embedded image
bool advLinkUpdateCanStart();                          // read-only check (web 409)
bool advLinkUpdateStart();                             // false when it cannot start (no image, no reply, OTA...)
bool advLinkUpdateActive();
const char* advLinkUpdateStatus();                     // "running", or the last outcome: idle, done, failed:<step>
int advLinkUpdateProgress();                           // 0..100 while updating, -1 otherwise
uint8_t advLinkUpdateResult();                         // 1 updated, 2 failed, 0 nothing new (read once)
