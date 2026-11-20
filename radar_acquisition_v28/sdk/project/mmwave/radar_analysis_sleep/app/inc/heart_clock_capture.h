#ifndef HEART_CLOCK_CAPTURE_H
#define HEART_CLOCK_CAPTURE_H
#include <stdint.h>
/* Stable split read on a 32-bit target. Raw counter only, no assumed scaling,
 * timer reconfiguration, frequency correction or heart-rate adjustment. */
void hc_read(uint32_t (*high)(void),uint32_t (*low)(void),uint32_t *hi,uint32_t *lo);
#ifdef HF_AUX_IMPLEMENTATION
void hc_read(uint32_t (*high)(void),uint32_t (*low)(void),uint32_t *hi,uint32_t *lo) {
    uint32_t a,b,value;
    do {a=high();value=low();b=high();}while(a!=b);
    *hi=b;*lo=value;
}
#endif
#endif
