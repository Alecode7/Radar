#ifndef V6_HEART_DEVICE_H
#define V6_HEART_DEVICE_H
#include <stdbool.h>
#include <stdint.h>
int v6_heart_init(void);
/* V28 retains these legacy names for the unchanged radar callback/shell.
 * This adapter streams IQ only; it never creates a heart computation task. */
void v6_heart_start(void);
void v6_heart_stop(void);
void v6_heart_reset(void);
void v6_heart_submit(uint32_t frame,uint16_t bin,bool active,const int16_t iq[6]);
/* Only after an actual SLEEP line, matching the old PC breath context. */
void v6_heart_observe_status(bool in_bed,uint8_t breath);
void v6_heart_emit(void);
#endif
