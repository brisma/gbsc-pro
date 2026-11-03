#ifndef STUB_HAL_H
#define STUB_HAL_H
#include "link_hal.h"
extern char gLog[2048][48];
extern int gLogN;
extern AdvRegs gRegs;
extern AdvSwitches gSw;
void stubReset(void);
extern uint32_t gNowUs;
extern int gDropReply, gReplies;
extern uint8_t gReplyCmd, gReplySeq, gReplyLen, gReplyPayload[ADV_MAX_PAYLOAD];
extern uint32_t gReplyNotBefore, gReplyDeadline;
extern uint8_t gStaging[LINK_STAGING_SIZE];
extern int gProgramFail;
extern int gI2cReadFail;
extern int gHalFail;   /* ADV_STEP_* groups whose HAL call reports an I2C failure */
#endif
