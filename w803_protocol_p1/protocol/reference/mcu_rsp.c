#include "mcu_rsp.h"
#include <string.h>
static uint16_t r16(const uint8_t *p) {return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t r32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t r64(const uint8_t *p) {return r32(p)|((uint64_t)r32(p+4)<<32);}
static void w16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,uint32_t v) {unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8u));}
static void w64(uint8_t *p,uint64_t v) {w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32));}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint32_t c=0xffffu;size_t i;unsigned j;
    for(i=0;i<n;i++) {c^=(uint32_t)p[i]<<8;for(j=0;j<8;j++)c=(c&0x8000u)?((c<<1)^0x1021u)&0xffffu:(c<<1)&0xffffu;}
    return (uint16_t)c;
}
size_t mcu_rsp_size(void) {return sizeof(McuRsp);}
void mcu_rsp_init(McuRsp *c,McuRspToken token,void *context) {memset(c,0,sizeof(*c));c->fresh_token=token;c->context=context;}
void mcu_rsp_disconnect(McuRsp *c,uint32_t now) {
    c->phase=MCU_RSP_BACKOFF;c->pending=0;c->result_valid=0;c->rx_used=0;c->boot_id=0;c->token=0;c->next_poll=now+1000u;c->reconnects++;
}
static size_t begin(McuRsp *c,uint16_t id,const uint8_t *extra,size_t n,int scoped,uint32_t now,uint8_t *out,size_t capacity) {
    size_t payload=n+(scoped?8u:0u),size=payload+16u;uint8_t *p=c->tx;
    if(c->pending || size>MCU_RSP_MAX || capacity<size || (n && !extra))return 0;
    if(c->sequence==UINT32_MAX) {mcu_rsp_disconnect(c,now);return 0;}
    c->sequence++;p[0]=0x52;p[1]=0x53;p[2]=p[3]=p[4]=1;p[5]=0;w16(p+6,id);w32(p+8,c->sequence);w16(p+12,(uint16_t)payload);
    if(scoped)w64(p+14,c->token);
    if(n)memcpy(p+14+(scoped?8u:0u),extra,n);
    w16(p+size-2u,crc(p+2,size-4u));
    c->pending_id=id;c->pending_sequence=c->sequence;c->pending=1;c->attempts=1;
    c->first_sent=c->last_sent=now;c->tx_size=(uint16_t)size;memcpy(out,p,size);return size;
}
size_t mcu_rsp_begin(McuRsp *c,uint16_t id,const uint8_t *data,size_t n,uint32_t now,uint8_t *out,size_t cap) {
    size_t size;
    if(c->phase!=MCU_RSP_READY || id<0x0111u)return 0;
    size=begin(c,id,data,n,1,now,out,cap);
    if(size && id==0x0121u)c->result_valid=0;
    return size;
}
size_t mcu_rsp_poll(McuRsp *c,uint32_t now,uint8_t *out,size_t capacity) {
    uint8_t boot[8];
    if(c->pending) {
        if(now-c->first_sent>=2400u) {c->last_error=258;mcu_rsp_disconnect(c,now);return 0;}
        if(now-c->last_sent>=800u && c->attempts<3u && capacity>=c->tx_size) {
            c->attempts++;c->last_sent=now;memcpy(out,c->tx,c->tx_size);return c->tx_size;
        }
        return 0;
    }
    if(c->phase==MCU_RSP_BACKOFF) {if((int32_t)(now-c->next_poll)<0)return 0;c->phase=MCU_RSP_IDENTITY;c->sequence=0;}
    if(c->phase==MCU_RSP_BLOCKED)return 0;
    if(c->phase==MCU_RSP_IDENTITY)return begin(c,0x0102,0,0,0,now,out,capacity);
    if(c->phase==MCU_RSP_CAPS)return begin(c,0x0100,0,0,0,now,out,capacity);
    if(c->phase==MCU_RSP_OPEN) {
        if(!c->token) {
            uint64_t token=c->fresh_token?c->fresh_token(c->context):0;
            if(!token || token==c->previous_token) {c->last_error=257;c->phase=MCU_RSP_BLOCKED;return 0;}
            c->token=c->previous_token=token;
        }
        w64(boot,c->boot_id);return begin(c,0x0110,boot,8,1,now,out,capacity);
    }
    if(c->phase==MCU_RSP_READY && (int32_t)(now-c->next_poll)>=0)return begin(c,0x0123,0,0,1,now,out,capacity);
    return 0;
}
static void received(McuRsp *c,const uint8_t *p,size_t n,uint32_t now) {
    unsigned kind=p[4],id=r16(p+6);uint16_t code;const uint8_t *b=p+14;size_t length=n-16u;
    if(kind==4u && id==0x8200u && length==36u) {
        if(c->boot_id && r64(b)!=c->boot_id) {c->last_error=12;c->token=0;mcu_rsp_disconnect(c,now);}
        return;
    }
    if(kind!=2u || !c->pending || id!=c->pending_id || r32(p+8)!=c->pending_sequence)return;
    if(now-c->first_sent>=2400u) {c->last_error=258;mcu_rsp_disconnect(c,now);return;}
    if(length<(id>=0x0110u?12u:4u))return;
    if(id>=0x0110u && r64(b+4)!=c->token)return;
    code=r16(b);c->pending=0;c->last_error=code;
    if(code) {
        if(code==9u || code==12u) {c->token=0;mcu_rsp_disconnect(c,now);}
        else if(c->phase!=MCU_RSP_READY)mcu_rsp_disconnect(c,now);
        c->reply_ready=1;c->last_reply_id=(uint16_t)id;c->reply_size=0;return;
    }
    if(c->phase==MCU_RSP_IDENTITY) {
        if(length!=32u || !r64(b+4)) {c->phase=MCU_RSP_BLOCKED;c->last_error=256;return;}
        c->boot_id=r64(b+4);c->token=0;c->phase=MCU_RSP_CAPS;
    } else if(c->phase==MCU_RSP_CAPS) {
        if(length!=68u || (r32(b+8)&0x1183u)!=0x1183u) {c->phase=MCU_RSP_BLOCKED;c->last_error=256;return;}
        c->phase=MCU_RSP_OPEN;
    } else if(c->phase==MCU_RSP_OPEN) {
        if(length!=36u || r64(b+28)!=c->boot_id) {c->last_error=12;c->token=0;mcu_rsp_disconnect(c,now);return;}
        c->phase=MCU_RSP_READY;c->next_poll=now;
    } else {
        size_t body=length-12u;
        memcpy(c->reply,b+12,body);c->reply_size=(uint16_t)body;c->reply_ready=1;c->last_reply_id=(uint16_t)id;
        if(id==0x0123u) {
            uint32_t elapsed=now-c->first_sent;unsigned offsets[2]={40,44},i;
            if(body!=72u || r64(b+12)!=c->boot_id) {c->last_error=12;c->token=0;mcu_rsp_disconnect(c,now);return;}
            memcpy(c->result,b+12,72);c->last_result_ms=now;c->result_valid=1;c->next_poll=now+2000u;
            for(i=0;i<2;i++) {
                uint32_t age=r32(c->result+offsets[i]);
                if(age!=UINT32_MAX) {uint64_t aged=(uint64_t)age+elapsed;w32(c->result+offsets[i],aged>=UINT32_MAX?UINT32_MAX-1u:(uint32_t)aged);}
            }
        } else if((id==0x0120u || id==0x0121u || id==0x0122u) && body==32u) {
            if(r64(b+12)!=c->boot_id || !b[34] || b[32]==3u)c->result_valid=0;
        }
    }
}
static void consume(McuRsp *c) {c->rx_used--;memmove(c->rx,c->rx+1,c->rx_used);}
static void drain(McuRsp *c,uint32_t now) {
    while(c->rx_used) {
        size_t size;
        if(c->rx[0]!=0x52 || (c->rx_used>=2 && c->rx[1]!=0x53)) {consume(c);continue;}
        if(c->rx_used<14)return;
        size=16u+r16(c->rx+12);
        if(c->rx[2]!=1 || c->rx[3]!=1 || c->rx[5] || !r32(c->rx+8) || size>MCU_RSP_MAX) {c->errors++;consume(c);continue;}
        if(c->rx_used<size)return;
        if(r16(c->rx+size-2u)!=crc(c->rx+2,size-4u)) {c->errors++;consume(c);continue;}
        received(c,c->rx,size,now);
        if(c->rx_used<size)return; /* disconnect may have cleared the parser */
        c->rx_used=(uint16_t)(c->rx_used-size);memmove(c->rx,c->rx+size,c->rx_used);
    }
}
void mcu_rsp_feed(McuRsp *c,const uint8_t *p,size_t n,uint32_t now) {
    size_t i;if(n && !p)return;
    if(c->rx_used && now-c->rx_ms>200u) {c->errors++;while(c->rx_used) {consume(c);drain(c,now);}}
    for(i=0;i<n;i++) {
        if(c->rx_used>=MCU_RSP_MAX) {c->errors++;consume(c);}
        c->rx[c->rx_used++]=p[i];c->rx_ms=now;drain(c,now);
    }
}
int mcu_rsp_result(const McuRsp *c,uint32_t now,uint8_t *out,size_t capacity) {
    unsigned offsets[2]={40,44},i;uint32_t elapsed=now-c->last_result_ms;
    if(capacity<72u || !c->result_valid || c->phase!=MCU_RSP_READY || elapsed>=6000u ||
       (c->pending && now-c->first_sent>=2400u))return 0;
    memcpy(out,c->result,72);
    for(i=0;i<2u;i++) {
        uint32_t age=r32(out+offsets[i]);
        if(age!=UINT32_MAX) {uint64_t aged=(uint64_t)age+elapsed;w32(out+offsets[i],aged>=UINT32_MAX?UINT32_MAX-1u:(uint32_t)aged);}
    }
    if(out[36] && r32(out+40)>10000u)out[36]=2;
    if(out[37] && r32(out+44)>10000u)out[37]=2;
    return 72;
}
int mcu_rsp_reply(McuRsp *c,uint16_t *id,uint16_t *error,uint8_t *out,size_t capacity) {
    if(!c->reply_ready || capacity<c->reply_size)return -1;
    *id=c->last_reply_id;*error=c->last_error;if(c->reply_size)memcpy(out,c->reply,c->reply_size);
    c->reply_ready=0;return c->reply_size;
}
