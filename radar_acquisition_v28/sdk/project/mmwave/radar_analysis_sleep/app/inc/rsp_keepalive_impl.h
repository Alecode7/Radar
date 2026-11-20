#ifndef RSP_KEEPALIVE_IMPL_H
#define RSP_KEEPALIVE_IMPL_H
#include "rsp_keepalive.h"
size_t rsp_keepalive_encode(uint64_t uptime,uint32_t age,uint32_t flags,uint32_t seq,uint8_t *p,size_t cap) {
    if(!p || cap<RSP_KEEPALIVE_FRAME || !seq || (flags&~7u))return 0u;
    memset(p,0,RSP_KEEPALIVE_FRAME);p[0]=0x52u;p[1]=0x53u;p[2]=1u;p[3]=1u;p[4]=4u;
    p[6]=2u;p[7]=0x81u;
    for(unsigned i=0;i<4u;i++) {p[8u+i]=(uint8_t)(seq>>(8u*i));p[22u+i]=(uint8_t)(age>>(8u*i));p[26u+i]=(uint8_t)(flags>>(8u*i));}
    p[12]=16u;
    for(unsigned i=0;i<8u;i++)p[14u+i]=(uint8_t)(uptime>>(8u*i));
    uint16_t crc=rsp_result_crc(p+2,28u);p[30]=(uint8_t)crc;p[31]=(uint8_t)(crc>>8);return RSP_KEEPALIVE_FRAME;
}
#endif
