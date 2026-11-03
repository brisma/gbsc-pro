/* ADV7391 encoder: ED 525p/625p following the decoder's output rate, with I2P on or off (the ADV Controller decides
 * it by itself: it is a hardware coupling) */
#ifndef ADV7391_H
#define ADV7391_H
#include <stdint.h>

#define ENCODER_ADDR 0x56

enum { ENC_MODE_NONE = 0, ENC_MODE_525P = 1, ENC_MODE_625P = 2 };

/* status3 = ADV7280 STATUS_3 (0x13) just read: SD_OP_50Hz (bit 2) is the decoder's output rate, also with a forced
 * format or in free-run. Writes only what changed; a failed write is retried on the next call */
void ENC_FollowOutput(uint8_t status3);
void ENC_Forget(void);          /* the ADV7391 was reset into the ED base (I2C_AUTO_ENCODER): next follow writes */
void ENC_Lost(void);            /* its state is unknown (a failed init write): next follow resets and reloads */
uint8_t ENC_Mode(void);         /* ENC_MODE_* as last written (signal report "encoder") */

#endif
