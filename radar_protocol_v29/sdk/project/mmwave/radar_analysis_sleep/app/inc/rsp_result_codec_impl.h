#ifndef RSP_RESULT_CODEC_IMPL_H
#define RSP_RESULT_CODEC_IMPL_H
#include "rsp_result.h"
#include <string.h>
static uint16_t rr_get16(const uint8_t *p) {return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rr_get32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t rr_get64(const uint8_t *p) {return rr_get32(p)|((uint64_t)rr_get32(p+4)<<32);}
static void rr_put16(uint8_t *p,uint16_t n) {p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void rr_put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(n>>(8u*i));}
static void rr_put64(uint8_t *p,uint64_t n) {rr_put32(p,(uint32_t)n);rr_put32(p+4,(uint32_t)(n>>32));}
uint16_t rsp_result_crc(const uint8_t *p,size_t length) {
    uint16_t crc=0xffffu;
    for(size_t i=0;i<length;i++) {crc^=(uint16_t)((uint16_t)p[i]<<8);
        for(unsigned b=0;b<8u;b++)crc=(uint16_t)((crc<<1)^((crc&0x8000u)?0x1021u:0u));}
    return crc;
}
static bool rr_valid(const RspResult *r) {
    if(r->presence>2u || r->bed>2u || r->motion>2u || r->sleep_state>4u ||
       r->breath_state>3u || r->heart_state>2u || r->heart_kind>2u || r->heart_quality>3u || (r->flags&~63u))return false;
    if(r->heart_state) {
        if(r->bed!=2u || !r->heart_kind || !r->heart_sample_seq || r->heart_x10<480u ||
           r->heart_x10>1200u || r->heart_age_ms==UINT32_MAX)return false;
        if(r->heart_state==1u && r->heart_age_ms>10000u)return false;
    } else if(r->heart_x10!=UINT16_MAX || r->heart_kind || r->heart_quality || r->heart_age_ms!=UINT32_MAX || r->heart_sample_seq)return false;
    if(r->breath_state) {
        if(r->bed!=2u || !r->breath_x10 || r->breath_x10==UINT16_MAX)return false;
        if(r->breath_state==1u && (r->breath_age_ms==UINT32_MAX || r->breath_age_ms>5000u))return false;
        if(r->breath_state==3u && r->breath_age_ms!=UINT32_MAX)return false;
    } else if(r->breath_x10!=UINT16_MAX || r->breath_age_ms!=UINT32_MAX)return false;
    if(r->bed==0u && (r->in_bed_s!=UINT32_MAX || r->out_bed_s!=UINT32_MAX))return false;
    if(r->bed==1u && r->in_bed_s!=0u)return false;
    if(r->bed==2u && r->out_bed_s!=0u)return false;
    return true;
}
size_t rsp_result_encode(const RspResult *r,uint32_t seq,uint8_t *out,size_t capacity) {
    if(!r || !out || capacity<RSP_RESULT_FRAME || !seq || !rr_valid(r))return 0u;
    memset(out,0,RSP_RESULT_FRAME);out[0]=0x52;out[1]=0x53;out[2]=1;out[3]=1;out[4]=4;
    rr_put16(out+6,RSP_RESULT_ID);rr_put32(out+8,seq);rr_put16(out+12,RSP_RESULT_PAYLOAD);
    uint8_t *p=out+14;
    rr_put64(p,r->uptime_ms);rr_put32(p+8,r->generation);
    p[12]=r->presence;p[13]=r->bed;p[14]=r->motion;p[15]=r->sleep_state;
    rr_put16(p+16,r->breath_x10);rr_put16(p+18,r->heart_x10);
    p[20]=r->breath_state;p[21]=r->heart_state;p[22]=r->heart_kind;p[23]=r->heart_quality;
    rr_put32(p+24,r->breath_age_ms);rr_put32(p+28,r->heart_age_ms);
    rr_put32(p+32,r->breath_sample_seq);rr_put32(p+36,r->heart_sample_seq);
    rr_put32(p+40,r->in_bed_s);rr_put32(p+44,r->out_bed_s);rr_put32(p+48,r->flags);rr_put32(p+52,r->faults);
    rr_put16(out+70,rsp_result_crc(out+2,68u));return RSP_RESULT_FRAME;
}
int rsp_result_decode(const uint8_t *data,size_t length,RspResult *r,uint32_t *seq) {
    if(!data || !r || !seq || length!=RSP_RESULT_FRAME || data[0]!=0x52 || data[1]!=0x53 ||
       data[2]!=1 || data[3]!=1 || data[4]!=4 || data[5] || rr_get16(data+6)!=RSP_RESULT_ID ||
       rr_get16(data+12)!=RSP_RESULT_PAYLOAD || !rr_get32(data+8) || rr_get16(data+70)!=rsp_result_crc(data+2,68u))return 0;
    const uint8_t *p=data+14;
    r->uptime_ms=rr_get64(p);r->generation=rr_get32(p+8);
    r->presence=p[12];r->bed=p[13];r->motion=p[14];r->sleep_state=p[15];
    r->breath_x10=rr_get16(p+16);r->heart_x10=rr_get16(p+18);
    r->breath_state=p[20];r->heart_state=p[21];r->heart_kind=p[22];r->heart_quality=p[23];
    r->breath_age_ms=rr_get32(p+24);r->heart_age_ms=rr_get32(p+28);
    r->breath_sample_seq=rr_get32(p+32);r->heart_sample_seq=rr_get32(p+36);
    r->in_bed_s=rr_get32(p+40);r->out_bed_s=rr_get32(p+44);r->flags=rr_get32(p+48);r->faults=rr_get32(p+52);
    *seq=rr_get32(data+8);return rr_valid(r)?1:0;
}
static void rr_consume(RspResultParser *s) {s->used--;memmove(s->bytes,s->bytes+1,s->used);}
static void rr_drain(RspResultParser *s,RspResultReceive callback,void *context) {
    while(s->used) {
        if(s->bytes[0]!=0x52) {rr_consume(s);continue;}
        if(s->used<2u)return;
        if(s->bytes[1]!=0x53) {rr_consume(s);continue;}
        if(s->used<14u)return;
        if(s->bytes[2]!=1 || s->bytes[3]!=1 || s->bytes[4]!=4 || s->bytes[5] ||
            rr_get16(s->bytes+6)!=RSP_RESULT_ID || rr_get16(s->bytes+12)!=RSP_RESULT_PAYLOAD) {
            s->errors++;rr_consume(s);continue;
        }
        if(s->used<RSP_RESULT_FRAME)return;
        RspResult result;uint32_t seq;
        if(rsp_result_decode(s->bytes,RSP_RESULT_FRAME,&result,&seq)) {
            if(callback)callback(&result,seq,context);
            s->used=0u;
        } else {s->errors++;rr_consume(s);}
    }
}
void rsp_result_parser_init(RspResultParser *s) {if(s)memset(s,0,sizeof(*s));}
void rsp_result_parser_poll(RspResultParser *s,uint32_t now,RspResultReceive callback,void *context) {
    if(!s)return;
    if(s->used && now-s->last_ms>200u) {
        s->errors++;
        while(s->used) {rr_consume(s);rr_drain(s,callback,context);}
    }
}
void rsp_result_parser_feed(RspResultParser *s,const uint8_t *data,size_t length,uint32_t now,RspResultReceive callback,void *context) {
    if(!s || (length && !data))return;
    rsp_result_parser_poll(s,now,callback,context);
    for(size_t i=0;i<length;i++) {
        if(s->used>=RSP_RESULT_FRAME) {s->errors++;rr_consume(s);}
        s->bytes[s->used++]=data[i];s->last_ms=now;rr_drain(s,callback,context);
    }
}
#endif
