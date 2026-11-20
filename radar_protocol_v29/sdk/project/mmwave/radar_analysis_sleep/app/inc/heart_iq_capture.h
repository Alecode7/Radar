#ifndef SLEEP_HEART_IQ_CAPTURE_H
#define SLEEP_HEART_IQ_CAPTURE_H
/* Read-only diagnostic encoder. No estimator state, heap or device IO. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define HR_IQ_CAPTURE_MAX_BINS 24u
#define HR_IQ_CAPTURE_BUFFER_BYTES 768u
typedef bool (*HrIqRead)(void *context,uint16_t bin,int16_t iq[6]);
static uint32_t hr_iq_hash_byte(uint32_t hash,uint8_t byte) { return (hash^byte)*UINT32_C(16777619); }
static uint32_t hr_iq_hash_u32(uint32_t hash,uint32_t value) {
    unsigned i;for(i=0;i<4u;i++) hash=hr_iq_hash_byte(hash,(uint8_t)(value>>(i*8u)));return hash;
}
static size_t hr_iq_capture_encode(char *output,size_t capacity,uint32_t generation,uint32_t frame,uint32_t ms,
    uint8_t flags,uint16_t first,uint16_t count,uint16_t range_first,uint16_t range_end,uint16_t selected,
    HrIqRead read,void *context) {
    static const char hex[]="0123456789abcdef";
    uint32_t hash=UINT32_C(2166136261),metadata[11];size_t used;int length;unsigned i,j;
    if(output==NULL || capacity==0u) return 0u;
    output[0]='\0';
    if(read==NULL || generation==0u || flags>7u || count==0u || count>HR_IQ_CAPTURE_MAX_BINS ||
       first<range_first || (uint32_t)first+count>range_end || range_end<=range_first) return 0u;
    metadata[0]=1u;metadata[1]=generation;metadata[2]=frame;metadata[3]=ms;metadata[4]=flags;
    metadata[5]=first;metadata[6]=count;metadata[7]=3u;metadata[8]=range_first;metadata[9]=range_end;metadata[10]=selected;
    for(i=0;i<11u;i++) hash=hr_iq_hash_u32(hash,metadata[i]);
    length=snprintf(output,capacity,"HRIQ V=1 GEN=%lu F=%lu MS=%lu FLAGS=%u START=%u N=%u ANT=3 R0=%u R1=%u SEL=%u DATA=",
        (unsigned long)generation,(unsigned long)frame,(unsigned long)ms,(unsigned)flags,(unsigned)first,
        (unsigned)count,(unsigned)range_first,(unsigned)range_end,(unsigned)selected);
    if(length<0 || (size_t)length>=capacity) { output[0]='\0';return 0u; }
    used=(size_t)length;
    if(used+(size_t)count*24u+16u>=capacity) { output[0]='\0';return 0u; }
    for(i=0;i<count;i++) {
        int16_t iq[6];
        if(!read(context,(uint16_t)(first+i),iq)) { output[0]='\0';return 0u; }
        for(j=0;j<6u;j++) {
            uint16_t word=(uint16_t)iq[j];unsigned byte_index;
            for(byte_index=0;byte_index<2u;byte_index++) {
                uint8_t byte=(uint8_t)(word>>(byte_index*8u));
                hash=hr_iq_hash_byte(hash,byte);output[used++]=hex[byte>>4];output[used++]=hex[byte&15u];
            }
        }
    }
    length=snprintf(output+used,capacity-used," SUM=%08lx\r\n",(unsigned long)hash);
    if(length<0 || (size_t)length>=capacity-used) { output[0]='\0';return 0u; }
    return used+(size_t)length;
}
#endif
