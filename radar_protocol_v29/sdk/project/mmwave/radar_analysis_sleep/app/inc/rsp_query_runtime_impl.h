#ifndef RSP_QUERY_RUNTIME_IMPL_H
#define RSP_QUERY_RUNTIME_IMPL_H
#include "rsp_query_impl.h"
#include "rsp_transport_impl.h"
static RspSession s_rsp_session;
static uint8_t s_rsp_initialization,s_rsp_init_stage;
void rsp_query_runtime_init_state(uint8_t initialization,uint8_t stage) {
    taskENTER_CRITICAL();s_rsp_initialization=initialization;s_rsp_init_stage=stage;taskEXIT_CRITICAL();
}
static int rq_runtime_result(uint8_t *out,size_t capacity,void *context) {
    (void)context;return rsp_result_runtime_frame(out,capacity);
}
static uint64_t rq_runtime_uptime(void *context) {(void)context;return hr_clock_uptime_ms();}
static int rq_runtime_health(uint8_t *out,size_t capacity,void *context) {
    uint8_t frame[RSP_KEEPALIVE_FRAME];(void)context;
    if(!out || capacity<RSP_HEALTH_BODY)return -1;
    if(rsp_keepalive_runtime_frame(frame,sizeof(frame))!=(int)sizeof(frame))return -1;
    memcpy(out,frame+14,16u);
    taskENTER_CRITICAL();out[16]=s_rsp_initialization;out[17]=s_rsp_init_stage;taskEXIT_CRITICAL();
    out[18]=out[19]=0u;return RSP_HEALTH_BODY;
}
static int rq_nibble(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
static int rq_hex_write(const uint8_t *data,size_t n,void *context) {
    (void)context;
    char line[2u*RSP_QUERY_MAX_RESPONSE+32u];
    static const char digits[]="0123456789abcdef";
    const char prefix[]="RSP_REPLY V=1.1 HEX=";size_t used=sizeof(prefix)-1u;
    if(n>RSP_QUERY_MAX_RESPONSE)return -1;
    memcpy(line,prefix,used);
    for(size_t i=0;i<n;i++) {line[used++]=digits[data[i]>>4];line[used++]=digits[data[i]&15u];}
    line[used++]='\r';line[used++]='\n';
    return sleep_trace_record(line,used)==(int)used?(int)n:0;
}
static RspTransport s_rsp_pipe;
int rsp_query_runtime_pipe(const char *hex) {
    uint8_t bytes[RSP_QUERY_MAX_REQUEST];size_t length=0u;
    if(!hex)return -1;
    if(strcmp(hex,"reset")==0) {memset(&s_rsp_pipe,0,sizeof(s_rsp_pipe));return 0;}
    if(strcmp(hex,"poll")==0) {
        if(!s_rsp_pipe.mode)return 0;
        rsp_transport_poll(&s_rsp_pipe,hr_clock_now_ms());return s_rsp_pipe.faulted?-1:0;
    }
    while(length<=2u*RSP_QUERY_MAX_REQUEST && hex[length])length++;
    if(!length || length>2u*RSP_QUERY_MAX_REQUEST || (length&1u))return -1;
    for(size_t i=0;i<length/2u;i++) {
        int a=rq_nibble(hex[2u*i]),b=rq_nibble(hex[2u*i+1u]);
        if(a<0 || b<0)return -1;
        bytes[i]=(uint8_t)((a<<4)|b);
    }
    if(!s_rsp_pipe.mode) {
        const RspQueryProvider provider={rq_runtime_result,rq_runtime_uptime,HEART_FAST_MARKER,NULL,&s_rsp_session,rq_runtime_health};
        if(rsp_transport_init(&s_rsp_pipe,&provider,rq_hex_write,NULL,RSP_PORT_HEX_DIAGNOSTIC))return -1;
    }
    /* Single Shell task, diagnostic stream only; no UART RX ISR installed. */
    rsp_transport_feed(&s_rsp_pipe,bytes,length/2u,hr_clock_now_ms());
    return s_rsp_pipe.faulted?-1:0;
}
int rsp_query_runtime_hex(const char *hex) {
    uint8_t request[RSP_QUERY_MAX_REQUEST],response[RSP_QUERY_MAX_RESPONSE];
    size_t length=0u;
    if(!hex)return -1;
    while(length<=2u*RSP_QUERY_MAX_REQUEST && hex[length])length++;
    if(length<32u || length>2u*RSP_QUERY_MAX_REQUEST || (length&1u))return -1;
    for(size_t i=0;i<length/2u;i++) {
        int a=rq_nibble(hex[i*2u]),b=rq_nibble(hex[i*2u+1u]);
        if(a<0 || b<0)return -1;
        request[i]=(uint8_t)((a<<4)|b);
    }
    const RspQueryProvider provider={rq_runtime_result,rq_runtime_uptime,HEART_FAST_MARKER,NULL,&s_rsp_session,rq_runtime_health};
    size_t n=rsp_query_handle(&provider,request,length/2u,response,sizeof(response));
    if(!n)return -1;
    return rq_hex_write(response,n,NULL)==(int)n?0:-1;
}
#endif
