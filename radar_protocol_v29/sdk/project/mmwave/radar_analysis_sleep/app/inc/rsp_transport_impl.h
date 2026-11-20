#ifndef RSP_TRANSPORT_IMPL_H
#define RSP_TRANSPORT_IMPL_H
#include "rsp_transport.h"
#include <string.h>
static void rt_flush(RspTransport *s) {
    if(!s || !s->mode || s->faulted || !s->tx_size)return;
    size_t remaining=(size_t)(s->tx_size-s->tx_offset);
    int n=s->write(s->tx+s->tx_offset,remaining,s->context);
    if(n<0 || (size_t)n>remaining) {s->write_errors++;s->faulted=1u;return;}
    s->tx_offset=(uint16_t)(s->tx_offset+(unsigned)n);
    if(s->tx_offset==s->tx_size)s->tx_offset=s->tx_size=0u;
}
static void rt_request(const uint8_t *p,size_t n,void *context) {
    RspTransport *s=(RspTransport *)context;
    /* One pending reply. Read-only query retries recover overload. Never use
     * this drop policy for future writes without transaction semantics. */
    if(s->tx_size || s->faulted) {s->dropped++;return;}
    s->tx_size=(uint16_t)rsp_query_handle(&s->provider,p,n,s->tx,sizeof(s->tx));
    s->tx_offset=0u;rt_flush(s);
}
int rsp_transport_init(RspTransport *s,const RspQueryProvider *p,RspTransportWrite w,void *context,unsigned mode) {
    if(!s)return -1;
    memset(s,0,sizeof(*s));
    if(!w || (mode!=RSP_PORT_BINARY_DEDICATED && mode!=RSP_PORT_HEX_DIAGNOSTIC))return -1;
    if(p)s->provider=*p;
    s->write=w;s->context=context;s->mode=(uint8_t)mode;return 0;
}
void rsp_transport_poll(RspTransport *s,uint32_t now) {
    if(!s || !s->mode || s->faulted)return;
    rt_flush(s);rsp_query_parser_poll(&s->parser,now,rt_request,s);
}
void rsp_transport_feed(RspTransport *s,const uint8_t *p,size_t n,uint32_t now) {
    if(!s || !s->mode || s->faulted || (n && !p))return;
    rt_flush(s);rsp_query_parser_feed(&s->parser,p,n,now,rt_request,s);
}
#endif
