// The menu on the device: engine + backend + OSD and OLED renderers + IR and encoder input
#pragma once
#include <stdint.h>

void menuEncoderAttach();            // setup(), after the capability detection (no-op without CAP_ENCODER)
void menuDeviceBegin();              // setup(), after the settings, profiles and capability detection
void menuDeviceKey(uint8_t k);       // MenuKey from loop context (HAVE_BUTTONS builds)
void menuNotifyAutoload(const char* name, const char* discarded);   // auto-load: after its POP_LOAD was queued (discarded: whose unsaved changes it drops, or null)
void menuNotice(uint16_t str);       // a one-off toast (S_* string id) from the device code
void menuDeviceLoop();               // loop(), before osdDriverLoop() and the upload return
bool menuUiBusy();                   // a menu or an overlay is shown
struct MenuSignal;
const MenuSignal& menuDeviceSignal();   // the menu's signal snapshot (diagnostics)
void menuToggleIrLog();              // serial 'I' / dev.ir_codes: log decoded IR codes
bool menuIrLogOn();
int32_t menuDeviceLive(uint8_t lv);   // MenuLive readback (GET /api/live cache; loop context: reads the scaler)                  // Developer > Serial log > IR codes log readback
