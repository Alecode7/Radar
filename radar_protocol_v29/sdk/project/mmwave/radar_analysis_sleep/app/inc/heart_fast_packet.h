#ifndef HEART_FAST_PACKET_H
#define HEART_FAST_PACKET_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Canonical metadata words, all uint32 little endian, followed by18 int16 IQ
 * and, only ifOUT=1, nine int32 filtered phase values. FNV1a32 covers all.
 * SRC1=direct static-Doppler, SRC2=vendor gain-normalized static-Doppler.
 * RF counts callbacks; neither its continuity nor MS proves zero ADC loss. */
static inline uint32_t hff_byte(uint32_t sum,uint8_t value) { return (sum^value)*16777619u; }
static inline uint32_t hff_word(uint32_t sum,uint32_t value,unsigned bytes) {
    unsigned i;for(i=0;i<bytes;i++)sum=hff_byte(sum,(uint8_t)(value>>(i*8u)));return sum;
}
static inline int hff_encode(char *buffer,size_t capacity,const uint32_t meta[14],
                             const int16_t iq[18],const int32_t phase[9]) {
    static const char hex[]="0123456789abcdef";uint32_t sum=2166136261u;int n;size_t at;unsigned i,j;
    if(buffer==NULL || meta==NULL || iq==NULL || phase==NULL || capacity<2u || meta[0]!=1u || meta[1]==0u ||
       (meta[5]!=1u && meta[5]!=2u) || meta[6]!=20u || meta[7]!=5u || meta[8]>7u ||
       meta[9]>65532u || meta[10]!=3u || meta[11]>1u || meta[12]>511u)return -1;
    n=snprintf(buffer,capacity,"HFF V=1 GEN=%lu RF=%lu SF=%lu MS=%lu SRC=%lu FS=20 OF=5 FLAGS=%lu START=%lu N=3 OUT=%lu MASK=%lu GAP=%lu DATA=",
        (unsigned long)meta[1],(unsigned long)meta[2],(unsigned long)meta[3],(unsigned long)meta[4],
        (unsigned long)meta[5],(unsigned long)meta[8],(unsigned long)meta[9],(unsigned long)meta[11],
        (unsigned long)meta[12],(unsigned long)meta[13]);
    if(n<0 || (size_t)n>=capacity)return -1;
    at=(size_t)n;
    for(i=0;i<14u;i++)sum=hff_word(sum,meta[i],4u);
    if(at+72u+4u+(meta[11]?72u:0u)+16u>=capacity)return -1;
    for(i=0;i<18u;i++) {
        uint16_t value=(uint16_t)iq[i];sum=hff_word(sum,value,2u);
        for(j=0;j<2u;j++) {uint8_t b=(uint8_t)(value>>(8u*j));buffer[at++]=hex[b>>4];buffer[at++]=hex[b&15u];}
    }
    buffer[at++]=' ';buffer[at++]='P';buffer[at++]='H';buffer[at++]='=';
    if(meta[11])for(i=0;i<9u;i++) {
        uint32_t value=(uint32_t)phase[i];sum=hff_word(sum,value,4u);
        for(j=0;j<4u;j++) {uint8_t b=(uint8_t)(value>>(8u*j));buffer[at++]=hex[b>>4];buffer[at++]=hex[b&15u];}
    }
    n=snprintf(buffer+at,capacity-at," SUM=%08lx\r\n",(unsigned long)sum);
    return n<0 || (size_t)n>=capacity-at?-1:(int)(at+(size_t)n);
}
#endif
