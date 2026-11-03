/* Signal report: the decoder status reads
 * ADV_DetectLoop already does (no extra I2C), turned into debounced fields for ADV_CMD_SIGNAL. DDL-free (host
 * tests). */
#ifndef SIGNAL_H
#define SIGNAL_H
#include <stdint.h>
#include "adv-link.h"

void SIG_Reset(void);
/* After each status read (powered = 1) or each detect tick with the decoder off (powered = 0) */
void SIG_Sample(uint8_t powered, uint8_t ack, uint8_t status1, uint8_t status2, uint8_t status3);
void SIG_Get(AdvSignal* out, uint8_t vidSel, uint8_t encoder, uint8_t input);

#endif
