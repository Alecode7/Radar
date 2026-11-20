#include "common.h"
#include "heart_rate_core.h"
#include "heart_rate_device.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "sleep_async_log.h"
#include "hr_migration_clock.h"
/* Port repairs: hardware time and bounded missing-input hold; estimator math is unchanged. */
#define printf sleep_trace_printf

#if CONFIG_SLEEP_HEART_DEVICE
#define HR_QUEUE_DEPTH 32u
#define HR_STACK_BYTES 6144u
#define HR_HEAP_RESERVE 4096u
#define HR_ESTIMATE_BUDGET_MS 4000u
#ifndef HR_ALIGNMENT_TRACE
#define HR_ALIGNMENT_TRACE 1
#endif
typedef struct { HrSample sample;uint32_t generation;uint8_t breath; } Input;
typedef struct {
    uint32_t generation,start_ms,phase_at_ms,phase_ms[HR_PHASE_COUNT],yield_ms;
    unsigned phase;bool over_budget,spectrum_seen;
} EstimateContext;
static HrCore core;
static QueueHandle_t input_queue;
static TaskHandle_t worker_handle;
static volatile uint32_t generation=1u;
static HeartRateDeviceResult published;
static HeartRateDeviceDiagnostics diagnostics;
static uint32_t worker_generation,serial_number,last_emitted,drop_count;
static uint32_t last_emit_ms;
static uint8_t last_emitted_state=255u;
static bool producer_was_in_bed;
static volatile bool acquisition_active;
static HrHomeCache home_cache;
static HrHomeCandidate home_candidate;

#if HR_ALIGNMENT_TRACE
/* One bounded completion snapshot. Only the producer prints, after HRRAW.
 * A missed snapshot is counted explicitly; it must never look like a full
 * trace. No queue/task/core workspace is added for this diagnostic build. */
typedef struct {
    uint32_t attempt,generation,frame,sample_ms,start_ms,end_ms,sequence,accepted_ms;
    int32_t candidate_milli;
    int16_t core_bpm,core_bin;
    uint16_t reason,public_bpm,public_reason,count,candidates;
    uint8_t status,breath_in,breath_used,quality,support,fresh,conflict,public_state;
} HeartEstimateTrace;
static HeartEstimateTrace estimate_trace;
static uint32_t trace_emitted,trace_missed;
#endif

static uint32_t now_ms(void) {
    return hr_clock_now_ms();
}
static uint32_t copy_published(const HeartRateDeviceResult *value,uint32_t expected_generation) {
    uint32_t sequence=0u;
    taskENTER_CRITICAL();
    if(generation==expected_generation) { published=*value;published.generation=generation;published.sequence=++serial_number;sequence=published.sequence; }
    taskEXIT_CRITICAL();
    return sequence;
}

#if HR_ALIGNMENT_TRACE
static void record_estimate_trace(const Input *input,uint32_t attempt,uint32_t start,uint32_t end,
    uint8_t status,const HrResult *result,const HeartRateDeviceResult *value,uint32_t sequence) {
    HeartEstimateTrace trace;
    memset(&trace,0,sizeof(trace));trace.attempt=attempt;trace.generation=input->generation;
    trace.frame=input->sample.frame;trace.sample_ms=input->sample.ms;trace.start_ms=start;trace.end_ms=end;
    trace.sequence=sequence;trace.status=status;trace.breath_in=input->breath;
    trace.core_bpm=-1;trace.core_bin=-1;trace.candidate_milli=-1;trace.accepted_ms=UINT32_MAX;
    trace.count=core.count;trace.candidates=core.candidate_count;
    if(result!=NULL) {
        trace.core_bpm=result->bpm;trace.core_bin=result->target_bin;trace.reason=result->reason;
        trace.breath_used=result->breath;trace.quality=result->quality;trace.support=result->support;
        trace.fresh=result->fresh;trace.conflict=result->harmonic_conflict;trace.accepted_ms=result->accepted_ms;
        if(result->candidate_bpm>=0.0 && result->candidate_bpm<=1000.0)
            trace.candidate_milli=(int32_t)(result->candidate_bpm*1000.0+0.5);
    }
    if(value!=NULL && sequence!=0u) {
        trace.public_bpm=value->bpm==UINT16_MAX?0u:value->bpm;
        trace.public_state=value->state;trace.public_reason=value->reason;
    }
    taskENTER_CRITICAL();
    if(estimate_trace.attempt!=trace_emitted) trace_missed++;
    estimate_trace=trace;
    taskEXIT_CRITICAL();
}
static void emit_estimate_trace(void) {
    HeartEstimateTrace trace;uint32_t missed;
    taskENTER_CRITICAL();trace=estimate_trace;missed=trace_missed;
    if(trace.attempt==trace_emitted) { taskEXIT_CRITICAL();return; }
    trace_emitted=trace.attempt;taskEXIT_CRITICAL();
    printf("HREST V=1 EST=%lu GEN=%lu F=%lu MS=%lu START=%lu END=%lu STATUS=%u SEQ=%lu BRIN=%u BRUSE=%u CORE=%d CAND=%ld REASON=%u Q=%u ANT=%u BIN=%d FRESH=%u HCON=%u CONF=%lu COUNT=%u HCNT=%u PUB=%u PSTATE=%u PREASON=%u LOST=%lu\r\n",
        (unsigned long)trace.attempt,(unsigned long)trace.generation,(unsigned long)trace.frame,
        (unsigned long)trace.sample_ms,(unsigned long)trace.start_ms,(unsigned long)trace.end_ms,
        (unsigned int)trace.status,(unsigned long)trace.sequence,(unsigned int)trace.breath_in,
        (unsigned int)trace.breath_used,(int)trace.core_bpm,(long)trace.candidate_milli,
        (unsigned int)trace.reason,(unsigned int)trace.quality,(unsigned int)trace.support,(int)trace.core_bin,
        (unsigned int)trace.fresh,(unsigned int)trace.conflict,(unsigned long)trace.accepted_ms,
        (unsigned int)trace.count,(unsigned int)trace.candidates,(unsigned int)trace.public_bpm,
        (unsigned int)trace.public_state,(unsigned int)trace.public_reason,(unsigned long)missed);
}
#endif
static void publish_empty(uint8_t state,uint16_t reason,uint32_t ms,uint32_t expected_generation) {
    HeartRateDeviceResult value;
    taskENTER_CRITICAL();value=published;taskEXIT_CRITICAL();
    if(value.state==state && value.reason==reason && ms-value.computed_ms<5000u) return;
    value.state=state;value.reason=reason;value.bpm=UINT16_MAX;value.raw_bpm=0;
    value.quality=0;value.support=0;value.harmonic_conflict=0;value.bin=UINT16_MAX;
    value.have_confirm=0;value.age_ms=UINT32_MAX;value.computed_ms=ms;value.dropped=drop_count;
    copy_published(&value,expected_generation);
}
void heart_rate_device_reset(void) {
    HeartRateDeviceResult value;
    memset(&value,0,sizeof(value));value.bpm=UINT16_MAX;value.age_ms=UINT32_MAX;
    value.reason=HR_RESET;value.state=input_queue!=NULL?1u:0u;value.computed_ms=now_ms();
    taskENTER_CRITICAL();generation++;value.generation=generation;value.dropped=drop_count;published=value;
    hr_home_clear_cache(&home_cache,generation);hr_home_clear_candidate(&home_candidate);
    published.sequence=++serial_number;producer_was_in_bed=false;
    /* All queued samples predate this discontinuity. Never leave a full old
     * queue behind while repeatedly resetting only the published result. */
    if(input_queue!=NULL) xQueueReset(input_queue);
    taskEXIT_CRITICAL();
}
void heart_rate_device_get(HeartRateDeviceResult *value) {
    uint32_t now=now_ms();
    if(value==NULL) return;
    taskENTER_CRITICAL();*value=published;
    value->cost_ms=diagnostics.last_cost_ms;value->max_cost_ms=diagnostics.max_cost_ms;
    value->stack_free_bytes=diagnostics.stack_free_bytes;value->dropped=drop_count;
    taskEXIT_CRITICAL();
    value->report_ms=now;
    value->age_ms=value->have_confirm?now-value->last_confirm_ms:UINT32_MAX;
    if(!hr_clock_ready()) {
        value->state=0u;value->reason=HR_CLOCK_FAULT_REASON;value->bpm=UINT16_MAX;value->raw_bpm=0u;
        value->have_confirm=0u;value->age_ms=UINT32_MAX;value->quality=0u;value->support=0u;
        value->bin=UINT16_MAX;value->harmonic_conflict=0u;return;
    }
    if((value->state==3u || value->state==4u) && value->age_ms>10000u) {
        value->state=5u;value->bpm=UINT16_MAX;value->reason=HR_STALE;
    } else if(value->state==3u && now-value->computed_ms>5000u) {
        /* A 5s estimate cadence plus computation time normally crosses this
         * point before the next result is published. Keep the ORIGINAL
         * confirmation/age until its existing 10s TTL; label it held rather
         * than inventing either a new measurement or a temporary loss. A
         * newly published invalid estimate still replaces it immediately. */
        value->state=4u;value->reason=HR_WAIT_UPDATE;
    }
}
void heart_rate_device_get_diagnostics(HeartRateDeviceDiagnostics *value) {
    if(value==NULL) return;
    taskENTER_CRITICAL();*value=diagnostics;taskEXIT_CRITICAL();
}
void heart_rate_device_get_display(HeartRateDisplay *value) {
    uint32_t now=now_ms();
    if(value==NULL)return;
    taskENTER_CRITICAL();
    if(!hr_clock_ready())hr_home_clear_cache(&home_cache,generation);
    hr_home_get(&home_cache,now,value);
    taskEXIT_CRITICAL();
}
void heart_rate_device_start(void) {
    heart_rate_device_reset();
    taskENTER_CRITICAL();acquisition_active=true;taskEXIT_CRITICAL();
    if(worker_handle!=NULL) vTaskResume(worker_handle);
}
void heart_rate_device_stop(void) {
    taskENTER_CRITICAL();acquisition_active=false;taskEXIT_CRITICAL();
    if(worker_handle!=NULL) vTaskSuspend(worker_handle);
    if(input_queue!=NULL) xQueueReset(input_queue);
    taskENTER_CRITICAL();diagnostics.busy=0;diagnostics.phase=HR_PHASE_IDLE;taskEXIT_CRITICAL();
    heart_rate_device_reset();
}
static int cooperative_yield(void *context,unsigned phase) {
    EstimateContext *estimate=(EstimateContext *)context;uint32_t now=now_ms(),wait_start;
    if(phase>=HR_PHASE_COUNT) return 1;
    estimate->phase_ms[estimate->phase]+=now-estimate->phase_at_ms;estimate->phase_at_ms=now;
    taskENTER_CRITICAL();diagnostics.phase=phase;
    if((phase==HR_PHASE_SPECTRUM || phase>=HR_PHASE_PREPROCESS) && !estimate->spectrum_seen) {
        diagnostics.spectral_estimates++;estimate->spectrum_seen=true;
    }
    estimate->phase=phase;taskEXIT_CRITICAL();
    if(!hr_clock_ready() || !acquisition_active || generation!=estimate->generation) return 1;
    if(now_ms()-estimate->start_ms>=HR_ESTIMATE_BUDGET_MS) { estimate->over_budget=true;return 1; }
    wait_start=now_ms();vTaskDelay(1);estimate->yield_ms+=now_ms()-wait_start;
    if(!hr_clock_ready() || !acquisition_active || generation!=estimate->generation) return 1;
    if(now_ms()-estimate->start_ms>=HR_ESTIMATE_BUDGET_MS) { estimate->over_budget=true;return 1; }
    return 0;
}

static void process_input(const Input *input) {
    HrResult result;HeartRateDeviceResult value;uint32_t start,end,stack_free;
    EstimateContext estimate;int status;
#if HR_ALIGNMENT_TRACE
    uint32_t trace_attempt;
#endif
    if(!acquisition_active) return;
    if(!hr_clock_ready()) {publish_empty(0u,HR_CLOCK_FAULT_REASON,now_ms(),generation);return;}
    taskENTER_CRITICAL();diagnostics.processed++;taskEXIT_CRITICAL();
    if(worker_generation!=generation) { hr_core_init(&core);worker_generation=generation; }
    if(input->generation!=worker_generation) return;
    if(!(input->sample.flags&HR_IN_BED)) { hr_core_init(&core);publish_empty(2u,HR_WAIT_BED,input->sample.ms,input->generation);return; }
    status=hr_core_push(&core,input->sample.ms,input->sample.frame,input->sample.bin,input->sample.flags,5u,input->sample.iq);
    if(status!=0) { publish_empty(2u,status==-2?HR_CAPACITY:HR_INPUT,input->sample.ms,input->generation);return; }
    if(!(input->sample.flags&HR_VALID)) {
        bool keep=false;uint32_t current=now_ms();
        /* Brief missing input is not a new negative HR estimate. Keep only
         * the already-confirmed <=10s result, unchanged confirmation/SEQ.
         * Motion, exit, stale results, reset and clock faults still clear. */
        taskENTER_CRITICAL();
        if(generation==input->generation && !(input->sample.flags&HR_ACTIVE) &&
           hr_clock_ready() && published.have_confirm && published.bpm!=UINT16_MAX &&
           (published.state==3u || published.state==4u) && current-published.last_confirm_ms<=10000u) {
            published.state=4u;published.reason=HR_INPUT;keep=true;
        }
        taskEXIT_CRITICAL();
        if(!keep)publish_empty(2u,HR_INPUT,input->sample.ms,input->generation);
        return;
    }
    if(input->sample.ms-core.last_estimate_ms<5000u) return;
    core.last_estimate_ms=input->sample.ms;
    start=now_ms();
    memset(&estimate,0,sizeof(estimate));estimate.generation=input->generation;
    estimate.start_ms=start;estimate.phase_at_ms=start;estimate.phase=HR_PHASE_CHECK;
    taskENTER_CRITICAL();diagnostics.estimates++;diagnostics.last_start_ms=start;
#if HR_ALIGNMENT_TRACE
    trace_attempt=diagnostics.estimates;
#endif
    diagnostics.busy=1;diagnostics.phase=HR_PHASE_CHECK;taskEXIT_CRITICAL();
    status=hr_core_estimate(&core,input->breath,input->sample.ms,&result,cooperative_yield,&estimate);
    end=now_ms();
    estimate.phase_ms[estimate.phase]+=end-estimate.phase_at_ms;
    stack_free=(uint32_t)uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t);
    /* Capture attempt diagnostics BEFORE rejecting an obsolete result. In V8
     * reset erased the expensive attempt and misleadingly left COST/MAX=0. */
    taskENTER_CRITICAL();diagnostics.last_cost_ms=end-start;
    if(diagnostics.last_cost_ms>diagnostics.max_cost_ms) diagnostics.max_cost_ms=diagnostics.last_cost_ms;
    diagnostics.last_finish_ms=end;diagnostics.last_phase=estimate.phase;
    memcpy(diagnostics.phase_ms,estimate.phase_ms,sizeof(diagnostics.phase_ms));diagnostics.yield_ms=estimate.yield_ms;
    diagnostics.busy=0;diagnostics.phase=HR_PHASE_IDLE;
    diagnostics.stack_free_bytes=stack_free;
    if(!status || worker_generation!=generation) diagnostics.aborted++;
    else diagnostics.completed++;
    if(estimate.over_budget) diagnostics.budget_aborts++;
    taskEXIT_CRITICAL();
    if(worker_generation!=generation) {
#if HR_ALIGNMENT_TRACE
        record_estimate_trace(input,trace_attempt,start,end,3u,NULL,NULL,0u);
#endif
        return; /* reset during the calculation */
    }
    if(!status) {
        publish_empty(hr_clock_ready()?2u:0u,!hr_clock_ready()?HR_CLOCK_FAULT_REASON:
            (estimate.over_budget?HR_BUDGET:HR_RESET),end,input->generation);
#if HR_ALIGNMENT_TRACE
        record_estimate_trace(input,trace_attempt,start,end,2u,NULL,NULL,0u);
#endif
        return;
    }
    /* New display samples only: old values retain their original sequence and
     * age even when this estimate fails, is held, or contains motion. */
    {
        bool usable=result.bpm>=0 && core.have_accepted && end-result.accepted_ms<=10000u &&
                    end-input->sample.ms<=5000u;
        unsigned tentative;
        taskENTER_CRITICAL();
        tentative=hr_home_candidate(&home_candidate,&result,input->generation,input->sample.ms,input->breath,usable);
        if(generation==input->generation) {
            if(result.fresh && usable)
                hr_home_accept(&home_cache,generation,end,result.accepted_ms,(unsigned)result.bpm,1u,
                    result.quality,result.harmonic_conflict,result.target_bin);
            else if(tentative)
                hr_home_accept(&home_cache,generation,end,input->sample.ms,tentative,2u,1u,0u,result.target_bin);
        }
        taskEXIT_CRITICAL();
    }
    taskENTER_CRITICAL();value=published;taskEXIT_CRITICAL();
    value.computed_ms=input->sample.ms;value.cost_ms=end-start;
    if(value.cost_ms>value.max_cost_ms) value.max_cost_ms=value.cost_ms;
    value.bpm=result.bpm>=0?(uint16_t)result.bpm:UINT16_MAX;
    value.raw_bpm=result.bpm>=0?(uint16_t)result.bpm:0u;value.reason=result.reason;
    value.have_confirm=result.bpm>=0 && core.have_accepted;
    value.last_confirm_ms=result.accepted_ms;
    value.state=result.bpm>=0?(result.fresh?3u:4u):
        (result.reason==HR_WAIT_BED || result.reason==HR_WAIT_BREATH || result.reason==HR_QUIET_SHORT || result.reason==HR_LOCKING?1u:2u);
    value.age_ms=value.have_confirm?end-result.accepted_ms:UINT32_MAX;
    if(value.bpm!=UINT16_MAX && value.age_ms>10000u) {
        value.bpm=UINT16_MAX;value.state=5u;
    } else if(value.bpm!=UINT16_MAX && end-input->sample.ms>5000u) {
        value.bpm=UINT16_MAX;value.state=2u;value.reason=HR_STALE;
        value.have_confirm=0;value.age_ms=UINT32_MAX;
    }
    value.quality=result.quality;value.support=result.support;value.harmonic_conflict=result.harmonic_conflict;
    value.bin=result.target_bin>=0?(uint16_t)result.target_bin:UINT16_MAX;value.dropped=drop_count;
    value.stack_free_bytes=(uint32_t)uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t);
#if HR_ALIGNMENT_TRACE
    {
        uint32_t sequence=copy_published(&value,input->generation);
        record_estimate_trace(input,trace_attempt,start,end,sequence!=0u?1u:3u,&result,&value,sequence);
    }
#else
    copy_published(&value,input->generation);
#endif
}

static void worker(void *unused) {
    Input input;(void)unused;
    for(;;) {
        if(xQueueReceive(input_queue,&input,pdMS_TO_TICKS(100))==pdTRUE) process_input(&input);
        else if(worker_generation!=generation) { hr_core_init(&core);worker_generation=generation; }
    }
}

int heart_rate_device_init(void) {
    HeapStats_t heap;uint32_t need;
    if(input_queue!=NULL) return 0;
    if(hr_clock_init()!=0) {publish_empty(0u,HR_CLOCK_FAULT_REASON,now_ms(),generation);return -4;}
    heart_rate_device_reset();vPortGetHeapStats(&heap);
    need=HR_STACK_BYTES+HR_QUEUE_DEPTH*(uint32_t)sizeof(Input)+2048u+HR_HEAP_RESERVE;
    if(heap.xAvailableHeapSpaceInBytes<need || heap.xSizeOfLargestFreeBlockInBytes<need) {
        publish_empty(0u,HR_RESOURCE,now_ms(),generation);return -1;
    }
    input_queue=xQueueCreate(HR_QUEUE_DEPTH,sizeof(Input));
    if(input_queue==NULL) { publish_empty(0u,HR_RESOURCE,now_ms(),generation);return -2; }
    if(xTaskCreate(worker,"sleep_hr",HR_STACK_BYTES/sizeof(StackType_t),NULL,1u,&worker_handle)!=pdPASS) {
        vQueueDelete(input_queue);input_queue=NULL;publish_empty(0u,HR_RESOURCE,now_ms(),generation);return -3;
    }
    heart_rate_device_stop();return 0;
}

void heart_rate_device_submit(uint32_t frame,uint16_t bin,bool valid,bool active,
    bool in_bed,uint8_t breath,const int16_t iq[6]) {
    Input input;
    BaseType_t queued;
#if HR_ALIGNMENT_TRACE
    uint32_t received;
#endif
    if(input_queue==NULL || iq==NULL || !acquisition_active) return;
    now_ms();
    if(!hr_clock_ready()) {publish_empty(0u,HR_CLOCK_FAULT_REASON,now_ms(),generation);return;}
    taskENTER_CRITICAL();diagnostics.received++;taskEXIT_CRITICAL();
#if HR_ALIGNMENT_TRACE
    received=diagnostics.received;
#endif
    if(!in_bed && producer_was_in_bed) heart_rate_device_reset();
    producer_was_in_bed=in_bed;
    memset(&input,0,sizeof(input));input.sample.ms=now_ms();input.sample.frame=frame;input.sample.bin=bin;
    input.sample.flags=(valid?HR_VALID:0u)|(active?HR_ACTIVE:0u)|(in_bed?HR_IN_BED:0u);
    memcpy(input.sample.iq,iq,sizeof(input.sample.iq));input.breath=breath;input.generation=generation;
    queued=xQueueSend(input_queue,&input,0);
    if(queued!=pdTRUE) {
        uint32_t discarded=(uint32_t)uxQueueMessagesWaiting(input_queue)+1u;
        taskENTER_CRITICAL();drop_count+=discarded;diagnostics.overflows++;taskEXIT_CRITICAL();
        heart_rate_device_reset(); /* flush obsolete samples and cancel old work */
    } else {
        uint32_t depth=(uint32_t)uxQueueMessagesWaiting(input_queue);
        taskENTER_CRITICAL();if(depth>diagnostics.queue_peak) diagnostics.queue_peak=depth;taskEXIT_CRITICAL();
    }
#if HR_ALIGNMENT_TRACE
    /* Includes invalid/out-of-bed inputs absent from HRRAW. Timestamp, IQ,
     * flags and breath are the actual queued values, not a later readback. */
    printf("HRIN V=1 N=%lu GEN=%lu F=%lu MS=%lu BIN=%u FLAGS=%u BR=%u I0=%d Q0=%d I1=%d Q1=%d I2=%d Q2=%d ENQ=%u\r\n",
        (unsigned long)received,(unsigned long)input.generation,(unsigned long)input.sample.frame,
        (unsigned long)input.sample.ms,(unsigned int)input.sample.bin,(unsigned int)input.sample.flags,
        (unsigned int)input.breath,(int)input.sample.iq[0],(int)input.sample.iq[1],
        (int)input.sample.iq[2],(int)input.sample.iq[3],(int)input.sample.iq[4],(int)input.sample.iq[5],
        (unsigned int)(queued==pdTRUE));
#endif
}

void heart_rate_device_emit_pending(void) {
    HeartRateDeviceResult value;HeartRateDeviceDiagnostics info;uint32_t elapsed_ms,run_ms;
    bool changed;heart_rate_device_get(&value);heart_rate_device_get_diagnostics(&info);
#if HR_ALIGNMENT_TRACE
    emit_estimate_trace();
#endif
    elapsed_ms=value.report_ms-last_emit_ms;
    changed=value.sequence!=last_emitted || value.state!=last_emitted_state;
    if(!changed && elapsed_ms<(info.busy?1000u:5000u)) return;
    if(changed && value.state==last_emitted_state && elapsed_ms<1000u) return;
    last_emitted=value.sequence;last_emitted_state=value.state;
    last_emit_ms=value.report_ms;run_ms=info.busy?value.report_ms-info.last_start_ms:0u;
    printf("HRDEV VER=1 SEQ=%lu GEN=%lu MS=%lu CONF=%lu HR=%u RAW=%u STATE=%u Q=%u REASON=%u BIN=%u ANT=%u AGE=%lu COST=%lu MAX=%lu HEAP=%lu MINHEAP=%lu STACK=%lu DROP=%lu HCON=%u IN=%lu DONE=%lu EST=%lu COMPLETE=%lu ABORT=%lu BUDGET=%lu OVF=%lu QNOW=%lu QMAX=%lu BUSY=%u PHASE=%lu RUNMS=%lu SPECTRA=%lu LASTPHASE=%lu CHECKMS=%lu SETUPMS=%lu PREPMS=%lu WINMS=%lu DFTMS=%lu PEAKMS=%lu CONFMS=%lu YIELDMS=%lu\r\n",
        (unsigned long)value.sequence,(unsigned long)value.generation,(unsigned long)value.report_ms,
        (unsigned long)(value.have_confirm?value.last_confirm_ms:0u),
        value.bpm==UINT16_MAX?0u:(unsigned int)value.bpm,(unsigned int)value.raw_bpm,
        (unsigned int)value.state,(unsigned int)value.quality,(unsigned int)value.reason,
        (unsigned int)value.bin,(unsigned int)value.support,(unsigned long)value.age_ms,
        (unsigned long)value.cost_ms,(unsigned long)value.max_cost_ms,
        (unsigned long)xPortGetFreeHeapSize(),(unsigned long)xPortGetMinimumEverFreeHeapSize(),
        (unsigned long)value.stack_free_bytes,(unsigned long)value.dropped,(unsigned int)value.harmonic_conflict,
        (unsigned long)info.received,(unsigned long)info.processed,(unsigned long)info.estimates,
        (unsigned long)info.completed,(unsigned long)info.aborted,(unsigned long)info.budget_aborts,
        (unsigned long)info.overflows,(unsigned long)(input_queue!=NULL?uxQueueMessagesWaiting(input_queue):0u),
        (unsigned long)info.queue_peak,(unsigned int)info.busy,(unsigned long)info.phase,(unsigned long)run_ms,
        (unsigned long)info.spectral_estimates,(unsigned long)info.last_phase,
        (unsigned long)info.phase_ms[HR_PHASE_CHECK],(unsigned long)info.phase_ms[HR_PHASE_SPECTRUM],
        (unsigned long)info.phase_ms[HR_PHASE_PREPROCESS],
        (unsigned long)info.phase_ms[HR_PHASE_WINDOW],(unsigned long)info.phase_ms[HR_PHASE_DFT],
        (unsigned long)info.phase_ms[HR_PHASE_PEAKS],(unsigned long)info.phase_ms[HR_PHASE_CONSENSUS],
        (unsigned long)info.yield_ms);
    {
        HeartRateDisplay show;heart_rate_device_get_display(&show);
        printf("HRSHOW V=1 SEQ=%lu GEN=%lu MS=%lu CONF=%lu BPM=%u STATE=%u KIND=%u AGE=%lu Q=%u HCON=%u\r\n",
            (unsigned long)show.sequence,(unsigned long)show.generation,(unsigned long)show.report_ms,
            (unsigned long)show.confirmed_ms,(unsigned)show.bpm,(unsigned)show.state,(unsigned)show.kind,
            (unsigned long)show.age_ms,(unsigned)show.quality,(unsigned)show.harmonic);
    }
}
uint32_t heart_rate_device_core_bytes(void) { return (uint32_t)sizeof(core); }

#ifdef HR_DEVICE_TEST
void heart_rate_device_test_drain(void) {
    Input input;
    while(xQueueReceive(input_queue,&input,0)==pdTRUE) process_input(&input);
}
#endif
#else
int heart_rate_device_init(void) { return -1; }
void heart_rate_device_reset(void) { }
void heart_rate_device_start(void) { }
void heart_rate_device_stop(void) { }
void heart_rate_device_submit(uint32_t f,uint16_t b,bool v,bool a,bool bed,uint8_t br,const int16_t iq[6]) {
    (void)f;(void)b;(void)v;(void)a;(void)bed;(void)br;(void)iq;
}
void heart_rate_device_get(HeartRateDeviceResult *r) {
    if(r!=NULL) { memset(r,0,sizeof(*r));r->bpm=UINT16_MAX;r->age_ms=UINT32_MAX; }
}
void heart_rate_device_get_display(HeartRateDisplay *r) {
    if(r!=NULL) {memset(r,0,sizeof(*r));r->age_ms=UINT32_MAX;}
}
void heart_rate_device_get_diagnostics(HeartRateDeviceDiagnostics *r) {
    if(r!=NULL) memset(r,0,sizeof(*r));
}
void heart_rate_device_emit_pending(void) { }
uint32_t heart_rate_device_core_bytes(void) { return 0; }
#endif
