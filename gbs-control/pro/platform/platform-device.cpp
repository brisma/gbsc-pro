#include <Arduino.h>
#include <Wire.h>
#include "platform-device.h"

static PlatformProbes gProbes;
static uint8_t gRounds = 0, gBusAtBoot = 3;     // probe rounds; bit0 SDA, bit1 SCL level before the first round

static ProbeResult probe(uint8_t addr) {
    ProbeResult r = {false, 0};
    for (uint8_t i = 0; i < 3; i++) {
        r.attempts++;
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) { r.ack = true; break; }
        delay(2);   // a slow STV9426 power-up can miss the first probe
    }
    return r;
}

// A reset in the middle of an I2C transfer can leave a slave holding SDA low: every probe then fails and a
// Pro board looks like a standard board (seen after an esptool reset). Clock SCL until SDA is
// released, send a STOP, then give the Wire library the pins back.
static uint8_t busLevels() { return (uint8_t)((digitalRead(SDA) == HIGH ? 1 : 0) | (digitalRead(SCL) == HIGH ? 2 : 0)); }

static void i2cBusRecover() {
    pinMode(SDA, INPUT_PULLUP);
    pinMode(SCL, INPUT_PULLUP);
    delayMicroseconds(10);
    if (digitalRead(SDA) == HIGH && digitalRead(SCL) == HIGH) return;   // idle bus
    pinMode(SCL, OUTPUT_OPEN_DRAIN);
    for (uint8_t i = 0; i < 9 && digitalRead(SDA) == LOW; i++) {
        digitalWrite(SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(SCL, HIGH);
        delayMicroseconds(5);
    }
    pinMode(SDA, OUTPUT_OPEN_DRAIN);                // STOP: SDA rises while SCL is high
    digitalWrite(SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(SDA, HIGH);
    delayMicroseconds(5);
    Wire.begin(SDA, SCL);
}

static void probeAll() {
    gProbes.stv = probe(0x5D);
    gProbes.pt = probe(0x44);
    gProbes.oled = probe(0x3C);
}

// Up to 5 rounds ~200 ms apart until a Pro chip answers (a standard board pays ~1 s at boot, with or without
// the OLED; a Pro is never demoted by a slave that is still busy after a reset)
void platformDetect() {
    pinMode(SDA, INPUT_PULLUP);
    pinMode(SCL, INPUT_PULLUP);
    delayMicroseconds(10);
    gBusAtBoot = busLevels();
    for (gRounds = 1; ; gRounds++) {
        i2cBusRecover();
        probeAll();
        if (platformDetectDone(gProbes, gRounds)) break;
        delay(200);
    }
    platformSetCaps(platformCapsFrom(gProbes, BOARD_AUTO));
}

uint8_t platformProbeRounds() { return gRounds; }
uint8_t platformBusAtBoot() { return gBusAtBoot; }

uint8_t platformApplyBoardSetting(uint8_t board) {
    uint8_t before = platformCaps();
    platformSetCaps(platformCapsFrom(gProbes, board));
    return before;
}

const PlatformProbes& platformProbes() { return gProbes; }

bool platformLinkHardware() { return platformProbesSayPro(gProbes); }

// Active low, open-drain style: never driven high, so a lost detection race on a Pro (GPIO2 = IR receiver output,
// wired straight to the pin with a 4.7k pull-up) cannot fight the receiver; off = input
void platformLed(bool on) {
    if (!hwHas(CAP_LED)) return;
    if (on) {
        digitalWrite(2, LOW);
        pinMode(2, OUTPUT);
    } else {
        pinMode(2, INPUT);
    }
}

const char* platformBoardName() { return hwHas(CAP_ORIGINAL) ? "standard" : "pro"; }
