#ifndef RSP_RESULT_RUNTIME_IMPL_H
#define RSP_RESULT_RUNTIME_IMPL_H
#include "rsp_result.h"
#include "rsp_result_codec_impl.h"
#include "rsp_keepalive_impl.h"
#include "heart_rate_device.h"
#include "hr_migration_clock.h"
static RspResult s_rsp_snapshot;
static uint64_t s_rsp_bed_since;
static uint32_t s_rsp_snapshot_ms,s_rsp_seq,s_rsp_stream_at,s_rsp_keep_seq;
static uint8_t s_rsp_bed;
static bool s_rsp_have,s_rsp_stream;
void rsp_result_runtime_stream(bool enabled) {taskENTER_CRITICAL();s_rsp_stream=enabled;taskEXIT_CRITICAL();}
void rsp_result_runtime_update(void) {
    const SleepDetectResult *sleep=sleep_detect_get_result();
    HeartRateDisplay heart;heart_rate_device_get_display(&heart);
    RspResult next;memset(&next,0,sizeof(next));
    uint32_t now=hr_clock_now_ms();uint64_t uptime=hr_clock_uptime_ms();
    next.uptime_ms=uptime;next.generation=heart.generation;
    next.presence=sleep->has_person?2u:1u;next.bed=sleep->in_bed?2u:1u;next.motion=sleep->is_active?2u:1u;
    next.sleep_state=(uint8_t)(sleep->sleep_state+1u);
    next.breath_x10=UINT16_MAX;next.breath_age_ms=UINT32_MAX;next.flags=RSP_RESULT_HEX_DIAGNOSTIC;
    /* Existing BR lacks an independent confirmation timestamp. Report this
     * honestly instead of renewing an invented age on repeated values. */
    if(sleep->in_bed && sleep->breath_rate) {
        next.breath_x10=(uint16_t)(sleep->breath_rate*10u);
        next.breath_state=sleep->breath_valid?3u:2u;next.flags|=RSP_RESULT_BR_TIME_UNKNOWN;
    }
    next.heart_x10=UINT16_MAX;next.heart_age_ms=UINT32_MAX;
    if(sleep->in_bed && heart.bpm && heart.state) {
        next.heart_x10=(uint16_t)(heart.bpm*10u);next.heart_age_ms=heart.age_ms;
        next.heart_state=sleep->is_active?2u:heart.state;
        next.heart_kind=heart.kind;next.heart_quality=heart.quality;next.heart_sample_seq=heart.sequence;
        if(heart.harmonic)next.flags|=RSP_RESULT_HCON;
    }
    if(sleep->is_active)next.flags|=RSP_RESULT_MOTION;
    if(!hr_clock_ready()) {next.flags|=RSP_RESULT_CLOCK_FAULT;next.faults=1u;}
    taskENTER_CRITICAL();
    if(!s_rsp_have || s_rsp_bed!=next.bed) {s_rsp_bed=next.bed;s_rsp_bed_since=uptime;}
    uint32_t duration=(uint32_t)((uptime-s_rsp_bed_since)/1000u);
    next.in_bed_s=next.bed==2u?duration:0u;next.out_bed_s=next.bed==1u?duration:0u;
    s_rsp_snapshot=next;s_rsp_snapshot_ms=now;s_rsp_have=true;
    taskEXIT_CRITICAL();
}
int rsp_keepalive_runtime_frame(uint8_t *out,size_t capacity) {
    uint32_t generation,seq;uint64_t previous;bool have;
    uint64_t uptime=hr_clock_uptime_ms();
    taskENTER_CRITICAL();previous=s_rsp_snapshot.uptime_ms;generation=s_rsp_snapshot.generation;have=s_rsp_have;
    seq=++s_rsp_keep_seq;if(!seq)seq=++s_rsp_keep_seq;taskEXIT_CRITICAL();
    uint64_t elapsed=uptime>=previous?uptime-previous:UINT64_MAX;
    uint32_t age=have?(elapsed>=UINT32_MAX?UINT32_MAX-1u:(uint32_t)elapsed):UINT32_MAX;
    uint32_t flags=have?RSP_KEEP_HAVE:0u;
    HeartRateDisplay live;heart_rate_device_get_display(&live);
    bool same=live.generation==generation;
    if(!have || age>5000u || !same || !hr_clock_ready())flags|=RSP_KEEP_STALE;
    if(!hr_clock_ready()) {flags|=RSP_KEEP_CLOCK_FAULT;age=UINT32_MAX;}
    return (int)rsp_keepalive_encode(uptime,age,flags,seq,out,capacity);
}
void rsp_result_runtime_poll(uint32_t scheduler_ms) {
    taskENTER_CRITICAL();bool emit=s_rsp_stream && scheduler_ms-s_rsp_stream_at>=2000u;
    bool have=s_rsp_have;if(emit)s_rsp_stream_at=scheduler_ms;taskEXIT_CRITICAL();
    if(!emit)return;
    uint8_t bytes[RSP_KEEPALIVE_FRAME];char line[96];
    int n=rsp_keepalive_runtime_frame(bytes,sizeof(bytes));
    const char prefix[]="RSP_KEEP V=1.1 HEX=";size_t used=sizeof(prefix)-1u;
    static const char digits[]="0123456789abcdef";memcpy(line,prefix,used);
    if(n>0) {
        for(int i=0;i<n;i++) {line[used++]=digits[bytes[i]>>4];line[used++]=digits[bytes[i]&15u];}
        line[used++]='\r';line[used++]='\n';sleep_trace_record(line,used);
    }
    if(have)rsp_result_runtime_dump();
}
int rsp_result_runtime_frame(uint8_t *out,size_t capacity) {
    RspResult value;uint32_t previous,seq;bool have;
    HeartRateDisplay live;
    heart_rate_device_get_display(&live);
    uint32_t now=hr_clock_now_ms();uint64_t uptime=hr_clock_uptime_ms();
    taskENTER_CRITICAL();value=s_rsp_snapshot;previous=s_rsp_snapshot_ms;have=s_rsp_have;
    seq=++s_rsp_seq;if(!seq)seq=++s_rsp_seq;taskEXIT_CRITICAL();
    if(!have)return -1;
    uint32_t elapsed=now-previous;value.uptime_ms=uptime;
    if(elapsed>5000u || !hr_clock_ready() || live.generation!=value.generation) {
        value.generation=live.generation;
        value.presence=value.bed=value.motion=value.sleep_state=0u;
        value.breath_x10=value.heart_x10=UINT16_MAX;value.breath_state=value.heart_state=value.heart_kind=value.heart_quality=0u;
        value.breath_age_ms=value.heart_age_ms=value.in_bed_s=value.out_bed_s=UINT32_MAX;
        value.breath_sample_seq=value.heart_sample_seq=0u;value.flags|=RSP_RESULT_STALE;
        if(!hr_clock_ready()) {value.flags|=RSP_RESULT_CLOCK_FAULT;value.faults|=1u;}
    } else if(value.heart_state) {
        uint64_t age=(uint64_t)value.heart_age_ms+elapsed;
        value.heart_age_ms=age>=UINT32_MAX?UINT32_MAX-1u:(uint32_t)age;
        if(value.heart_age_ms>10000u)value.heart_state=2u;
    }
    return (int)rsp_result_encode(&value,seq,out,capacity);
}
int rsp_result_runtime_dump(void) {
    uint8_t bytes[RSP_RESULT_FRAME];char line[2u*RSP_RESULT_FRAME+32u];
    static const char digits[]="0123456789abcdef";
    int length=rsp_result_runtime_frame(bytes,sizeof(bytes));
    if(length<=0) {sleep_trace_printf("RSP_RESULT RET=NO_DATA\r\n");return -1;}
    const char prefix[]="RSP_FRAME V=1.1 HEX=";size_t used=sizeof(prefix)-1u;
    memcpy(line,prefix,used);
    for(int i=0;i<length;i++) {line[used++]=digits[bytes[i]>>4];line[used++]=digits[bytes[i]&15u];}
    line[used++]='\r';line[used++]='\n';
    return sleep_trace_record(line,used)<0?-1:0;
}
#endif
