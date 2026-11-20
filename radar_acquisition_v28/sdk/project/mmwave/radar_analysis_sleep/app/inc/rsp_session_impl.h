#ifndef RSP_SESSION_IMPL_H
#define RSP_SESSION_IMPL_H
#include "rsp_session.h"
/* Included after the validated query framing helpers, in the same C unit. */
static uint64_t rs_get64(const uint8_t *p) {return rq_get32(p)|((uint64_t)rq_get32(p+4)<<32);}
static bool rs_health_valid(const uint8_t *f,size_t n) {
    if(n!=RSP_HEALTH_BODY || f[16]>RSP_INIT_FAULT || f[17]>RSP_STAGE_COMPLETE || rq_get16(f+18))return false;
    if((f[16]==RSP_INIT_UNKNOWN && f[17]!=0u) || (f[16]==RSP_INIT_READY && f[17]!=RSP_STAGE_COMPLETE) ||
       ((f[16]==RSP_INIT_BOOTING || f[16]==RSP_INIT_FAULT) && (f[17]<RSP_STAGE_OPEN || f[17]>RSP_STAGE_HEART)))return false;
    uint32_t age=rq_get32(f+8),flags=rq_get32(f+12);
    if((flags&~7u) || (!(flags&1u) && (age!=UINT32_MAX || !(flags&2u))) ||
       ((flags&4u) && (age!=UINT32_MAX || !(flags&2u))) || (!(flags&2u) && age>5000u))return false;
    return true;
}
void rsp_session_reset(RspSession *s) {if(s)memset(s,0,sizeof(*s));}
static size_t rq_session_handle(const RspQueryProvider *provider,const uint8_t *in,uint8_t *out) {
    uint16_t id=rq_get16(in+6),length=rq_get16(in+12),result=0u,detail=0u;
    uint32_t seq=rq_get32(in+8);uint64_t token=length==8u?rs_get64(in+14):0u;
    RspSession *s=provider?provider->session:NULL;
    uint8_t *body=out+18;size_t body_size=8u;
    if(length!=8u) {result=RSP_QUERY_BAD_LENGTH;detail=8u;}
    else if(!token)result=RSP_SESSION_BAD_ARGUMENT;
    else if(!s)result=RSP_QUERY_UNSUPPORTED;
    else if(id==RSP_SESSION_OPEN_ID) {
        if(s->token!=token) {s->token=token;s->highest_sequence=seq;s->open_sequence=seq;}
        else if(s->open_sequence!=seq)result=RSP_SESSION_OLD_SEQUENCE;
        if(!result) {
            rq_put64(body+8,provider->uptime_ms?provider->uptime_ms(provider->context):0u);
            rq_put32(body+16,rq_capabilities(provider));rq_put32(body+20,31u);body_size=24u;
        }
    } else if(s->token!=token)result=RSP_SESSION_REQUIRED;
    else if(seq<s->highest_sequence || seq==s->open_sequence)result=RSP_SESSION_OLD_SEQUENCE;
    else {
        s->highest_sequence=seq;
        if(id==RSP_SESSION_HEALTH_ID) {
            uint8_t frame[RSP_HEALTH_BODY];
            int n=provider->read_health?provider->read_health(frame,sizeof(frame),provider->context):-1;
            if(!provider->read_health)result=RSP_QUERY_UNSUPPORTED;
            else if(n<=0)result=RSP_QUERY_NOT_READY;
            else if(!rs_health_valid(frame,(size_t)n))result=RSP_QUERY_INTERNAL;
            else {memcpy(body+8,frame,RSP_HEALTH_BODY);body_size=8u+RSP_HEALTH_BODY;}
        } else {
        uint8_t frame[RSP_RESULT_FRAME];RspResult value;uint32_t event_seq;
        int n=provider->read_result?provider->read_result(frame,sizeof(frame),provider->context):-1;
        if(n<=0)result=RSP_QUERY_NOT_READY;
        else if(n!=(int)sizeof(frame) || !rsp_result_decode(frame,(size_t)n,&value,&event_seq))result=RSP_QUERY_INTERNAL;
        else {memcpy(body+8,frame+14,RSP_RESULT_PAYLOAD);body_size+=RSP_RESULT_PAYLOAD;}
        }
    }
    /* Unlike legacy queries, session errors always echo 8 bytes of token.
     * Copy request fields first, so in==out remains supported. */
    rq_put64(body,token);
    out[0]=0x52u;out[1]=0x53u;out[2]=1u;out[3]=1u;out[4]=2u;out[5]=0u;
    rq_put16(out+6,id);rq_put32(out+8,seq);rq_put16(out+12,(uint16_t)(4u+body_size));
    rq_put16(out+14,result);rq_put16(out+16,detail);
    size_t size=20u+body_size;rq_put16(out+size-2u,rsp_result_crc(out+2,size-4u));return size;
}
#endif
