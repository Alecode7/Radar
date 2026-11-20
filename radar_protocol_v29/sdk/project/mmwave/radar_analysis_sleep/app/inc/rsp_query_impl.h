#ifndef RSP_QUERY_IMPL_H
#define RSP_QUERY_IMPL_H
#include "rsp_query.h"
#include <string.h>

static uint16_t rq_get16(const uint8_t *p) {return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rq_get32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void rq_put16(uint8_t *p,uint16_t n) {p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void rq_put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(n>>(8u*i));}
static void rq_put64(uint8_t *p,uint64_t n) {rq_put32(p,(uint32_t)n);rq_put32(p+4,(uint32_t)(n>>32));}
static bool rq_header(const uint8_t *p) {
    return p[0]==0x52u && p[1]==0x53u && p[2]==1u && p[3]==1u &&
        p[4]==1u && !p[5] && rq_get32(p+8) && rq_get16(p+12)<=RSP_QUERY_MAX_PAYLOAD;
}
static bool rq_request(const uint8_t *p,size_t n) {
    return p && n>=16u && n<=RSP_QUERY_MAX_REQUEST && rq_header(p) &&
        n==16u+rq_get16(p+12) && rq_get16(p+n-2u)==rsp_result_crc(p+2,n-4u);
}

static uint32_t rq_capabilities(const RspQueryProvider *p) {
    uint32_t caps=31u;
    if(p && p->session) {caps|=32u;if(p->read_health)caps|=64u;}
    return caps;
}
#include "rsp_session_impl.h"
size_t rsp_query_handle(const RspQueryProvider *provider,const uint8_t *in,size_t n,uint8_t *out,size_t capacity) {
    if(!out || capacity<RSP_QUERY_MAX_RESPONSE || !rq_request(in,n))return 0u;
    /* Copy header before writing, permitting in == out. */
    uint16_t id=rq_get16(in+6),length=rq_get16(in+12),result=RSP_QUERY_OK,detail=0u;
    uint32_t sequence=rq_get32(in+8);size_t body=0u;
    if(id==RSP_SESSION_OPEN_ID || id==RSP_SESSION_RESULT_ID || id==RSP_SESSION_HEALTH_ID)return rq_session_handle(provider,in,out);
    uint8_t *p=out+18;
    if(id!=RSP_QUERY_CAPS_ID && id!=RSP_QUERY_RESULT_ID)result=RSP_QUERY_UNSUPPORTED;
    else if(length)result=RSP_QUERY_BAD_LENGTH; /* Expected length is zero. */
    else if(id==RSP_QUERY_CAPS_ID) {
        memset(p,0,RSP_QUERY_CAPS_BODY);p[0]=1u;p[1]=1u;rq_put16(p+2,1u);
        /* Only the two queries, Shell HEX adapter and sampling-driven stream. */
        rq_put32(p+4,rq_capabilities(provider));rq_put16(p+8,RSP_QUERY_MAX_PAYLOAD);rq_put16(p+10,68u);
        rq_put16(p+12,RSP_RESULT_ID);rq_put16(p+14,RSP_RESULT_PAYLOAD);rq_put32(p+16,2000u);
        /* No unique boot ID / BR timestamp / raw binary transport / control /
         * anomaly service. These restrictions are explicit on the wire. */
        rq_put32(p+20,31u);
        rq_put64(p+24,provider && provider->uptime_ms?provider->uptime_ms(provider->context):0u);
        if(provider && provider->firmware_tag) {
            size_t i=0u;while(i<31u && provider->firmware_tag[i]) {p[32u+i]=(uint8_t)provider->firmware_tag[i];i++;}
        }
        body=RSP_QUERY_CAPS_BODY;
    } else {
        uint8_t frame[RSP_RESULT_FRAME];RspResult value;uint32_t event_seq;
        int received=provider && provider->read_result?provider->read_result(frame,sizeof(frame),provider->context):-1;
        if(received<=0)result=RSP_QUERY_NOT_READY;
        else if(received!=(int)RSP_RESULT_FRAME || !rsp_result_decode(frame,(size_t)received,&value,&event_seq))result=RSP_QUERY_INTERNAL;
        else {memcpy(p,frame+14,RSP_RESULT_PAYLOAD);body=RSP_RESULT_PAYLOAD;}
    }
    /* Error responses contain only result/detail, never a partial success body. */
    if(result)body=0u;
    out[0]=0x52u;out[1]=0x53u;out[2]=1u;out[3]=1u;out[4]=2u;out[5]=0u;
    rq_put16(out+6,id);rq_put32(out+8,sequence);rq_put16(out+12,(uint16_t)(body+4u));
    rq_put16(out+14,result);rq_put16(out+16,detail);
    size_t size=20u+body;rq_put16(out+size-2u,rsp_result_crc(out+2,size-4u));return size;
}

static void rq_consume(RspQueryParser *s) {s->used--;memmove(s->bytes,s->bytes+1,s->used);}
static void rq_drain(RspQueryParser *s,RspQueryRequest callback,void *context) {
    while(s->used) {
        if(s->bytes[0]!=0x52u) {rq_consume(s);continue;}
        if(s->used<2u)return;
        if(s->bytes[1]!=0x53u) {rq_consume(s);continue;}
        if(s->used<14u)return;
        if(!rq_header(s->bytes)) {s->errors++;rq_consume(s);continue;}
        size_t size=16u+rq_get16(s->bytes+12);
        if(s->used<size)return;
        if(rq_request(s->bytes,size)) {
            if(callback)callback(s->bytes,size,context);
            s->used=0u;
        } else {s->errors++;rq_consume(s);}
    }
}
void rsp_query_parser_init(RspQueryParser *s) {if(s)memset(s,0,sizeof(*s));}
void rsp_query_parser_poll(RspQueryParser *s,uint32_t now,RspQueryRequest callback,void *context) {
    if(!s)return;
    if(s->used && now-s->last_ms>200u) {
        s->errors++;
        while(s->used) {rq_consume(s);rq_drain(s,callback,context);}
    }
}
void rsp_query_parser_feed(RspQueryParser *s,const uint8_t *p,size_t length,uint32_t now,RspQueryRequest callback,void *context) {
    if(!s || (length && !p))return;
    rsp_query_parser_poll(s,now,callback,context);
    for(size_t i=0;i<length;i++) {
        if(s->used>=RSP_QUERY_MAX_REQUEST) {s->errors++;rq_consume(s);}
        s->bytes[s->used++]=p[i];s->last_ms=now;rq_drain(s,callback,context);
    }
}
#endif
