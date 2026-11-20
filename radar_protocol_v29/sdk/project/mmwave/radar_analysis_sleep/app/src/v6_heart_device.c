/* V28 acquisition adapter. Keep the existing callback/shell API and HRIN V27
 * wire schema; W803 owns all heart estimation. No queue, task or math workspace.
 * Sampling, IQ selection and the SLEEP breath context remain in the V27 caller. */
#include "common.h"
#include "v6_heart_device.h"
#include "hr_migration_clock.h"
#include <stdio.h>
#include "sleep_async_log.h"
#define printf sleep_trace_printf

static uint32_t generation=1u,samples,last_emit_ms;
static uint8_t previous_breath;
static bool initialized,running;

void v6_heart_reset(void) {
    taskENTER_CRITICAL();
    generation++;previous_breath=0u;
    taskEXIT_CRITICAL();
}
int v6_heart_init(void) {
    if(initialized)return 0;
    if(hr_clock_init()!=0)return -4;
    running=false;v6_heart_reset();initialized=true;
    return 0;
}
void v6_heart_start(void) {
    v6_heart_reset();
    taskENTER_CRITICAL();running=initialized && hr_clock_ready();taskEXIT_CRITICAL();
}
void v6_heart_stop(void) {
    taskENTER_CRITICAL();running=false;taskEXIT_CRITICAL();
    v6_heart_reset();
}
void v6_heart_observe_status(bool bed,uint8_t breath) {
    /* Match the positive BR hold/reset ordering of the V27 SLEEP callback. */
    if(!bed)v6_heart_reset();
    else if(breath>0u) {
        taskENTER_CRITICAL();previous_breath=breath;taskEXIT_CRITICAL();
    }
}
void v6_heart_submit(uint32_t frame,uint16_t bin,bool active,const int16_t iq[6]) {
    uint32_t source_ms,n,source_generation;uint8_t breath;
    if(iq==NULL || !hr_clock_ready())return;
    source_ms=hr_clock_now_ms();
    taskENTER_CRITICAL();
    if(!running) {taskEXIT_CRITICAL();return;}
    n=++samples;source_generation=generation;breath=previous_breath;
    taskEXIT_CRITICAL();
    /* V is the existing data schema, not this firmware's V28 identity.
     * ENQ=0 truthfully means there is no local radar heart-compute queue. */
    printf("HRIN V=27 N=%lu GEN=%lu F=%lu MS=%lu BIN=%u FLAGS=%u BR=%u I0=%d Q0=%d I1=%d Q1=%d I2=%d Q2=%d ENQ=0\r\n",
        (unsigned long)n,(unsigned long)source_generation,(unsigned long)frame,(unsigned long)source_ms,
        (unsigned)bin,(unsigned)(3u|(active?4u:0u)),(unsigned)breath,
        (int)iq[0],(int)iq[1],(int)iq[2],(int)iq[3],(int)iq[4],(int)iq[5]);
}
void v6_heart_emit(void) {
    uint32_t now=hr_clock_now_ms(),source_generation,count;bool active;SleepTraceStats tx;
    if(now-last_emit_ms<5000u)return;
    last_emit_ms=now;
    taskENTER_CRITICAL();source_generation=generation;count=samples;active=running;taskEXIT_CRITICAL();
    sleep_trace_get(&tx);
    printf("HRSTREAM V=1 GEN=%lu MS=%lu IN=%lu ACTIVE=%u CLOCK_OK=%u HEART_COMPUTE=0 OWNER=W803 LOCAL_QUEUE=0 HEAP=%lu MINHEAP=%lu TXDROP=%lu FORMAT_DROP=%lu\r\n",
        (unsigned long)source_generation,(unsigned long)now,(unsigned long)count,(unsigned)active,
        (unsigned)hr_clock_ready(),(unsigned long)xPortGetFreeHeapSize(),
        (unsigned long)xPortGetMinimumEverFreeHeapSize(),(unsigned long)tx.refused,(unsigned long)tx.format_refused);
}
