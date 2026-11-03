// STV9426 on the device: the global driver over Wire (stv9426-wire.cpp)
#pragma once
#include "stv9426-driver.h"

Stv9426& osdDriver();
OsdBus& osdBus();         // the chip itself (Developer readback)
void osdDriverBegin();   // always binds the compat layer; programs the chip only when CAP_OSD is present
void osdDriverLoop();    // loop(): follows the running output preset, pending power-on, flush
void osdReadWindow();    // TV5725 output window -> horizontal placement (on OSD open and after Screen adjust)
