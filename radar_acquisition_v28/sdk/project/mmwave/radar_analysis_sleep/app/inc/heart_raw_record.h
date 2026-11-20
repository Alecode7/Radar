#ifndef HEART_RAW_RECORD_H
#define HEART_RAW_RECORD_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef struct {
    uint32_t frame,amplitude;
    int32_t phase;
    uint16_t bin;
    uint8_t person,active,bed,antenna;
    int16_t real,imag,delta,iq[6];
} HeartRawRecord;
/* Exactly one complete legacy line. A too-small buffer returns failure; the
 * caller must not enqueue a truncated diagnostic record. No HR math here. */
int hrraw_encode(char *buffer,size_t capacity,const HeartRawRecord *s);
#ifdef HF_AUX_IMPLEMENTATION
int hrraw_encode(char *buffer,size_t capacity,const HeartRawRecord *s) {
    int n=snprintf(buffer,capacity,
        "HRRAW F=%lu FS=5 V=1 P=%u A=%u BED=%u BIN=%u ANT=%u I=%d Q=%d AMP=%lu PH=%ld DPH=%d I0=%d Q0=%d I1=%d Q1=%d I2=%d Q2=%d\r\n",
        (unsigned long)s->frame,(unsigned)s->person,(unsigned)s->active,(unsigned)s->bed,
        (unsigned)s->bin,(unsigned)s->antenna,(int)s->real,(int)s->imag,(unsigned long)s->amplitude,
        (long)s->phase,(int)s->delta,(int)s->iq[0],(int)s->iq[1],(int)s->iq[2],(int)s->iq[3],(int)s->iq[4],(int)s->iq[5]);
    return n<0 || (size_t)n>=capacity?-1:n;
}
#endif
#endif
